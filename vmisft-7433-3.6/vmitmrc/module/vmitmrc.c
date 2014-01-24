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
#include "vmitmrc.h"


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
int tmr_set_count(int timer, uint32_t count);
EXPORT_SYMBOL(tmr_set_count);
int tmr_get_count(int timer, uint32_t * count);
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

static struct pci_device_id vmitmr_pci_tbl[] __initdata = {
        {PCI_VENDOR_ID_VMIC, 0x7696, PCI_ANY_ID, PCI_ANY_ID},
        {PCI_VENDOR_ID_VMIC, 0x7697, PCI_ANY_ID, PCI_ANY_ID},
        {0}
};
MODULE_DEVICE_TABLE(pci, vmitmr_pci_tbl);

static int __init vmitmr_init_one(struct pci_dev *dev,
                                  const struct pci_device_id *ent);
static void __devexit vmitmr_remove_one(struct pci_dev *dev);
static struct pci_driver vmitmr_driver = {
        .name = MOD_NAME,
        .id_table = vmitmr_pci_tbl,
        .probe = vmitmr_init_one,
        .remove = vmitmr_remove_one
};

struct timer_data {
        int timer;
        int running;
        int interrupt_pending;
        wait_queue_head_t wq;
};

static void *vmitmr_base = NULL;
static int vmitmr_irq;
static int vmitmr_major;
static struct timer_data vmitmr_device[VMITMR_NUM_DEVICES + 1];
static spinlock_t vmitmr_lock = SPIN_LOCK_UNLOCKED;


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
static inline void __tmr_start(int timer)
{
        uint8_t tei;

        tei = readb(vmitmr_base + VMITMR_TEI);
        writeb(tei | VMITMR_TEI__ENABLE(timer), vmitmr_base + VMITMR_TEI);
}


/*============================================================================
 * Start the timer countdown
 */
int tmr_start(int timer)
{
        unsigned long irqflags;

        if (tmr_not_valid(timer))
                return -ENODEV;

        spin_lock_irqsave(&vmitmr_lock, irqflags);
        __tmr_start(timer);
        vmitmr_device[timer].running = 1;
        spin_unlock_irqrestore(&vmitmr_lock, irqflags);

        return 0;
}


/*============================================================================
 * Stop the timer countdown
 */
static inline void __tmr_stop(int timer)
{
        uint8_t tei;

        tei = readb(vmitmr_base + VMITMR_TEI);
        writeb(tei & ~VMITMR_TEI__ENABLE(timer), vmitmr_base + VMITMR_TEI);
}


/*============================================================================
 * Stop the timer countdown
 */
int tmr_stop(int timer)
{
        unsigned long irqflags;

        if (tmr_not_valid(timer))
                return -ENODEV;

        spin_lock_irqsave(&vmitmr_lock, irqflags);
        __tmr_stop(timer);
        vmitmr_device[timer].running = 0;
        spin_unlock_irqrestore(&vmitmr_lock, irqflags);

        return 0;
}


/*============================================================================
 * Set the timer count
 */
int tmr_set_count(int timer, uint32_t count)
{
        unsigned long irqflags;
        uint32_t load_count;
        uint8_t twss;

        if (tmr_not_valid(timer))
                return -ENODEV;

        if (3 > count)
                return -EINVAL;

        spin_lock_irqsave(&vmitmr_lock, irqflags);

        __tmr_stop(timer);

        /* Figuring out the count to load is fun!
         */
        load_count = count - 2;
        if (0 == (load_count % 0x10000))
                load_count -= 0x10000;

        /* Set the count mode (16 or 32-bit)
         */
        if (0x10000 > load_count) {
                twss = readb(vmitmr_base + VMITMR_TWSS);
                writeb(twss & ~VMITMR_TWSS__32bit(timer),
                       vmitmr_base + VMITMR_TWSS);
                DPRINTF(MOD_NAME ": Timer mode is 16-bit\n");
        } else {
                twss = readb(vmitmr_base + VMITMR_TWSS);
                writeb(twss | VMITMR_TWSS__32bit(timer),
                       vmitmr_base + VMITMR_TWSS);
                DPRINTF(MOD_NAME ": Timer mode is 32-bit\n");
        }

        /* Load the count value
         */
        writeb(VMITMR_TMR__WRITE_SCALE, vmitmr_base + VMITMR_TMR(timer));
        writeb(0, vmitmr_base + VMITMR_SC(timer));
        writeb(0, vmitmr_base + VMITMR_SC(timer));
        writeb(VMITMR_TMR__WRITE_LOWER, vmitmr_base + VMITMR_TMR(timer));
        writeb(load_count & 0xff, vmitmr_base + VMITMR_LC(timer));
        writeb((load_count >> 8) & 0xff, vmitmr_base + VMITMR_LC(timer));
        writeb(VMITMR_TMR__WRITE_UPPER, vmitmr_base + VMITMR_TMR(timer));
        writeb((load_count >> 16) & 0xff, vmitmr_base + VMITMR_UC(timer));
        writeb((load_count >> 24) & 0xff, vmitmr_base + VMITMR_UC(timer));

        /* If the timer was already running, restart it with the new count
         */
        if (vmitmr_device[timer].running)
                __tmr_start(timer);

        spin_unlock_irqrestore(&vmitmr_lock, irqflags);

        return 0;
}


