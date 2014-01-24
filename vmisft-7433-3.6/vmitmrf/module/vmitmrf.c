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
#include <linux/interrupt.h>
#include "vmitmrf.h"


#ifdef DEBUG
//#define DPRINTF(x...)  printk(KERN_DEBUG MOD_NAME": "x)
#define DPRINTF(x...)  printk(KERN_ERR MOD_NAME": "x)
#else
#define DPRINTF(x...)
#endif

 /* Make an extra define for interrupt debugging because it's way to noisy!
  */
#ifdef DEBUG_ISR
#define DPRINTF_ISR(x...)  printk(KERN_DEBUG MOD_NAME": "x)
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


static struct pci_device_id vmitmr_pci_tbl[] __initdata = {
	{PCI_VENDOR_ID_VMIC, 0x0004, PCI_ANY_ID, PCI_ANY_ID},
	{PCI_VENDOR_ID_VMIC, 0x0005, PCI_ANY_ID, PCI_ANY_ID},
	{PCI_VENDOR_ID_VMIC, 0x6504, PCI_ANY_ID, PCI_ANY_ID},
	{PCI_VENDOR_ID_VMIC, 0x6505, PCI_ANY_ID, PCI_ANY_ID},
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
	int interrupt_pending;
	wait_queue_head_t wq;
};

static uint32_t vmitmr_addr, vmitmr_size;
static volatile void *vmitmr_base = NULL;
static int vmitmr_major;
static spinlock_t vmitmr_lock = SPIN_LOCK_UNLOCKED;
static struct timer_data vmitmr_device[VMITMR_NUM_DEVICES + 1];
int vmifpga_initialized = 0;


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

	csr = readb(vmitmr_base + VMITMR_TCSR(timer));
	writeb(csr | VMITMR_TCSR1__EN, vmitmr_base + VMITMR_TCSR(timer));

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

	csr = readb(vmitmr_base + VMITMR_TCSR(timer));
	writeb(csr & ~VMITMR_TCSR1__EN, vmitmr_base + VMITMR_TCSR(timer));

	spin_unlock_irqrestore(&vmitmr_lock, irqflags);

	return 0;
}


/*============================================================================
 * Set the timer count
 */
int tmr_set_count(int timer, uint32_t count)
{
	count -= 1;

	DPRINTF("Setting timer %d count to 0x%x\n", timer, count);

	switch (timer) {
	case 1:
		writew(count, vmitmr_base + VMITMR_TMRLCR1);
		break;
	case 2:
		writew(count, vmitmr_base + VMITMR_TMRLCR2);
		break;
	case 3:
		writel(count, vmitmr_base + VMITMR_TMRLCR3);
		break;
	case 4:
		writel(count, vmitmr_base + VMITMR_TMRLCR4);
		break;
	default:
		return -ENODEV;
	}

	return 0;
}


/*============================================================================
 * Get the current timer count
 */
int tmr_get_count(int timer, uint32_t * count)
{
	switch (timer) {
	case 1:
		*count = readw(vmitmr_base + VMITMR_TMRCCR1);
		break;
	case 2:
		*count = readw(vmitmr_base + VMITMR_TMRCCR2);
		break;
	case 3:
		*count = readl(vmitmr_base + VMITMR_TMRCCR3);
		break;
	case 4:
		*count = readl(vmitmr_base + VMITMR_TMRCCR4);
		break;
	default:
		return -ENODEV;
	}

	DPRINTF("Read timer %d count to be 0x%x\n", timer, *count);

	*count += 1;

	return 0;
}


/*============================================================================
 * Set the clock rate of a timer
 */
int tmr_set_rate(int timer, int rate)
{
	uint8_t csr, cs;
	unsigned long irqflags;

	if (tmr_not_valid(timer))
		return -ENODEV;

	switch (rate) {
	case 2000:
		cs = VMITMR_TCSR1__CS__2mhz;
		break;
	case 1000:
		cs = VMITMR_TCSR1__CS__1mhz;
		break;
	case 500:
		cs = VMITMR_TCSR1__CS__500khz;
		break;
	case 250:
		cs = VMITMR_TCSR1__CS__250khz;
		break;
	default:
		return -EINVAL;
	}

	spin_lock_irqsave(&vmitmr_lock, irqflags);

	csr = readb(vmitmr_base + VMITMR_TCSR(timer));
	writeb((csr & ~VMITMR_TCSR1__CS) | cs,
	       vmitmr_base + VMITMR_TCSR(timer));

	spin_unlock_irqrestore(&vmitmr_lock, irqflags);

	return 0;
}


/*============================================================================
 * Get the clock rate of a timer
 */
