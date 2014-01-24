/*
===============================================================================
                            COPYRIGHT NOTICE

    Copyright (C) 2002-2005 GE Fanuc
    International Copyright Secured.  All Rights Reserved.

-------------------------------------------------------------------------------

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

   o Redistributions of source code must retain the above copyright notice, this
     list of conditions and the following disclaimer.
   o Redistributions in binary form must reproduce the above copyright notice,
     this list of conditions and the following disclaimer in the documentation
     and/or other materials provided with the distribution.
   o Neither the name of GE Fanuc nor the names of its contributors may be used
     to endorse or promote products derived from this software without specific
     prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

===============================================================================
*/


#include <linux/config.h>
#ifdef CONFIG_SMP
#define __SMP__
#endif

#include <asm/uaccess.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/poll.h>
#include <linux/hwtimer.h>
#include <linux/kernel.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)
#include <linux/interrupt.h>
#endif
#include "vmitmr_82c54.h"


#ifdef DEBUG
#define DPRINTF(x...)  printk(KERN_DEBUG x)
#else
#define DPRINTF(x...)
#endif

 /* Make an extra define for interrupt debugging because it's way to noisy!
  */
#ifdef DEBUG_ISR
#define DPRINTF_ISR(x...)  printk(KERN_DEBUG x)
#else
#define DPRINTF_ISR(x...)
#endif


MODULE_AUTHOR("GE Fanuc <www.gefanuc.com/embedded>");
MODULE_DESCRIPTION("GE Fanuc Timer Driver");
#ifdef MODULE_LICENSE
MODULE_LICENSE("Dual BSD/GPL");
#endif


int tmr_start(int timer);
EXPORT_SYMBOL(tmr_start);
int tmr_stop(int timer);
EXPORT_SYMBOL(tmr_stop);
int tmr_set_count(int timer, uint16_t count);
EXPORT_SYMBOL(tmr_set_count);
int tmr_get_count(int timer, uint16_t * count);
EXPORT_SYMBOL(tmr_get_count);
int tmr_interrupt_enable(int timer);
EXPORT_SYMBOL(tmr_interrupt_enable);
int tmr_interrupt_disable(int timer);
EXPORT_SYMBOL(tmr_interrupt_disable);
int tmr_interrupt_wait(int timer);
EXPORT_SYMBOL(tmr_interrupt_wait);


static int vmitmr_open(struct inode *inode, struct file *file_ptr);
static int vmitmr_close(struct inode *inode, struct file *file_ptr);
static ssize_t vmitmr_read(struct file *file_ptr, char *buffer,
                           size_t nbytes, loff_t * off);
static ssize_t vmitmr_write(struct file *file_ptr, const char *buffer,
                            size_t nbytes, loff_t * off);
static unsigned int vmitmr_poll(struct file *file_ptr, poll_table * wait);
static int vmitmr_ioctl(struct inode *inode, struct file *file_ptr,
                        unsigned int cmd, unsigned long arg);
static struct file_operations file_ops = {
        .owner = THIS_MODULE,
        .read = vmitmr_read,
        .write = vmitmr_write,
        .poll = vmitmr_poll,
        .ioctl = vmitmr_ioctl,
        .open = vmitmr_open,
        .release = vmitmr_close
};

struct timer_data {
        int running;
        uint16_t load_count;
        int interrupt_canceled;
        volatile int interrupt_pending;
        wait_queue_head_t wq;
};

static struct timer_data vmitmr_device[VMITMR_NUM_DEVICES + 1];
static volatile unsigned long vmitmr_base = 0x500;
static volatile unsigned long pwrmgt_base = 0;
static int vmitmr_major = MOD_MAJOR;
static spinlock_t vmitmr_lock = SPIN_LOCK_UNLOCKED;


/*============================================================================
 * Is this a valid timer?
 */
int inline tmr_not_valid(int timer)
{
        return ((1 > timer) || (VMITMR_NUM_DEVICES < timer));
}


/*============================================================================
 * Enable the timer interrupt.
 */
static inline void __tmr_interrupt_enable(int timer)
{
        uint8_t intclr;

        intclr = inb(pwrmgt_base + PWRMGT_INTCLR);
        outb(intclr | PWRMGT_INTCLR__TMR(timer), pwrmgt_base + PWRMGT_INTCLR);
}


