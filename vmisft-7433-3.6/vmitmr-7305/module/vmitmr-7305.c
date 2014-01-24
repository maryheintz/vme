/*
===============================================================================
                            COPYRIGHT NOTICE

    Copyright (C) 2003-2005 GE Fanuc
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

#include <asm/io.h>
#include <asm/uaccess.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/module.h>
#include <linux/poll.h>
#include <linux/hwtimer.h>
#include <linux/kernel.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)
#include <linux/interrupt.h>
#endif
#include "vmitmr-7305.h"

#ifdef DEBUG
#define DPRINTF(x...)  printk(KERN_DEBUG x)
#else
#define DPRINTF(x...)
#endif

 /* Make an extra define for interrupt debugging because it's way to noisy! */
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
int tmr_set_count(int timer, uint32_t count);
EXPORT_SYMBOL(tmr_set_count);
int tmr_get_count(int timer, uint32_t * count);
EXPORT_SYMBOL(tmr_get_count);
int tmr_set_rate(int timer, int rate);
EXPORT_SYMBOL(tmr_set_rate);
int tmr_get_rate(int timer, int *rate);
EXPORT_SYMBOL(tmr_get_rate);
void tmr_latch_sync_enable(void);
EXPORT_SYMBOL(tmr_latch_sync_enable);
void tmr_latch_sync_disable(void);
EXPORT_SYMBOL(tmr_latch_sync_disable);
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
        int timer;
        int cs;
        int interrupt_pending;
        wait_queue_head_t wq;
};

static volatile unsigned long vmitmr_base;
static int vmitmr_major;
static spinlock_t vmitmr_lock = SPIN_LOCK_UNLOCKED;
static struct timer_data vmitmr_device[VMITMR_NUM_DEVICES + 1];


/*============================================================================
 * Is this a valid timer?
 */
int inline tmr_not_valid(int timer)
{
        return ((1 > timer) || (VMITMR_NUM_DEVICES < timer));
}


/*============================================================================
 * Start the timer countdown
 */
int tmr_start(int timer)
{
        uint8_t csr;
        unsigned long irqflags;

        if (tmr_not_valid(timer))
                return -ENODEV;

        spin_lock_irqsave(&vmitmr_lock, irqflags);

        csr = isa_readb(vmitmr_base + VMITMR_ENA);
        isa_writeb(csr | VMITMR_ENA__EN(timer), vmitmr_base + VMITMR_ENA);

        spin_unlock_irqrestore(&vmitmr_lock, irqflags);

        return 0;
}


/*============================================================================
 * Stop the timer countdown
 */
int tmr_stop(int timer)
{
        uint8_t csr;
        unsigned long irqflags;

        if (tmr_not_valid(timer))
                return -ENODEV;

        spin_lock_irqsave(&vmitmr_lock, irqflags);

        csr = isa_readb(vmitmr_base + VMITMR_ENA);
        isa_writeb(csr & ~VMITMR_ENA__EN(timer), vmitmr_base + VMITMR_ENA);

        spin_unlock_irqrestore(&vmitmr_lock, irqflags);

        return 0;
}


/*============================================================================
 * Set the timer count
 */
int tmr_set_count(int timer, uint32_t count)
{
        if (tmr_not_valid(timer))
                return -ENODEV;

        isa_writew(count - 1, vmitmr_base + VMITMR_LCR(timer));

        return 0;
}


/*============================================================================
 * Get the current timer count
 */
int tmr_get_count(int timer, uint32_t * count)
{
        if (tmr_not_valid(timer))
                return -ENODEV;

        *count = isa_readw(vmitmr_base + VMITMR_CCR(timer));

        return 0;
}


/*============================================================================
 * Set the clock rate of a timer
 */
int tmr_set_rate(int timer, int rate)
{
        uint8_t csr, cs;
        unsigned long irqflags;
        struct timer_data *device = &vmitmr_device[timer];

        if (tmr_not_valid(timer))
                return -ENODEV;

        switch (rate) {
        case 2000:
                cs = VMITMR_CSR__CS__2mhz;
                break;
        case 1000:
                cs = VMITMR_CSR__CS__1mhz;
                break;
        case 500:
                cs = VMITMR_CSR__CS__500khz;
                break;
        case 250:
                cs = VMITMR_CSR__CS__250khz;
                break;
        default:
                return -EINVAL;
        }

        spin_lock_irqsave(&vmitmr_lock, irqflags);

        csr = isa_readb(vmitmr_base + VMITMR_CSR(timer));
        isa_writeb((csr & ~VMITMR_CSR__CS) | cs,
                   vmitmr_base + VMITMR_CSR(timer));
        device->cs = cs;

        spin_unlock_irqrestore(&vmitmr_lock, irqflags);

        return 0;
}


