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
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/reboot.h>
#include <linux/watchdog.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include "vmiwdt-7806.h"


#ifdef DEBUG
#define DPRINTF(x...)  printk(KERN_DEBUG MOD_NAME": "x)
#else
#define DPRINTF(x...)
#endif


/* Older 2.4.x kernel versions did not have *TIMEOUT operations in "watchdog.h"
 */
#ifndef WDIOC_SETTIMEOUT
#define WDIOC_SETTIMEOUT        _IOWR(WATCHDOG_IOCTL_BASE, 6, int)
#define WDIOC_GETTIMEOUT        _IOR(WATCHDOG_IOCTL_BASE, 7, int)
#define WDIOF_SETTIMEOUT        0x0080
#endif				/* WDIOC_SETTIMEOUT */


MODULE_AUTHOR("GE Fanuc <www.gefanuc.com/embedded>");
MODULE_DESCRIPTION("GE Fanuc Watchdog Timer Driver");
#ifdef MODULE_LICENSE
MODULE_LICENSE("Dual BSD/GPL");
#endif


static int vmiwdt_open(struct inode *inode, struct file *file_ptr);
static int vmiwdt_close(struct inode *inode, struct file *file_ptr);
static ssize_t vmiwdt_write(struct file *file_ptr, const char *buffer,
			    size_t nbytes, loff_t * off);
static int vmiwdt_ioctl(struct inode *inode, struct file *file_ptr,
			unsigned int cmd, unsigned long arg);
static int vmiwdt_notify(struct notifier_block *notify, unsigned long event,
			 void *reserved);
static struct file_operations file_ops = {
	.owner = THIS_MODULE,
	.write = vmiwdt_write,
	.ioctl = vmiwdt_ioctl,
	.open = vmiwdt_open,
	.release = vmiwdt_close
};


static struct miscdevice miscdev = {
	WATCHDOG_MINOR,
	MOD_NAME,
	&file_ops
};

static struct notifier_block vmiwdt_notifier = {
	vmiwdt_notify,
	NULL,
	0
};

static struct pci_dev *vmiwdt_pci_dev;
static uint32_t vmiwdt_addr, vmiwdt_size;
static volatile void *vmiwdt_base = NULL;
static int vmiwdt_count = 0;
static spinlock_t vmiwdt_lock = SPIN_LOCK_UNLOCKED;
static uint8_t vmiwdt_rev_id;
static int wd_time = VMIWDT_MAX_TIME_MS;
static int boot_status = 0;


MODULE_PARM(wd_time, "i");

/*============================================================================
 * Unlock the WDT
 */
static inline void __wdt_unlock(void)
{
	writew(0x80, vmiwdt_base + VMIWDT_RELOAD);
	writew(0x86, vmiwdt_base + VMIWDT_RELOAD);
}

/*============================================================================
 * Kick the dog
 */
static inline void __wdt_keepalive(void)
{
	__wdt_unlock();
	writew(VMIWDT_RELOAD__WDT_RELOAD, vmiwdt_base + VMIWDT_RELOAD);
}

/*============================================================================
 * Kick the dog
 */
static inline void wdt_keepalive(void)
{
	spin_lock(&vmiwdt_lock);
	__wdt_keepalive();
	spin_unlock(&vmiwdt_lock);
	DPRINTF("Keepalive\n");
}

/*============================================================================
 * Disable the WDT
 */
static inline void __wdt_disable(void)
{
	pci_write_config_byte(vmiwdt_pci_dev, VMIWDT_LOCK, 
		(VMIWDT_LOCK__TIMEOUT_MD | VMIWDT_LOCK__DISABLE));
}

/*============================================================================
 * Disable the WDT
 */