/*============================================================================
 * Disable the timer interrupt
 */
static inline void __tmr_interrupt_disable(int timer)
{
        uint8_t intclr;

        intclr = inb(pwrmgt_base + PWRMGT_INTCLR);
        outb(intclr & ~PWRMGT_INTCLR__TMR(timer), pwrmgt_base + PWRMGT_INTCLR);
}


/*============================================================================
 * Start the timer countdown
 */
static inline void __tmr_start(int timer)
{
        struct timer_data *device = &vmitmr_device[timer];
        uint8_t intclr;

        intclr = inb(pwrmgt_base + PWRMGT_INTCLR);

        outb(VMITMR_CTL__ST__TMR(timer) | VMITMR_CTL__RW__LSBMSB |
             VMITMR_CTL__MODE2, vmitmr_base + VMITMR_CTL);
        outb(device->load_count & 0xff, vmitmr_base + VMITMR_TIMER(timer));
        outb(device->load_count >> 8, vmitmr_base + VMITMR_TIMER(timer));

        /* This device is really screwy. Interrupts get disabled when we
           program the count, so we keep track of the interrupt enable status
           and re-enable interrupts if necessary.
         */
        outb(intclr, pwrmgt_base + PWRMGT_INTCLR);
}


/*============================================================================
 * Start the timer countdown
 */
int tmr_start(int timer)
{
        struct timer_data *device = &vmitmr_device[timer];
        unsigned long irqflags;

        if (tmr_not_valid(timer))
                return -ENODEV;

        /* For purposes of this function, I'm assuming start means start with
           the loaded count regardless of whether the timer was already running
           or not.
         */
        spin_lock_irqsave(&vmitmr_lock, irqflags);
        __tmr_start(timer);
        device->running = 1;
        spin_unlock_irqrestore(&vmitmr_lock, irqflags);

        return 0;
}


/*============================================================================
 * Stop the timer countdown
 */
int tmr_stop(int timer)
{
        struct timer_data *device = &vmitmr_device[timer];
        unsigned long irqflags;

        if (tmr_not_valid(timer))
                return -ENODEV;

        spin_lock_irqsave(&vmitmr_lock, irqflags);

        /* Putting the timer in a write (load count) mode halts the timer
         */
        outb(VMITMR_CTL__ST__TMR(timer) | VMITMR_CTL__RW__LSBMSB |
             VMITMR_CTL__MODE2, vmitmr_base + VMITMR_CTL);

        device->running = 0;

        spin_unlock_irqrestore(&vmitmr_lock, irqflags);

        return 0;
}


/*============================================================================
 * Set the timer count
 */
int tmr_set_count(int timer, uint16_t count)
{
        struct timer_data *device = &vmitmr_device[timer];
        unsigned long irqflags;

        if (tmr_not_valid(timer))
                return -ENODEV;

        if (3 > count)
                return -EINVAL;

        spin_lock_irqsave(&vmitmr_lock, irqflags);

        device->load_count = count;

        /* If the timer was already running, restart it with the new count
         */
        if (device->running)
                __tmr_start(timer);

        spin_unlock_irqrestore(&vmitmr_lock, irqflags);

        return 0;
}


/*============================================================================
 * Get the current timer count
 */
int tmr_get_count(int timer, uint16_t * count)
{
        unsigned long irqflags;

        if (tmr_not_valid(timer))
                return -ENODEV;

        spin_lock_irqsave(&vmitmr_lock, irqflags);

        outb(VMITMR_CTL__ST__TMR(timer) | VMITMR_CTL__RW__LATCH |
             VMITMR_CTL__MODE2, vmitmr_base + VMITMR_CTL);
        *count = inb(vmitmr_base + VMITMR_TIMER(timer));
        *count |= inb(vmitmr_base + VMITMR_TIMER(timer)) << 8;

        spin_unlock_irqrestore(&vmitmr_lock, irqflags);

        return 0;
}


/*============================================================================
 * Enable the timer interrupt
 */