/*============================================================================
 * Get the current timer count
 */
int tmr_get_count(int timer, uint32_t * count)
{
        unsigned long irqflags;

        if (tmr_not_valid(timer))
                return -ENODEV;

        spin_lock_irqsave(&vmitmr_lock, irqflags);

        if (readb(vmitmr_base + VMITMR_TWSS) & VMITMR_TWSS__32bit(timer)) {
                /* 32-bit mode timer count is broken in the hardware. Your
                   initial count will be 0x10000 less than the expected count.
                   Once you count down to the next 0x10000 boundary though,
                   the count will correct itself, so interrupts will happen at
                   the correct scheduled time. There is no way to detect this
                   aberrant condition and compensate for it, so instead, just
                   return the user an error so he know's it's broken.
                 */
#if 0
                writeb(VMITMR_TMR__READBACK_32bit,
                       vmitmr_base + VMITMR_TMR(timer));
                *count = readb(vmitmr_base + VMITMR_LC(timer));
                *count |= readb(vmitmr_base + VMITMR_LC(timer)) << 8;
                *count |= readb(vmitmr_base + VMITMR_UC(timer)) << 16;
                *count |= readb(vmitmr_base + VMITMR_UC(timer)) << 24;
                *count += 0x10002;
#endif
                *count = 0;
                spin_unlock_irqrestore(&vmitmr_lock, irqflags);
                return -ENOSYS;

        } else {
                writeb(VMITMR_TMR__READBACK_16bit,
                       vmitmr_base + VMITMR_TMR(timer));
                *count = readb(vmitmr_base + VMITMR_LC(timer));
                *count |= readb(vmitmr_base + VMITMR_LC(timer)) << 8;
                ++(*count);
                *count &= 0xffff;
        }
        spin_unlock_irqrestore(&vmitmr_lock, irqflags);

        DPRINTF(MOD_NAME ": Timer %d count = %d\n", timer, *count);

        return 0;
}


/*============================================================================
 * Enable the timer interrupt.
 */
static inline void __tmr_interrupt_enable(int timer)
{
        uint8_t tei;

        tei = readb(vmitmr_base + VMITMR_TEI);
        writeb(tei & ~VMITMR_TEI__INTR_MASK(timer), vmitmr_base + VMITMR_TEI);
        vmitmr_device[timer].interrupt_pending = 0;
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

        DPRINTF(MOD_NAME ": Timer %d interrupt enabled\n", timer);

        spin_unlock_irqrestore(&vmitmr_lock, irqflags);

        return 0;
}


/*============================================================================
 * Disable the timer interrupt
 */