int tmr_get_rate(int timer, int *rate)
{
	uint8_t cs;
	unsigned long irqflags;

	if (tmr_not_valid(timer))
		return -ENODEV;

	spin_lock_irqsave(&vmitmr_lock, irqflags);

	cs = readb(vmitmr_base + VMITMR_TCSR(timer)) & VMITMR_TCSR1__CS;

	spin_unlock_irqrestore(&vmitmr_lock, irqflags);

	switch (cs) {
	case VMITMR_TCSR1__CS__2mhz:
		*rate = 2000;
		break;
	case VMITMR_TCSR1__CS__1mhz:
		*rate = 1000;
		break;
	case VMITMR_TCSR1__CS__500khz:
		*rate = 500;
		break;
	case VMITMR_TCSR1__CS__250khz:
		*rate = 250;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}


/*============================================================================
 * With latch sync enabled, a read to timers 1 or 2 cause timer's 3 and 4 to
 * latch their values.
 */
void tmr_latch_sync_enable(void)
{
	writel(VMITMR_TCSR2__LS, vmitmr_base + VMITMR_TCSR2);
}


/*============================================================================
 * Disable the timer latch syncronization
 */
void tmr_latch_sync_disable(void)
{
	writel(0, vmitmr_base + VMITMR_TCSR2);
}


/*============================================================================
 * Clear the timer interrupt. We continuously clear the interrupt in a loop
 * until we see it's status cleared in the CSR.
 * NOTE: This function assumes all error checking is completed, and any
 * necessary locks have been acquired.
 */
static inline void __tmr_interrupt_clear(int timer)
{
	while (readb(vmitmr_base + VMITMR_TCSR(timer)) & VMITMR_TCSR1__IRQ_STAT) {
		writel(0, vmitmr_base + VMITMR_TIC(timer));
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
	csr = readb(vmitmr_base + VMITMR_TCSR(timer));
	writeb(csr | VMITMR_TCSR1__IRQ_EN, vmitmr_base + VMITMR_TCSR(timer));
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

	csr = readb(vmitmr_base + VMITMR_TCSR(timer));
	writeb(csr & ~VMITMR_TCSR1__IRQ_EN, vmitmr_base + VMITMR_TCSR(timer));
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

	DPRINTF_ISR("Waiting for timer %d interrupt\n", timer);

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
static ssize_t vmitmr_write(struct file *file_ptr, const char *buffer,
			    size_t nbytes, loff_t * off)
{
	struct timer_data *device = file_ptr->private_data;
	uint32_t count=0;

DPRINTF("1 About to write timer %d\n", device->timer);
//count = nbytes;
//DPRINTF("2 About to write timer %d\n", device->timer);
//count = (uint32_t)buffer;
//DPRINTF("3 About to write timer %d\n", device->timer);
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
static int vmitmr_ioctl(struct inode *inode, struct file *file_ptr,
			uint32_t cmd, unsigned long arg)
{
	struct timer_data *device = file_ptr->private_data;
	int rate, rval;

	switch (cmd) {
	case TIMER_START:
		if (0 > (rval = tmr_start(device->timer)))
			return rval;
		break;
	case TIMER_STOP:
		if (0 > (rval = tmr_stop(device->timer)))
			return rval;
		break;
	case TIMER_RATE_SET:
		if (0 > copy_from_user(&rate, (void *) arg, sizeof (int)))
			return -EFAULT;

		if (0 > (rval = tmr_set_rate(device->timer, rate)))
			return rval;
		break;
	case TIMER_RATE_GET:
		if (0 > (rval = tmr_get_rate(device->timer, &rate)))
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
		if (0 > (rval = tmr_interrupt_enable(device->timer)))
			return rval;
		break;
	case TIMER_INTERRUPT_DISABLE:
		if (0 > (rval = tmr_interrupt_disable(device->timer)))
			return rval;
		break;
	case TIMER_INTERRUPT_WAIT:
		if (0 > (rval = tmr_interrupt_wait(device->timer)))
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
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
	int timer = MINOR(inode->i_rdev);
#else
	int timer = iminor(inode);
#endif

	if (tmr_not_valid(timer))
		return -ENODEV;

	file_ptr->private_data = (void *) &vmitmr_device[timer];
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
	MOD_INC_USE_COUNT;
#endif
	return 0;
}


/*============================================================================
 * Hook for the close file operation
 */
static int vmitmr_close(struct inode *inode, struct file *file_ptr)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
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

	DPRINTF_ISR("timer %d interrupt\n", timer);

	__tmr_interrupt_clear(timer);

	++device->interrupt_pending;
	wake_up_interruptible_all(&device->wq);
}


/*===========================================================================
 * Interrupt service routine. Check each timer to see if it generated an
 * interrupt and process it. Continue to process interrupts until all pending
 * are cleared.
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
static void vmitmr_isr(int irq, void *dev_id, struct pt_regs *regs)
#else
static int vmitmr_isr(int irq, void *dev_id, struct pt_regs *regs)
#endif
{
	uint32_t csr, stat;
	int timer;
	int handled = 0;
	unsigned long irqflags;

	spin_lock_irqsave(&vmitmr_lock, irqflags);

	while ((csr = readl(vmitmr_base + VMITMR_TCSR1))
	       && (stat = csr & 0x01010101)) {
		for (timer = 1; stat; stat >>= 8, ++timer) {
			if (1 & stat) {
				__tmr_handle_interrupt(timer);
				handled = 1;
			}
			}
	}

	spin_unlock_irqrestore(&vmitmr_lock, irqflags);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)
	return (IRQ_RETVAL(handled));
#endif

}


/*===========================================================================
 * Device initialization routine
 */
static int __init vmitmr_init_one(struct pci_dev *dev,
				  const struct pci_device_id *ent)
{
	int timer, rval, bar;
	static int count = 0;

	DPRINTF("Device found, vendor id = 0x%x, device id = 0x%x\n",
		dev->vendor, dev->device);

	switch (dev->device) {
	case 0x0004:
		bar = 2;
		break;
	case 0x0005:
	case 0x6504:
		bar = 1;
		break;
	case 0x6505:
		bar = 0;
		break;
	default:
		printk(KERN_ERR MOD_NAME ": No compatible device found\n");
		return -ENODEV;
		break;
	}
	DPRINTF("Device BAR = %d\n", bar);

	/* This driver only handles one device
	 */
	if (count)
		return -EBUSY;
	++count;

	if (pci_enable_device(dev))
		return -EIO;

	vmitmr_addr = pci_resource_start(dev, bar);
	vmitmr_size = pci_resource_len(dev, bar);

	/* I know all of these devices are memory-mapped. If you add support
	   for an I/O mapped device, fix this code. -DLH
	 */
	vmitmr_addr &= PCI_BASE_ADDRESS_MEM_MASK;
	DPRINTF("Requesting 0x%x bytes at memory address 0x%x\n",
		vmitmr_size, vmitmr_addr);
	/* The WDT's mmeory is stuck right in the middle of this address
	   range, so be careful not tot reserve it.
	 */
	if (!request_mem_region(vmitmr_addr, 0x8, MOD_NAME)) {
		return -EBUSY;
	}
	if (!request_mem_region(vmitmr_addr + 0x10, vmitmr_size - 0x10,
				MOD_NAME)) {
		return -EBUSY;
	}
	if (!(vmitmr_base = ioremap_nocache(vmitmr_addr, vmitmr_size))) {
		printk(KERN_ERR MOD_NAME ": Failed mapping the device\n");
		rval = -ENOMEM;
		goto err_ioremap;
	}

	/* Initialize all timers
	 */
	writel(0, vmitmr_base + VMITMR_TCSR1);
	writel(0, vmitmr_base + VMITMR_TCSR2);
	writew(0, vmitmr_base + VMITMR_TMRLCR1);
	writew(0, vmitmr_base + VMITMR_TMRLCR2);
	writel(0, vmitmr_base + VMITMR_TMRLCR3);
	writel(0, vmitmr_base + VMITMR_TMRLCR4);

	for (timer = 1; VMITMR_NUM_DEVICES >= timer; ++timer) {
		vmitmr_device[timer].timer = timer;
		tmr_set_rate(timer, 1000);
		vmitmr_device[timer].interrupt_pending = 0;
		init_waitqueue_head(&vmitmr_device[timer].wq);
	}

	if (0 != request_irq(dev->irq, vmitmr_isr, SA_INTERRUPT | SA_SHIRQ,
			     MOD_NAME, &file_ops)) {
		printk(KERN_ERR MOD_NAME ": Failure requesting irq %d\n",
		       dev->irq);
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

      err_ioremap:
	release_mem_region(vmitmr_addr, 0x8);
	release_mem_region(vmitmr_addr + 0x10, vmitmr_size - 0x10);

	return rval;
}


/*===========================================================================
 * Device exit routine
 */
static void __devexit vmitmr_remove_one(struct pci_dev *dev)
{
	unregister_chrdev(vmitmr_major, MOD_NAME);

	/* Disable all timers and timer interrupts
	 */
	writel(0, vmitmr_base + VMITMR_TCSR1);

	free_irq(dev->irq, &file_ops);

	iounmap((void *) vmitmr_base);

	release_mem_region(vmitmr_addr, 0x8);
	release_mem_region(vmitmr_addr + 0x10, vmitmr_size - 0x10);

	printk(KERN_NOTICE MOD_NAME ": Exiting %s module version: %s\n",
	       MOD_NAME, MOD_VERSION);
}


/*===========================================================================
 * Module initialization routine
 */
static int __init vmitmr_init(void)
{
	int rval;
#if 0
	if (1 > pci_module_init(&vmitmr_driver))
		return -ENODEV;

	return 0;
#endif
	rval = pci_module_init(&vmitmr_driver);
	if (0 > rval) {
		printk(KERN_ERR MOD_NAME ": error %d on pci_module_init\n",
		       rval);
	}

	return rval;
}


/*===========================================================================
 * Module exit routine
 */
static void __exit vmitmr_cleanup(void)
{
	pci_unregister_driver(&vmitmr_driver);
}


module_init(vmitmr_init);
module_exit(vmitmr_cleanup);