int tmr_interrupt_enable(int timer)
{
        struct timer_data *device = &vmitmr_device[timer];
        unsigned long irqflags;

        if (tmr_not_valid(timer))
                return -ENODEV;

        spin_lock_irqsave(&vmitmr_lock, irqflags);

        device->interrupt_canceled = 0;

        __tmr_interrupt_enable(timer);

        DPRINTF(MOD_NAME ": Timer %d interrupt enabled\n", timer);

        spin_unlock_irqrestore(&vmitmr_lock, irqflags);

        return 0;
}


/*============================================================================
 * Disable the timer interrupt
 */
int tmr_interrupt_disable(int timer)
{
        struct timer_data *device = &vmitmr_device[timer];
        unsigned long irqflags;

        if (tmr_not_valid(timer))
                return -ENODEV;

        spin_lock_irqsave(&vmitmr_lock, irqflags);

        __tmr_interrupt_disable(timer);

        DPRINTF(MOD_NAME ": Timer %d interrupt disabled\n", timer);

        /* Wake up any processes waiting on this interrupt
         */
        device->interrupt_canceled = 1;
        wake_up_interruptible_all(&device->wq);

        spin_unlock_irqrestore(&vmitmr_lock, irqflags);

        return 0;
}


/*============================================================================
 * Wait for a timer interrupt
 */
int tmr_interrupt_wait(int timer)
{
        struct timer_data *device = &vmitmr_device[timer];
        unsigned long irqflags;

        if (tmr_not_valid(timer))
                return -ENODEV;

        DPRINTF_ISR(MOD_NAME ": Waiting for timer %d interrupt\n", timer);

        while (1) {
                interruptible_sleep_on(&device->wq);

                spin_lock_irqsave(&vmitmr_lock, irqflags);

                if (device->interrupt_pending) {
                        --device->interrupt_pending;
                        spin_unlock_irqrestore(&vmitmr_lock, irqflags);
                        return 0;
                }

                spin_unlock_irqrestore(&vmitmr_lock, irqflags);
        }

        return 0;
}


/*============================================================================
 * Hook for the read file operation
 */
static ssize_t vmitmr_read(struct file *file_ptr, char *buffer, size_t nbytes,
                           loff_t * off)
{
        int timer = (int) file_ptr->private_data;
        uint32_t count;
        uint16_t count1;

        tmr_get_count(timer, &count1);

	count = (uint32_t)count1;

	DPRINTF("Read timer %d count 0x%x\n", device->timer, count);

        /* Hack to try to match integer data to size of the requested read.
           I'm assuming that the user would call read with code like this:
           int count;
           read(fd, &count, sizeof(count));
           if they request a read whose size does not match something we can
           convert to an integer value, then we return an error.
         */
/*
        switch (nbytes) {
        case 1:
                *(uint8_t *) buffer = count & 0xff;
                break;
        case 2:
                *(uint16_t *) buffer = count;
                break;
        case 4:
                *(uint32_t *) buffer = count;
                break;
        case 8:
                *(uint64_t *) buffer = (uint64_t) count;
                break;
        default:
                return -EINVAL;
        }
*/
	copy_to_user( buffer, &count, nbytes);

        return nbytes;
}


/*============================================================================
 * Hook for the write file operation
 */
static ssize_t vmitmr_write(struct file *file_ptr, const char *buffer,
                            size_t nbytes, loff_t * off)
{
        int timer = (int) file_ptr->private_data;
        uint16_t count=0;

	DPRINTF("About to write timer %d\n", device->timer);

	copy_from_user(&count, buffer, nbytes);

        /* Hack to try to match integer data to size of the requested write.
           I'm assuming that the user would call write with code like this:
           int count = 1000;
           write(fd, &count, sizeof(count));
           if they request a write whose size does not match something we can
           convert to an integer value, then we return an error.
         */
  /*
      switch (nbytes) {
        case 1:
                count = *(uint8_t *) buffer;
                break;
        case 2:
                count = *(uint16_t *) buffer;
                break;
        case 4:
                count = *(uint32_t *) buffer & 0xffff;
                break;
        case 8:
                count = *(uint64_t *) buffer & 0xffff;
                break;
        default:
                return -EINVAL;
        }
*/

	DPRINTF("Writing timer %d count 0x%x\n", device->timer, count);

        tmr_set_count(timer, count);

        return nbytes;
}


