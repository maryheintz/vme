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


#include <asm/uaccess.h>
#include <linux/config.h>
#include <linux/init.h>
#include <linux/miscdevice.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/string.h>


#ifdef DEBUG
#define DPRINTF(x...)  printk(KERN_DEBUG x)
#else
#define DPRINTF(x...)
#endif


MODULE_AUTHOR("GE Fanuc <www.gefanuc.com/embedded>");
MODULE_DESCRIPTION("GE Fanuc non-volatile ram driver");
#ifdef MODULE_LICENSE
MODULE_LICENSE("Dual BSD/GPL");
#endif


void *nvram_map(void);
EXPORT_SYMBOL(nvram_map);
size_t nvram_size(void);
EXPORT_SYMBOL(nvram_size);


static int vminvr_open(struct inode *inode, struct file *file_ptr);
static int vminvr_close(struct inode *inode, struct file *file_ptr);
static ssize_t vminvr_read(struct file *file_ptr, char *buffer,
			   size_t nbytes, loff_t * off);
static ssize_t vminvr_write(struct file *file_ptr, const char *buffer,
			    size_t nbytes, loff_t * off);
static loff_t vminvr_llseek(struct file *file_ptr, loff_t off, int whence);
static int vminvr_mmap(struct file *file_ptr, struct vm_area_struct *vma);
static struct file_operations file_ops;


static unsigned long phys_addr = 0;
MODULE_PARM(phys_addr, "i");
static uint32_t size = 0;
MODULE_PARM(size, "i");

static void *vminvr_base = NULL;
static struct miscdevice miscdev;


/*============================================================================
 * Return a memory-mapped pointer to the NVRAM base
 */
void *nvram_map(void)
{
	return vminvr_base;
}


/*============================================================================
 * Return the NVRAM size
 */
size_t nvram_size(void)
{
	return size;
}


/*============================================================================
 * Hook for the lseek file operation
 */
static loff_t vminvr_llseek(struct file *file_ptr, loff_t off, int whence)
{
	switch (whence) {
	case 0:		/* SEEK_SET */
		/* Do nothing */
		break;
	case 1:		/* SEEK_CUR */
		off += file_ptr->f_pos;
		break;
	case 2:		/* SEEK_END */
		off += size;
		break;
	default:
		return -EINVAL;
	}

	if (0 > off)
		return -EINVAL;

	return (file_ptr->f_pos = off);
}


/*============================================================================
 * Hook for the read file operation
 */
static ssize_t
vminvr_read(struct file *file_ptr, char *buffer, size_t nbytes, loff_t * off)
{
	int count, bytes_rem;

	/* Calculate how many bytes we can read (all|some|none)?
	 */
	count = (nbytes > size - *off) ? size - *off : nbytes;
	if (0 >= count)
		return 0;

	/* Transfer the data
	 */
	bytes_rem = copy_to_user(buffer, vminvr_base + *off, count);
	if (0 > bytes_rem)
		return -EFAULT;

	/* Update the file position
	 */
	count -= bytes_rem;
	*off += count;

	/* Return the actual number of bytes transferred
	 */
	return count;
}


/*============================================================================
 * Hook for the write file operation
 */
static ssize_t
vminvr_write(struct file *file_ptr, const char *buffer, size_t nbytes,
	     loff_t * off)
{
	int count, bytes_rem;

	/* Calculate how many bytes we can read (all|some|none)?
	 */
	count = (nbytes > size - *off) ? size - *off : nbytes;
	if (0 >= count)
		return 0;

	/* Transfer the data
	 */
	bytes_rem = copy_from_user(vminvr_base + *off, buffer, count);
	if (0 > bytes_rem)
		return -EFAULT;

	/* Update the file position
	 */
	count -= bytes_rem;
	*off += count;

	/* Return the actual number of bytes transferred
	 */
	return count;
}


/*============================================================================
 * Hook for the mmap file operation
 */
int vminvr_mmap(struct file *file_ptr, struct vm_area_struct *vma)
{
	unsigned long nbytes = vma->vm_end - vma->vm_start;
	unsigned long offset = vma->vm_pgoff << PAGE_SHIFT;
	unsigned long paddr = phys_addr + offset;

	if (paddr % PAGE_SIZE)
		return -ENXIO;

	if (nbytes + offset > size)
		return -ENOMEM;

	if (offset >= __pa(offset) || (file_ptr->f_flags & O_SYNC))
		vma->vm_flags |= VM_IO;

	vma->vm_flags |= VM_RESERVED;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,3) || defined RH9BRAINDAMAGE
	if (remap_page_range(vma, vma->vm_start, paddr, nbytes,
			     vma->vm_page_prot))
		return -EAGAIN;