static void wdt_disable(void)
{
#ifndef CONFIG_WATCHDOG_NOWAYOUT
	spin_lock(&vmiwdt_lock);
	/* It is recommended to guarantee that a timeout is not about to 
	 * occur before disabling the timer.  A reload sequence is
	 * suggested.
	 */
	__wdt_keepalive();
	__wdt_disable();
	spin_unlock(&vmiwdt_lock);
	DPRINTF("Disabled watchdog timer\n");
#endif				/* CONFIG_WATCHDOG_NOWAYOUT */
}

/*============================================================================
 * Enable the WDT
 */
static inline void __wdt_enable(void)
{
	pci_write_config_byte(vmiwdt_pci_dev, VMIWDT_LOCK, 
		(VMIWDT_LOCK__TIMEOUT_MD | VMIWDT_LOCK__ENABLE));
}

/*============================================================================
 * Enable the WDT
 */
static void wdt_enable(void)
{
	spin_lock(&vmiwdt_lock);
	__wdt_enable();
	spin_unlock(&vmiwdt_lock);
	DPRINTF("Enabled watchdog timeout\n");
}

/*============================================================================
 * Get timeout - returns the timeout in milliseconds.
 */
static uint32_t __wdt_get_timeout(void)
{
	uint32_t timeout;

	spin_lock(&vmiwdt_lock);
	timeout = readl(vmiwdt_base + VMIWDT_PRELOAD1) & VMIWDT_PRELOAD__MASK;
	spin_unlock(&vmiwdt_lock);

	/* The timeout value in the preload register is 1 less than the 
	   period.
	 */
	timeout++;

	return timeout;
}


/*============================================================================
 * Get timeout - returns timeout value in number of milliseconds
 */
static int wdt_get_timeout(void)
{
	int timeout;

	timeout = __wdt_get_timeout();

	return timeout;
}


/*============================================================================
 * Set timeout - input is in milliseconds.
 */
static inline int __wdt_set_timeout(int timeout)
{

	spin_lock(&vmiwdt_lock);

	/* The timeout value in the preload register is 1 less than the 
	   period.
	 */
	__wdt_unlock();
	writel((timeout-1) & VMIWDT_PRELOAD__MASK, 
		vmiwdt_base + VMIWDT_PRELOAD1);

	__wdt_unlock();
	writel(0, vmiwdt_base + VMIWDT_PRELOAD2);
	
	spin_unlock(&vmiwdt_lock);
	return 0;
}


/*============================================================================
 * Set timeout - sets the actual timeout value.
 */
static int wdt_set_timeout(int *timeout)
{
	int timeoutval;

	if (*timeout < VMIWDT_MIN_TIME_MS) {
		return -EINVAL;
	} else if (*timeout > VMIWDT_MAX_TIME_MS) {
		timeoutval = VMIWDT_MAX_TIME_MS;
		*timeout = timeoutval;
	} else {
		timeoutval = *timeout;
	}

	DPRINTF("Setting timeout value to %dms\n", *timeout);

	return __wdt_set_timeout(timeoutval);
}


/*============================================================================
 * Hook for notification of system events
 */
static int vmiwdt_notify(struct notifier_block *notify,
			 unsigned long event, void *reserved)
{
	/* Disable the timer upon shutdown or reboot regardless of 
	   whether CONFIG_WATCHDOG_NOWAYOUT is enabled or not
	 */
	if ((SYS_DOWN == event) || (SYS_HALT == event)) {
		spin_lock(&vmiwdt_lock);
		__wdt_keepalive();
		__wdt_disable();
		spin_unlock(&vmiwdt_lock);
	}

	return NOTIFY_DONE;
}


/*============================================================================
 * Hook for the write file operation
 */
static ssize_t vmiwdt_write(struct file *file_ptr,
			    const char *buffer, size_t nbytes, loff_t * off)
{
	wdt_keepalive();
	return 1;
}


/*============================================================================
 * Hook to the ioctl file operation
 */