/*============================================================================
 * Hook for the poll/select file operation
 */
static unsigned int vmitmr_poll(struct file *file_ptr, poll_table * wait)
{
        int timer = (int) file_ptr->private_data;
        struct timer_data *device = &vmitmr_device[timer];
        int mask = POLLIN | POLLRDNORM | POLLOUT | POLLWRNORM;
        unsigned long irqflags;

        poll_wait(file_ptr, &device->wq, wait);

        spin_lock_irqsave(&vmitmr_lock, irqflags);
        if (device->interrupt_pending) {
#if 0
                device->interrupt_pending = 0;
#endif
                --device->interrupt_pending;
                mask |= POLLPRI;
        }
        spin_unlock_irqrestore(&vmitmr_lock, irqflags);

        /* We can always read or write
         */
        return mask;
}


/*============================================================================
 * Hook to the ioctl file operation
 */
static int vmitmr_ioctl(struct inode *inode, struct file *file_ptr,
                        uint32_t cmd, unsigned long arg)
{
#if LINUX_VERSION_CODE<=KERNEL_VERSION(2,5,0)
        int timer = MINOR(inode->i_rdev);
#else
        int timer = iminor(inode);
#endif

        int rval;

        switch (cmd) {
        case TIMER_START:
                if (0 > (rval = tmr_start(timer)))
                        return rval;
                break;
        case TIMER_STOP:
                if (0 > (rval = tmr_stop(timer)))
                        return rval;
                break;
        case TIMER_INTERRUPT_ENABLE:
                if (0 > (rval = tmr_interrupt_enable(timer)))
                        return rval;
                break;
        case TIMER_INTERRUPT_DISABLE:
                if (0 > (rval = tmr_interrupt_disable(timer)))
                        return rval;
                break;
        case TIMER_INTERRUPT_WAIT:
                if (0 > (rval = tmr_interrupt_wait(timer)))
                        return rval;
                break;
        default:
                return -ENOTTY;
        }

        return 0;
}


/*============================================================================
 * Hook for the open file operation
 */
static int vmitmr_open(struct inode *inode, struct file *file_ptr)
{

#if LINUX_VERSION_CODE<=KERNEL_VERSION(2,5,0)
        int timer = MINOR(inode->i_rdev);
#else
        int timer = iminor(inode);
#endif
	
        if (tmr_not_valid(timer))
                return -ENODEV;

        file_ptr->private_data = (void *) timer;

#if LINUX_VERSION_CODE<=KERNEL_VERSION(2,5,0)
        MOD_INC_USE_COUNT;
#endif
        return 0;
}


/*============================================================================
 * Hook for the close file operation
 */
static int vmitmr_close(struct inode *inode, struct file *file_ptr)
{
#if LINUX_VERSION_CODE <KERNEL_VERSION(2,5,0)
        MOD_DEC_USE_COUNT;
#endif
        return 0;
}


/*===========================================================================
 * Handle interrupts on a particular timer
 */
static inline void __tmr_handle_interrupt(int timer)
{
        struct timer_data *device = &vmitmr_device[timer];

#if 0
        DPRINTF_ISR(MOD_NAME ": timer %d interrupt\n", timer);
#endif

        /* Wake all processes waiting for this interrupt
         */
        ++device->interrupt_pending;
        wake_up_interruptible_all(&device->wq);
}