/*============================================================================
 * Get the clock rate of a timer
 */
int tmr_get_rate(int timer, int *rate)
{
        uint8_t cs;

        if (tmr_not_valid(timer))
                return -ENODEV;

        cs = isa_readb(vmitmr_base + VMITMR_CSR(timer)) & VMITMR_CSR__CS;

        switch (cs) {
        case VMITMR_CSR__CS__2mhz:
                *rate = 2000;
                break;
        case VMITMR_CSR__CS__1mhz:
                *rate = 1000;
                break;
        case VMITMR_CSR__CS__500khz:
                *rate = 500;
                break;
        case VMITMR_CSR__CS__250khz:
                *rate = 250;
                break;
        default:
                return -EINVAL;
        }

        return 0;
}


/*============================================================================
 * With latch enabled, a read to timer 1 causes timer's 2 and 3 to latch their
 * values.
 */
void tmr_latch_sync_enable(void)
{
        uint8_t csr;
        unsigned long irqflags;

        spin_lock_irqsave(&vmitmr_lock, irqflags);

        csr = isa_readb(vmitmr_base + VMITMR_ENA);
        isa_writeb(csr | VMITMR_ENA__LATCH, vmitmr_base + VMITMR_ENA);

        spin_unlock_irqrestore(&vmitmr_lock, irqflags);
}


/*============================================================================
 * Disable the timer latch syncronization
 */
void tmr_latch_sync_disable(void)
{
        uint8_t csr;
        unsigned long irqflags;

        spin_lock_irqsave(&vmitmr_lock, irqflags);

        csr = isa_readb(vmitmr_base + VMITMR_ENA);
        isa_writeb(csr & ~VMITMR_ENA__LATCH, vmitmr_base + VMITMR_ENA);

        spin_unlock_irqrestore(&vmitmr_lock, irqflags);
}


/*============================================================================
 * Clear the timer interrupt. We continuously clear the interrupt in a loop
 * until we see it's status cleared in the CSR.
 * NOTE: This function assumes all error checking is completed, and any
 * necessary locks have been acquired.
 */
static inline void __tmr_interrupt_clear(int timer)
{
        while (VMITMR_CSR__IRQSTAT & isa_readb(vmitmr_base + VMITMR_CSR(timer))) {
                isa_writeb(0, vmitmr_base + VMITMR_IRQCLR(timer));
        }
}


/*============================================================================
 * Enable the timer interrupt.
 * NOTE: This function assumes all error checking is completed, and any
 * necessary locks have been acquired.
 */
static inline void __tmr_interrupt_enable(int timer)
{
        uint8_t csr;

        __tmr_interrupt_clear(timer);
        vmitmr_device[timer].interrupt_pending = 0;
        csr = isa_readb(vmitmr_base + VMITMR_CSR(timer));
        isa_writeb(csr | VMITMR_CSR__IRQEN, vmitmr_base + VMITMR_CSR(timer));
}


/*============================================================================
 * Enable the timer interrupt
 */
int tmr_interrupt_enable(int timer)
{
        unsigned long irqflags;

        if (tmr_not_valid(timer))
                return -ENODEV;

        spin_lock_irqsave(&vmitmr_lock, irqflags);

        __tmr_interrupt_enable(timer);

        spin_unlock_irqrestore(&vmitmr_lock, irqflags);

        return 0;
}


/*============================================================================
 * Disable the timer interrupt.
 * NOTE: This function assumes all error checking is completed, and any
 * necessary locks have been acquired.
 */
static inline void __tmr_interrupt_disable(int timer)
{
        uint8_t csr;

        csr = isa_readb(vmitmr_base + VMITMR_CSR(timer));
        isa_writeb(csr & ~VMITMR_CSR__IRQEN, vmitmr_base + VMITMR_CSR(timer));
        __tmr_interrupt_clear(timer);
        vmitmr_device[timer].interrupt_pending = 0;
}


/*============================================================================
 * Disable the timer interrupt
 */