int vmiwdt_ioctl(struct inode *inode,
		 struct file *file_ptr, uint32_t cmd, unsigned long arg)
{
	int status;
	int timeout;
	static struct watchdog_info wdt_info = {
		WDIOF_SETTIMEOUT | WDIOF_CARDRESET,
		0, MOD_NAME
	};
	switch (cmd) {
	case WDIOC_GETSUPPORT:
		wdt_info.firmware_version = vmiwdt_rev_id;
		if (0 >
		    copy_to_user((void *) arg, &wdt_info, sizeof (wdt_info)))
			return -EFAULT;
		break;
	case WDIOC_GETBOOTSTATUS:
		if (0 > copy_to_user((void *) arg, &boot_status, 
				sizeof (boot_status)))
			return -EFAULT;
		break;
	case WDIOC_KEEPALIVE:
		wdt_keepalive();
		break;
	case WDIOC_SETTIMEOUT:
		if (0 > copy_from_user(&timeout, (void *) arg, sizeof (int)))
			return -EFAULT;
		/* The wdt_set_timeout function expects milliseconds
		 */
		timeout *= 1000;
		status = wdt_set_timeout(&timeout);
		if (0 != status) {
			return status;
		}
		timeout /= 1000;
		if (0 > copy_to_user((void *) arg, &timeout, sizeof (int)))
			return -EFAULT;
		break;
#ifdef WDIOC_SETTIMEOUT_MS
	case WDIOC_SETTIMEOUT_MS:
#endif
		if (0 > copy_from_user(&timeout, (void *) arg, sizeof (int)))
			return -EFAULT;
		status = wdt_set_timeout(&timeout);
		if (0 != status) {
			return status;
		}
		if (0 > copy_to_user((void *) arg, &timeout, sizeof (int)))
			return -EFAULT;
		break;
	case WDIOC_GETTIMEOUT:
		/* The wdt_get_timeout function returns milliseconds
		 */
		timeout = wdt_get_timeout();
		timeout /= 1000;
		if (0 > copy_to_user((void *) arg, &timeout, sizeof (timeout)))
			return -EFAULT;
		break;
#ifdef WDIOC_GETTIMEOUT_MS
	case WDIOC_GETTIMEOUT_MS:
#endif
		timeout = wdt_get_timeout();
		if (0 > copy_to_user((void *) arg, &timeout, sizeof (timeout)))
			return -EFAULT;
		break;
	default:
		return -ENOTTY;
	}

	return 0;
}


/*============================================================================
 * Hook for the open file operation
 */
static int vmiwdt_open(struct inode *inode, struct file *file_ptr)
{
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,5,0)
	MOD_INC_USE_COUNT;
#endif

	if (1 == ++vmiwdt_count) {
		wdt_enable();
	}

	return 0;
}


/*============================================================================
 * Hook for the close file operation
 */
static int vmiwdt_close(struct inode *inode, struct file *file_ptr)
{
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,5,0)
	MOD_DEC_USE_COUNT;
#endif

	if (0 == --vmiwdt_count) {
		wdt_disable();
	}

	return 0;
}


/*===========================================================================
 * Module initialization routine
 */