#else
	if (remap_page_range(vma->vm_start, paddr, nbytes, vma->vm_page_prot))
		return -EAGAIN;
#endif

	return 0;
}


/*============================================================================
 * Hook for the open file operation
 */
static int vminvr_open(struct inode *inode, struct file *file_ptr)
{
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,5,0)
	MOD_INC_USE_COUNT;
#endif
	return 0;
}


/*============================================================================
 * Hook for the close file operation
 */
static int vminvr_close(struct inode *inode, struct file *file_ptr)
{
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,5,0)
	MOD_DEC_USE_COUNT;
#endif
	return 0;
}


/*============================================================================
 * Find and map a PCI NVRAM device if present.
 */
static int vminvr_find_pci_device(void)
{
	struct pci_dev *vminvr_pci_dev;
	int bar;
/* Obsoleted function!
	if (!pci_present()) {
		printk(KERN_ERR MOD_NAME ": PCI bios not found\n");
		return -1;
	}
*/
	/* FPGA devices */
	if ((vminvr_pci_dev =
	     pci_find_device(PCI_VENDOR_ID_VMIC, 0x0004, NULL)))
		bar = PCI_BASE_ADDRESS_1;
	else if ((vminvr_pci_dev =
		  pci_find_device(PCI_VENDOR_ID_VMIC, 0x0006, NULL)))
		bar = PCI_BASE_ADDRESS_1;
	else if ((vminvr_pci_dev =
		  pci_find_device(PCI_VENDOR_ID_VMIC, 0x6504, NULL)))
		bar = PCI_BASE_ADDRESS_0;
	else if ((vminvr_pci_dev =
		  pci_find_device(PCI_VENDOR_ID_VMIC, 0x6506, NULL)))
		bar = PCI_BASE_ADDRESS_0;
	/* VMIVME-7696 */
	else if ((vminvr_pci_dev =
		  pci_find_device(PCI_VENDOR_ID_VMIC, 0x7696, NULL)))
		bar = PCI_BASE_ADDRESS_3;
	/* VMIVME-7697[a] */
	else if ((vminvr_pci_dev =
		  pci_find_device(PCI_VENDOR_ID_VMIC, 0x7697, NULL)))
		bar = PCI_BASE_ADDRESS_3;
	else			/* No compatible device found */
		return -1;
	/* Get the base address
	 */
	pci_read_config_dword(vminvr_pci_dev, bar, (u32 *) & phys_addr);

	/* Determine the NVRAM size
	 */
	size = ~0;
	pci_write_config_dword(vminvr_pci_dev, bar, size);
	pci_read_config_dword(vminvr_pci_dev, bar, &size);
	pci_write_config_dword(vminvr_pci_dev, bar, phys_addr);
	size = ~(size & ~0xF) + 1;

	/* I know all of these devices are memory-mapped. If you add support
	   for an I/O mapped device, fix this code. -DLH
	 */
	phys_addr &= PCI_BASE_ADDRESS_MEM_MASK;

	vminvr_base = ioremap_nocache(phys_addr, size);
	if (NULL == vminvr_base) {
		printk(KERN_ERR MOD_NAME
		       ": Failure mapping the VMIC PCI NVRAM device\n");
		return -1;
	}

	DPRINTF(MOD_NAME ": VMIC PCI NVRAM device found\n");

	return 0;
}


/*============================================================================
 * Find and map an I/O space NVRAM device if present.
 *
 * We test to see if this board has a DS1384 or compatible device by
 * looking for a valid board id.  Starting with the VMIVME-7750 and the
 * VMICPCI-7753, the NVRAM device has moved to the FPGA part, so hopefully
 * this list will remain complete.
 */