int tmr_interrupt_disable(int timer)
{
        unsigned long irqflags;

        if (tmr_not_valid(timer))
                return -ENODEV;

        spin_lock_irqsave(&vmitmr_lock, irqflags);
        __tmr_interrupt_disable(timer);
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
        struct timer_data *device = file_ptr->private_data;
        uint32_t count;

        tmr_get_count(device->timer, &count);

	DPRINTF("Read timer %d count 0x%x\n", device->timer, count);

        /* Hack to try to match integer data to size of the requested read.
           I'm assuming that the user would call read with code like this:
           int count;
           read(fd, &count, sizeof(count));
           if they request a read whose size does not match something we can
           convert to an integer value, then we return an error. */
/*
        switch (nbytes) {
        case 1:
                *(uint8_t *) buffer = count & 0xff;
                break;
        case 2:
                *(uint16_t *) buffer = count & 0xffff;
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
	copy_to_user(buffer, &count, nbytes);

        return nbytes;
}


/*============================================================================
 * Hook for the write file operation
 */
static ssize_t vmitmr_write(struct file *file_ptr, const char *buffer,
                            size_t nbytes, loff_t * off)
{
        struct timer_data *device = file_ptr->private_data;
        uint32_t count=0;

	DPRINTF("1 About to write timer %d\n", device->timer);

	copy_from_user(&count, buffer, nbytes);

        /* Hack to try to match integer data to size of the requested write.
           I'm assuming that the user would call write with code like this:
           int count = 1000;
           write(fd, &count, sizeof(count));
           if they request a write whose size does not match something we can
           convert to an integer value, then we return an error. */
/*
        switch (nbytes) {
        case 1:
                count = *(uint8_t *) buffer;
                break;
        case 2:
                count = *(uint16_t *) buffer;
                break;
        case 4:
                count = *(uint32_t *) buffer;
                break;
        case 8:
                count = *(uint64_t *) buffer;
                break;
        default:
                return -EINVAL;
        }
*/
	DPRINTF("Writing timer %d 0x%x\n", device->timer, count);

        tmr_set_count(device->timer, count);

        return nbytes;
}


/*============================================================================
 * Hook for the poll/select file operation
 */
static unsigned int vmitmr_poll(struct file *file_ptr, poll_table * wait)
{
        struct timer_data *device = file_ptr->private_data;
        int mask = POLLIN | POLLRDNORM | POLLOUT | POLLWRNORM;
        unsigned long irqflags;

        poll_wait(file_ptr, &device->wq, wait);

        spin_lock_irqsave(&vmitmr_lock, irqflags);

        if (device->interrupt_pending) {
                --device->interrupt_pending;
#if 0
                device->interrupt_pending = 0;
#endif
                mask |= POLLPRI;
        }

        spin_unlock_irqrestore(&vmitmr_lock, irqflags);

        return mask;
}


/*============================================================================
 * Hook to the ioctl file operation
 */
static int vmitmr_ioctl(struct inode *inode, struct file *file_ptr,
                        uint32_t cmd, unsigned long arg)
{
        struct timer_data *device = file_ptr->private_data;
        int rate, rval;

        switch (cmd) {
        case TIMER_START:
                return tmr_start(device->timer);
                break;
        case TIMER_STOP:
                return tmr_stop(device->timer);
                break;
        case TIMER_RATE_SET:
                if (0 > copy_from_user(&rate, (void *) arg, sizeof (int)))
                        return -EFAULT;

                return tmr_set_rate(device->timer, rate);
                break;
        case TIMER_RATE_GET:
                rval = tmr_get_rate(device->timer, &rate);
                if (0 > rval)
                        return rval;

                if (0 > copy_to_user((void *) arg, &rate, sizeof (rate)))
                        return -EFAULT;
                break;
        case TIMER_LATCH_SYNC_ENABLE:
                tmr_latch_sync_enable();
                break;
        case TIMER_LATCH_SYNC_DISABLE:
                tmr_latch_sync_disable();
                break;
        case TIMER_INTERRUPT_ENABLE:
                return tmr_interrupt_enable(device->timer);
                break;
        case TIMER_INTERRUPT_DISABLE:
                return tmr_interrupt_disable(device->timer);
                break;
        case TIMER_INTERRUPT_WAIT:
                return tmr_interrupt_wait(device->timer);
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
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
        int timer = MINOR(inode->i_rdev);
#else
        int timer = iminor(inode);
#endif

	if (tmr_not_valid(timer))
                return -ENODEV;

        file_ptr->private_data = (void *) &vmitmr_device[timer];

#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,5,0)
        MOD_INC_USE_COUNT;
#endif
        return 0;
}


/*============================================================================
 * Hook for the close file operation
 */
static int vmitmr_close(struct inode *inode, struct file *file_ptr)
{
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,5,0)
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

        isa_writeb(VMITMR_CSR__IRQEN | device->cs,
                   vmitmr_base + VMITMR_CSR(timer));
        __tmr_interrupt_clear(timer);

        ++device->interrupt_pending;
        wake_up_interruptible_all(&device->wq);
}


/*===========================================================================
 * Interrupt service routine. Check each timer to see if it generated an
 * interrupt and process it. Continue to process interrupts until all pending
 * are cleared.
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)
int vmitmr_isr(int irq, void *dev_id, struct pt_regs *regs)
#else
static void vmitmr_isr(int irq, void *dev_id, struct pt_regs *regs)
#endif
{
        uint32_t csr, stat;
        int timer;
        unsigned long irqflags;
	int handled = 0;

        spin_lock_irqsave(&vmitmr_lock, irqflags);

        /* Read the interrupt status and enable bits from the CSR.
           Then mask the enable bits with the status bits. Determine which
           timer generated the interrupt and process. Repeat.... until all
           pending interrupts are cleared. */
        while ((csr = isa_readl(vmitmr_base + VMITMR_CSR(1)))
               && (stat = csr & 0x00808080)) 
	  {
            for (timer = 1; stat; stat >>= 8, ++timer) 
	      {
                if (0x80 & stat) 
                  {
                    __tmr_handle_interrupt(timer);

		    handled = 1;
                  }
              }
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
        int timer, rval;

        switch (isa_readw(0xd801c)) {
        case 0x7301:
        case 0x7305:
                vmitmr_base = 0xd8000;
                break;
        default:
                return -ENODEV;
        }

        if (!request_mem_region(vmitmr_base, 1, MOD_NAME)
            || !request_mem_region(vmitmr_base + VMITMR_CSR(1), 3, MOD_NAME)
            || !request_mem_region(vmitmr_base + VMITMR_LCR(1), 12, MOD_NAME)) {
                printk(KERN_ERR MOD_NAME ": Error requesting region\n");
                return -EBUSY;
        }

        isa_writeb(0, VMITMR_ENA);
        for (timer = 1; VMITMR_NUM_DEVICES >= timer; ++timer) {
                isa_writeb(0, VMITMR_CSR(timer));
                vmitmr_device[timer].timer = timer;
                tmr_interrupt_disable(timer);
                tmr_set_rate(timer, 1000);
                tmr_set_count(timer, 0xffff);
                vmitmr_device[timer].interrupt_pending = 0;
                init_waitqueue_head(&vmitmr_device[timer].wq);
        }

        if (0 != request_irq(5, vmitmr_isr, SA_INTERRUPT | SA_SHIRQ,
                             MOD_NAME, &file_ops)) {
                printk(KERN_ERR MOD_NAME ": Failure requesting irq 5");
                rval = -EIO;
                goto err_request_irq;
        }

        rval = register_chrdev(MOD_MAJOR, MOD_NAME, &file_ops);
        if (0 > rval) {
                printk(KERN_ERR MOD_NAME ": Failed registering device\n");
                rval = -EIO;
                goto err_register_chrdev;
        }

        /* Store the major device number so we can unregister it on exit.
           If we used dynamic major device number allocation then the allocated
           major device number is returned by register_chrdev. */
        vmitmr_major = (MOD_MAJOR) ? MOD_MAJOR : rval;

        printk(KERN_NOTICE MOD_NAME
               ": Installed VMIC timer module version: %s\n", MOD_VERSION);

        return 0;

      err_register_chrdev:
        free_irq(5, &file_ops);

      err_request_irq:
        release_mem_region(vmitmr_base, 1);
        release_mem_region(vmitmr_base + VMITMR_CSR(1), 3);
        release_mem_region(vmitmr_base + VMITMR_LCR(1), 12);

        return rval;
}


/*===========================================================================
 * Module exit routine
 */
static void __exit vmitmr_exit(void)
{
        int timer;

        unregister_chrdev(vmitmr_major, MOD_NAME);

        for (timer = 1; VMITMR_NUM_DEVICES >= timer; ++timer) {
                tmr_stop(timer);
                tmr_interrupt_disable(timer);
        }

        free_irq(5, &file_ops);

        release_mem_region(vmitmr_base, 1);
        release_mem_region(vmitmr_base + VMITMR_CSR(1), 3);
        release_mem_region(vmitmr_base + VMITMR_LCR(1), 12);

        printk(KERN_NOTICE MOD_NAME ": Exiting %s module version: %s\n",
               MOD_NAME, MOD_VERSION);
}


module_init(vmitmr_init);
module_exit(vmitmr_exit);