static int __init vmiwdt_init(void)
{
	int bar, rval;
	uint16_t cfg_reg, reload_reg;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
	if (!pci_present()) {
		printk(KERN_ERR MOD_NAME ": PCI bios not found\n");
		return -ENODEV;
	}
#endif

	if ((vmiwdt_pci_dev =
	     pci_find_device(PCI_VENDOR_ID_INTEL, 0x25AB, NULL))) {
		bar = PCI_BASE_ADDRESS_0;
	} else {
		printk(KERN_ERR MOD_NAME ": No compatible device found\n");
		return -ENODEV;
	}

	/* Get the base address
	 */
	pci_read_config_dword(vmiwdt_pci_dev, bar, (u32 *) & vmiwdt_addr);
	/* Determine the size
	 */
	vmiwdt_size = ~0;
	pci_write_config_dword(vmiwdt_pci_dev, bar, vmiwdt_size);
	pci_read_config_dword(vmiwdt_pci_dev, bar, &vmiwdt_size);
	pci_write_config_dword(vmiwdt_pci_dev, bar, vmiwdt_addr);
	vmiwdt_size = ~(vmiwdt_size & ~0xF) + 1;
	/* I know all of these devices are memory-mapped. If you add support
	   for an I/O mapped device, fix this code. -DLH
	 */
	vmiwdt_addr &= PCI_BASE_ADDRESS_MEM_MASK;
	DPRINTF
	    ("Requesting 0x%x bytes at memory address 0x%x\n",
	     vmiwdt_size, vmiwdt_addr);

	if (!request_mem_region(vmiwdt_addr, vmiwdt_size, MOD_NAME)) {
		printk(KERN_ERR MOD_NAME ": Failed requesting mem region\n");
		return -EBUSY;
	}

	vmiwdt_base = ioremap_nocache(vmiwdt_addr, vmiwdt_size);
	if (NULL == vmiwdt_base) {
		printk(KERN_ERR MOD_NAME ": Failed mapping the device\n");
		rval = -ENOMEM;
		goto err_ioremap;
	}
	pci_read_config_byte(vmiwdt_pci_dev, PCI_REVISION_ID, &vmiwdt_rev_id);

	boot_status = 0;
	spin_lock(&vmiwdt_lock);

	/* Disable watchdog timer prior to setting configuration.
	 */
	__wdt_disable();

	pci_read_config_word(vmiwdt_pci_dev, VMIWDT_CFG,&cfg_reg);
	pci_write_config_word(vmiwdt_pci_dev, VMIWDT_CFG, 
		(cfg_reg | VMIWDT_CFG__OUT_EN | VMIWDT_CFG__1KHZ | 
		 VMIWDT_CFG__INT_DISABLE));

	/* Determine if board was previously reset by watchdog.
	 */	
	reload_reg = readw(vmiwdt_base + VMIWDT_RELOAD);
	if (reload_reg & VMIWDT_RELOAD__WDT_TIMEOUT) {
		boot_status |= WDIOF_CARDRESET;
		__wdt_unlock();
		writew(VMIWDT_RELOAD__WDT_TIMEOUT, vmiwdt_base + VMIWDT_RELOAD);
	}

	spin_unlock(&vmiwdt_lock);
		
	rval = wdt_set_timeout(&wd_time);
	if (0 != rval) {
		printk(KERN_ERR MOD_NAME ": Failed initializing timeout\n");
		goto err_wdtset;
	}

	register_reboot_notifier(&vmiwdt_notifier);

	rval = misc_register(&miscdev);
	if (0 != rval) {
		printk(KERN_ERR MOD_NAME ": Failed registering device\n");
		goto err_register;
	}
	printk(KERN_NOTICE MOD_NAME
	       ": Installed VMIC watchdog timer module version: %s\n",
	       MOD_VERSION);
	return 0;
      err_register:
	unregister_reboot_notifier(&vmiwdt_notifier);
      err_wdtset:
	iounmap((void *) vmiwdt_base);
      err_ioremap:
	release_mem_region(vmiwdt_addr, vmiwdt_size);
	return rval;
}


/*===========================================================================
 * Module exit routine
 */
static void __exit vmiwdt_cleanup(void)
{
	misc_deregister(&miscdev);
	/* If we unload the module, the watchdog gets disabled regardless of
	   whether CONFIG_WATCHDOG_NOWAYOUT is enabled or not
	 */
	spin_lock(&vmiwdt_lock);
	__wdt_keepalive();
	__wdt_disable();
	spin_unlock(&vmiwdt_lock);
	unregister_reboot_notifier(&vmiwdt_notifier);
	iounmap((void *) vmiwdt_base);
	release_mem_region(vmiwdt_addr, vmiwdt_size);
	printk(KERN_NOTICE MOD_NAME
	       ": Exiting %s module version: %s\n", MOD_NAME, MOD_VERSION);
}


module_init(vmiwdt_init);
module_exit(vmiwdt_cleanup);