static int vminvr_find_io_device(void)
{
	/* VME boards
	 */
	switch (isa_readw(0xd8016)) {
	case 0x7591:
	case 0x7592:
	case 0x7695:
	case 0x7698:
	case 0x7740:
		phys_addr = 0xd8018;
		size = 32 * 1024 - 24;
		break;
	}

	/* CPCI boards
	 */
	if (!phys_addr) {
		switch (isa_readw(0xD800E)) {
			/* These boards don't have board id's, so we cannot
			   probe for them. Use the module parameters to specify
			   address and size.
			 */
#if 0
		case 0x7593:
		case 0x7594:
		case 0x7696:
		case 0x7697:
#endif
		case 0x7699:
			phys_addr = 0xD8014;
			size = 32 * 1024 - 20;
			break;
		case 0x7715:
		case 0x7716:
			phys_addr = 0xD8010;
			size = 32 * 1024 - 16;
			break;
		}
	}

	if (!phys_addr) {
		switch (isa_readw(0xD801C)) {
		case 0x7301:
		case 0x7305:
			phys_addr = 0xD8020;
			size = 32 * 1024 - 32;
			break;
		}
	}

	if (!phys_addr)
		return -1;

	vminvr_base = ioremap_nocache(phys_addr, size);
	if (NULL == vminvr_base) {
		printk(KERN_ERR MOD_NAME
		       ": Failure mapping the VMIC I/O NVRAM device\n");
		return -1;
	}

	DPRINTF(MOD_NAME ": VMIC I/O NVRAM device found\n");

	return 0;
}


/*============================================================================
 * Find and map a LPC bus NVRAM device if present.
 */
static int vminvr_find_lpc_device(void)
{
	void *board_id_addr;

	board_id_addr = ioremap_nocache(0xFFC0001F, 2);

	switch (readw(board_id_addr)) {
	case 0x7505:
		phys_addr = 0xFFC00020;
		size = 32 * 1024 - 32;
		break;
	}

	iounmap(board_id_addr);

	if (!phys_addr)
		return -1;

	vminvr_base = ioremap_nocache(phys_addr, size);
	if (NULL == vminvr_base) {
		printk(KERN_ERR MOD_NAME
		       ": Failure mapping the LPC bus VMIC NVRAM device\n");
		return -1;
	}

	DPRINTF(MOD_NAME ": VMIC LPC bus NVRAM device found\n");

	return 0;
}


/*===========================================================================
 * Module initialization routine
 */
static int __init vminvr_init(void)
{
	int rval;

	if (phys_addr && size) {
		/* The device address and size were set using module parameters
		 */
		vminvr_base = ioremap_nocache(phys_addr, size);
		if (NULL == vminvr_base) {
			printk(KERN_ERR MOD_NAME
			       ": Failure mapping the VMIC NVRAM device\n");
			return -1;
		}
	} else {
		/* Probe for the device
		 */
		if ((0 != vminvr_find_pci_device())
		    && (0 != vminvr_find_io_device())
		    && (0 != vminvr_find_lpc_device())) {
			printk(KERN_ERR MOD_NAME ": Not a supported board\n");
			return -1;
		}
	}

	memset(&file_ops, 0, sizeof(file_ops));
	file_ops.owner = THIS_MODULE;
	file_ops.llseek = vminvr_llseek;
	file_ops.read = vminvr_read;
	file_ops.write = vminvr_write;
	file_ops.mmap = vminvr_mmap;
	file_ops.open = vminvr_open;
	file_ops.release = vminvr_close;

	memset(&miscdev, 0, sizeof(miscdev));
	miscdev.minor = NVRAM_MINOR; 
	miscdev.name  = MOD_NAME;
	miscdev.fops  = &file_ops;


	rval = misc_register(&miscdev);
	if (0 > rval) {
		printk(KERN_ERR MOD_NAME "%s: Failed registering device\n", MOD_NAME);
		goto err_register;
	}

	DPRINTF(MOD_NAME ": Device is %d bytes at base address 0x%lx\n",
		size, phys_addr);

	printk(KERN_NOTICE MOD_NAME
	       ": Installed VMIC NVRAM module version: %s\n", MOD_VERSION);

	return 0;

      err_register:
	iounmap(vminvr_base);
	return rval;
}


/*===========================================================================
 * Module exit routine
 */
static void __exit vminvr_exit(void)
{
	misc_deregister(&miscdev);

	iounmap(vminvr_base);

	printk(KERN_NOTICE MOD_NAME ": exiting vminvr module version: %s\n",
	       MOD_VERSION);
}


module_init(vminvr_init);
module_exit(vminvr_exit);