static inline void __tmr_interrupt_disable(int timer)
{
        uint8_t tei;

        tei = readb(vmitmr_base + VMITMR_TEI);
        writeb(tei | VMITMR_TEI__INTR_MASK(timer), vmitmr_base + VMITMR_TEI);
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

        DPRINTF(MOD_NAME ": Timer %d interrupt disabled\n", timer);

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
static ssize_t
vmitmr_read(struct file *file_ptr, char *buffer, size_t nbytes, loff_t * off)
{
        struct timer_data *device = file_ptr->private_data;
        uint32_t count;
        int rval;

        rval = tmr_get_count(device->timer, &count);

	DPRINTF("Read timer %d count 0x%x\n", device->timer, &count);

        if (0 > rval) {
                return rval;
        }

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
static ssize_t
vmitmr_write(struct file *file_ptr, const char *buffer, size_t nbytes,
             loff_t * off)
{
        struct timer_data *device = file_ptr->private_data;
        uint32_t count=0;

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
                count = *(uint32_t *) buffer;
                break;
        case 8:
                count = *(uint64_t *) buffer;
                break;
        default:
                return -EINVAL;
        }
*/

	DPRINTF("Writing timer %d count 0x%x\n", device->timer, count);

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
static int
vmitmr_ioctl(struct inode *inode, struct file *file_ptr, uint32_t cmd,
             unsigned long arg)
{
        struct timer_data *device = file_ptr->private_data;

        switch (cmd) {
        case TIMER_START:
                return tmr_start(device->timer);
                break;
        case TIMER_STOP:
                return tmr_stop(device->timer);
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
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,5,0)
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

        DPRINTF_ISR(MOD_NAME ": timer %d interrupt\n", timer);
        ++device->interrupt_pending;
        wake_up_interruptible_all(&device->wq);
}


/*===========================================================================
 * Interrupt service routine
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)
int vmitmr_isr(int irq, void *dev_id, struct pt_regs *regs)
#else
static void vmitmr_isr(int irq, void *dev_id, struct pt_regs *regs)
#endif
{
        uint32_t stat;
        int timer;
	int handled = 0;

        spin_lock(&vmitmr_lock);

        while ((stat = readb(vmitmr_base + VMITMR_TIS) & 0x7)) 
          {
             for (timer = 1; stat; stat >>= 1, ++timer) 
               {
                 if (1 & stat) 
                   {
                      __tmr_handle_interrupt(timer);
		      handled = 1;
                   }
               }
          }

        spin_unlock(&vmitmr_lock);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)
	return(IRQ_RETVAL(handled));
#endif
}


/*===========================================================================
 * Device initialization routine
 */
static int __init vmitmr_init_one(struct pci_dev *dev,
                                  const struct pci_device_id *ent)
{
        uint32_t phys_addr, size;
        int timer, rval;
        static int count = 0;

        /* This driver only handles one device
         */
        if (count)
                return -EBUSY;
        ++count;

        if (pci_enable_device(dev))
                return -EIO;

        phys_addr = pci_resource_start(dev, VMITMR_BAR);
        size = pci_resource_len(dev, VMITMR_BAR);

        /* I know all of these devices are memory-mapped. If you add support
           for an I/O mapped device, fix this code. -DLH
         */
        phys_addr &= PCI_BASE_ADDRESS_MEM_MASK;
        vmitmr_base = ioremap_nocache(phys_addr, size);
        if (!vmitmr_base) {
                printk(KERN_ERR MOD_NAME ": Failed mapping the device\n");
                return -ENOMEM;
        }

        /* Initialize timers
         */
        for (timer = 1; timer <= VMITMR_NUM_DEVICES; ++timer) {
                vmitmr_device[timer].timer = timer;
                vmitmr_device[timer].interrupt_pending = 0;
                init_waitqueue_head(&vmitmr_device[timer].wq);
                tmr_interrupt_disable(timer);
                tmr_stop(timer);
                tmr_set_count(timer, 0xffffffff);
        }

        /* Legacy IRQ assignment. These timers are hardwired to IRQ5.
         */
        vmitmr_irq = 5;
        if (0 != request_irq(vmitmr_irq, vmitmr_isr, SA_INTERRUPT | SA_SHIRQ,
                             MOD_NAME, &file_ops)) {
                printk(KERN_ERR MOD_NAME ": Failure requesting irq %d\n",
                       vmitmr_irq);
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
           major device number is returned by register_chrdev.
         */
        vmitmr_major = (MOD_MAJOR) ? MOD_MAJOR : rval;

        printk(KERN_NOTICE MOD_NAME
               ": Installed VMIC timer module version: %s\n", MOD_VERSION);

        return 0;

      err_register_chrdev:
        free_irq(dev->irq, &file_ops);

      err_request_irq:
        iounmap((void *) vmitmr_base);

        return rval;
}


/*===========================================================================
 * Device exit routine
 */
static void __devexit vmitmr_remove_one(struct pci_dev *dev)
{
        int timer;

        unregister_chrdev(vmitmr_major, MOD_NAME);

        /* Disable all timers and timer interrupts
         */
        for (timer = 1; timer <= VMITMR_NUM_DEVICES; ++timer) {
                tmr_interrupt_disable(timer);
                tmr_stop(timer);
        }

        free_irq(vmitmr_irq, &file_ops);

        if (vmitmr_base)
                iounmap(vmitmr_base);

        printk(KERN_NOTICE MOD_NAME ": Exiting %s module version: %s\n",
               MOD_NAME, MOD_VERSION);
}


/*===========================================================================
 * Module initialization routine
 */
static int __init vmitmr_init(void)
{
        if (1 > pci_register_driver(&vmitmr_driver))
                return -ENODEV;

        return 0;
}


/*===========================================================================
 * Module exit routine
 */
static void __exit vmitmr_exit(void)
{
        pci_unregister_driver(&vmitmr_driver);
}


module_init(vmitmr_init);
module_exit(vmitmr_exit);