/*===========================================================================
 * Interrupt service routine
*/
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0) 
static int vmitmr_isr(int irq, void *dev_id, struct pt_regs *regs)
#else
static void vmitmr_isr(int irq, void *dev_id, struct pt_regs *regs)
#endif
{
        unsigned long irqflags;
        uint8_t intclr, intmask, intstat, stat;
        int timer;
	int handled = 0;

        spin_lock_irqsave(&vmitmr_lock, irqflags);

        intclr = inb(pwrmgt_base + PWRMGT_INTCLR);

        while ((stat = inb(pwrmgt_base + PWRMGT_INTSTAT) & 0xe0)) 
          {
            intmask = 0;
            intstat = 0;

            for (timer = 1; stat & 0xe0; stat <<= 1, ++timer) 
              {
                if (0x80 & stat) 
                  {
                     __tmr_handle_interrupt(timer);
                     intmask |= PWRMGT_INTCLR__TMR(timer);
                     intstat |= PWRMGT_INTSTAT__TMR(timer);
		     handled = 1;
                  }
              }

            /* Clear all active interrupts
             */
            outb(intclr & ~intmask, pwrmgt_base + PWRMGT_INTCLR);

            /* Loop here until we know the interrupts have been cleared
             */
            while (inb(pwrmgt_base + PWRMGT_INTSTAT) & intstat) ;

            /* Re-enable the cleared interrupts
             */
            outb(intclr, pwrmgt_base + PWRMGT_INTCLR);
          }

        spin_unlock_irqrestore(&vmitmr_lock, irqflags);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0) 
	return(IRQ_RETVAL(handled));
#endif
}


/*===========================================================================
 * Module initialization routine
 */
static int __init vmitmr_init(void)
{
        struct pci_dev *pwrmgt_pci_dev;
        int timer, rval;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
        if (!pci_present()) {
                printk(KERN_ERR MOD_NAME ": PCI bios not found\n");
                return -ENODEV;
        }
#endif

        pwrmgt_pci_dev = pci_find_device(PCI_VENDOR_ID_INTEL,
                                         PCI_DEVICE_ID_INTEL_82371AB_3, NULL);
        if (NULL == pwrmgt_pci_dev) {
                printk(KERN_ERR MOD_NAME ": PIIX4 device not found\n");
                return -ENODEV;
        }

        pci_read_config_dword(pwrmgt_pci_dev, 0x40, (u32 *) & pwrmgt_base);
        pwrmgt_base &= PCI_BASE_ADDRESS_IO_MASK;
        DPRINTF(MOD_NAME ": Power management base found at 0x%lx\n",
                pwrmgt_base);

        if (NULL == request_region(vmitmr_base, 4, MOD_NAME)) {
                printk(KERN_ERR MOD_NAME
                       ": Error requesting 4 bytes at address 0x%lx\n",
                       vmitmr_base);
                return -EBUSY;
        }

        /* Initialize all timers
         */
        for (timer = 1; VMITMR_NUM_DEVICES >= timer; ++timer) {
                init_waitqueue_head(&vmitmr_device[timer].wq);
                tmr_interrupt_disable(timer);
                tmr_stop(timer);
                tmr_set_count(timer, 0xffff);
                vmitmr_device[timer].interrupt_pending = 0;
                vmitmr_device[timer].running = 0;
        }

        if (request_irq(5, vmitmr_isr, SA_INTERRUPT | SA_SHIRQ, MOD_NAME,
                        &file_ops)) {
                printk(KERN_ERR MOD_NAME ": Failure requesting irq 5\n");
                rval = -EIO;
                goto err_request_irq;
        }

        if (0 > (rval = register_chrdev(vmitmr_major, MOD_NAME, &file_ops))) {
                printk(KERN_ERR MOD_NAME ": Failed registering device\n");
                rval = -EIO;
                goto err_register_chrdev;
        }

        /* If we used dynamic major device number allocation, store the major
           number. We need it to unregister the driver.
         */
        if (!vmitmr_major)
                vmitmr_major = rval;

        printk(KERN_NOTICE MOD_NAME
               ": Installed VMIC timer module version: %s\n", MOD_VERSION);

        return 0;

      err_register_chrdev:
        free_irq(5, &file_ops);

      err_request_irq:
        release_region(vmitmr_base, 4);

        return rval;
}


/*===========================================================================
 * Module exit routine
 */
static void __exit vmitmr_exit(void)
{
        int timer;

        unregister_chrdev(vmitmr_major, MOD_NAME);

        for (timer = 1; VMITMR_NUM_DEVICES <= timer; ++timer) {
                tmr_interrupt_disable(timer);
                tmr_stop(timer);
        }

        free_irq(5, &file_ops);

        release_region(vmitmr_base, 4);

        printk(KERN_NOTICE MOD_NAME ": Exiting %s module version: %s\n",
               MOD_NAME, MOD_VERSION);
}


module_init(vmitmr_init);
module_exit(vmitmr_exit);
