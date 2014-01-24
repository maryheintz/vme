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
#include <linux/kernel.h>
#include <linux/version.h>
#include "vmivme.h"


#ifdef DEBUG
#define DPRINTF(x...)  printk(KERN_DEBUG x)
#else
#define DPRINTF(x...)
#endif


#define VMIVME_COMM__VME_EN                0x0800
#define VMIVME_COMM__BTOV                  0x0030
#define VMIVME_COMM__BTOV__1000            0x0030
#define VMIVME_COMM__BTOV__256             0x0020
#define VMIVME_COMM__BTOV__64              0x0010
#define VMIVME_COMM__BTOV__16              0x0000
#define VMIVME_COMM__BTO                   0x0080
#define VMIVME_COMM__ABLE                  0x0004
#define VMIVME_COMM__SEC                   0x0002
#define VMIVME_COMM__MEC                   0x0001

#define VMIVMEP_COMM__MB_M3                0x0400
#define VMIVMEP_COMM__MB_M2                0x0200
#define VMIVMEP_COMM__MB_M1                0x0100
#define VMIVMEP_COMM__MB_M0                0x0080
#define VMIVMEP_COMM__BERRIM               0x0040

#define VMIVMEI_COMM__BYPASS               0x0400
#define VMIVMEI_COMM__WTDSYS               0x0100
#define VMIVMEI_COMM__BERRST               0x0080
#define VMIVMEI_COMM__BERRI                0x0040


static int vmivme_open (struct inode *inode, struct file *file_ptr);
static int vmivme_close (struct inode *inode, struct file *file_ptr);
static int vmivme_ioctl (struct inode *inode, struct file *file_ptr,
                         unsigned int cmd, unsigned long arg);

static struct file_operations file_ops = {
  owner:THIS_MODULE,
  ioctl:vmivme_ioctl,
  open:vmivme_open,
  release:vmivme_close
};

static void *vmivme_base = NULL;
static void *vmivme_comm;
static char *version = MOD_VERSION;
static rwlock_t vmivme_rwlock = RW_LOCK_UNLOCKED;
static int comm;
static struct miscdevice miscdev = {
  MOD_MINOR,
  MOD_NAME,
  &file_ops
};


MODULE_AUTHOR ("GE Fanuc <www.gefanuc.com/embedded>");
MODULE_DESCRIPTION ("GE Fanuc VMEbus extensions Driver");
#ifdef MODULE_LICENSE
MODULE_LICENSE ("Dual BSD/GPL");
#endif
MODULE_PARM (comm, "i");


/*============================================================================
 * Enable master endian conversion
 */
static void
vme_mec_enable ( void )
{
  write_lock (&vmivme_rwlock);
  writew (readw (vmivme_comm) | VMIVME_COMM__MEC, vmivme_comm);
  write_unlock (&vmivme_rwlock);
}


/*============================================================================
 * Disable master endian conversion
 */
static void
vme_mec_disable ( void )
{
  write_lock (&vmivme_rwlock);
  writew (readw (vmivme_comm) & ~VMIVME_COMM__MEC, vmivme_comm);
  write_unlock (&vmivme_rwlock);
}


/*============================================================================
 * Enable slave endian conversion
 */
static void
vme_sec_enable ( void )
{
  write_lock (&vmivme_rwlock);
  writew (readw (vmivme_comm) | VMIVME_COMM__SEC, vmivme_comm);
  write_unlock (&vmivme_rwlock);
}


/*============================================================================
 * Disable slave endian conversion
 */
static void
vme_sec_disable ( void )
{
  write_lock (&vmivme_rwlock);
  writew (readw (vmivme_comm) & ~VMIVME_COMM__SEC, vmivme_comm);
  write_unlock (&vmivme_rwlock);
}


/*============================================================================
 * Find the PLX device if present, and configure the VME registers
 */
static int
vmic_init_plx ( void )
{
  struct pci_dev *vmic_pci_dev;
  uint32_t base_addr;

  if ((vmic_pci_dev = pci_find_device (PCI_VENDOR_ID_VMIC, 0x0001, NULL)))
    {
      pci_read_config_dword (vmic_pci_dev, 0x18, &base_addr);

      if (NULL == (vmivme_base = ioremap_nocache (base_addr, 12)))
        {
          printk (KERN_ERR MOD_NAME ": Failure mapping the VMIC registers\n");
          return (-1);
        }

      vmivme_comm = vmivme_base;
      return (0);
    }

  return (-1);
}


/*============================================================================
 * Find the FPGA device if present, and configure the VME registers
 */
static int
vmic_init_fpga ( void )
{
  struct pci_dev *vmic_pci_dev;
  uint32_t base_addr;
  uint16_t board_id;

  if ((vmic_pci_dev = pci_find_device (PCI_VENDOR_ID_VMIC, 0x0004, NULL))
      || (vmic_pci_dev = pci_find_device (PCI_VENDOR_ID_VMIC, 0x0005, NULL))
      || (vmic_pci_dev = pci_find_device (PCI_VENDOR_ID_VMIC, 0x0006, NULL)))
    {
      pci_read_config_dword (vmic_pci_dev, 0x10, &base_addr);

      if (NULL == (vmivme_base = ioremap_nocache (base_addr, 12)))
        {
          printk (KERN_ERR MOD_NAME ": Failure mapping the VMIC registers\n");
          return (-1);
        }

      pci_read_config_word (vmic_pci_dev, PCI_SUBSYSTEM_ID,
                            (uint16_t *) & board_id);
      DPRINTF (MOD_NAME ": Board is VMIVME-%x\n", board_id);

      vmivme_comm = vmivme_base;
      return (0);
    }

  return (-1);
}


/*============================================================================
 * Find the ISA device if present, and configure the VME registers
 */
static int
vmic_init_isa ( void )
{
  uint16_t board_id;

  if (NULL == (vmivme_base = ioremap_nocache (0xd8000, 0x18)))
    {
      printk (KERN_ERR MOD_NAME ": Failure mapping the VMIC registers\n");
      return (-1);
    }

  /* Test to see if this boad has extension registers in I/O space by looking
     for a valid board id.  Starting with the 7750, these registers have moved
     to the FPGA part, so hopefully, this list will remain complete.
   */
  board_id = readw (vmivme_base + 0x16);
  switch (board_id)
    {
    case 0x7591:
    case 0x7592:
    case 0x7695:
    case 0x7696:
    case 0x7697:
    case 0x7698:
    case 0x7740:
      DPRINTF (MOD_NAME ": Board is VMIVME-%x\n", board_id);
      break;
    default:
      iounmap (vmivme_base);

      /* Make sure vmivme_base is NULL so the cleanup function doesn't try to
         unmap memory that it shouldn't.
       */
      vmivme_base = NULL;
      return (-1);
      break;
    }

  vmivme_comm = vmivme_base + 0x0e;
  return (0);
}


/*===========================================================================
 * Locate the VMIC registers if present and initialize them.
 */
int
vmic_init ( void )
{

  if (0 == vmic_init_plx ())
    {
      DPRINTF (MOD_NAME ": VMIC PLX device found and initialized\n");
      return (0);
    }

  if (0 == vmic_init_fpga ())
    {
      DPRINTF (MOD_NAME ": VMIC FPGA device found and initialized\n");
      return (0);
    }

  if (0 == vmic_init_isa ())
    {
      DPRINTF (MOD_NAME ": VMIC ISA device found and initialized\n");
      return (0);
    }

  return (-1);
}



/*============================================================================
 * Hook to the ioctl file operation
 */
static int
vmivme_ioctl (struct inode *inode, struct file *file_ptr, uint32_t cmd,
              unsigned long arg)
{
  switch (cmd)
    {
    case VMIVME_MEC_ENABLE:
      vme_mec_enable ();
      break;
    case VMIVME_MEC_DISABLE:
      vme_mec_disable ();
      break;
    case VMIVME_SEC_ENABLE:
      vme_sec_enable ();
      break;
    case VMIVME_SEC_DISABLE:
      vme_sec_disable ();
      break;
    default:
      return (-ENOTTY);
    }

  return (0);
}


/*============================================================================
 * Hook for the open file operation
 */
static int
vmivme_open (struct inode *inode, struct file *file_ptr)
{
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,5,0)
  MOD_INC_USE_COUNT;
#endif

  return (0);
}


/*============================================================================
 * Hook for the close file operation
 */
static int
vmivme_close (struct inode *inode, struct file *file_ptr)
{
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,5,0)
  MOD_DEC_USE_COUNT;
#endif

  return (0);
}


/*===========================================================================
 * Module exit routine
 */
static void __exit
vmivme_exit ( void )
{
  misc_deregister (&miscdev);

  if (vmivme_base)
    iounmap (vmivme_base);

  printk (KERN_NOTICE MOD_NAME ": Exiting %s module version: %s\n", MOD_NAME,
          version);
}


/*===========================================================================
 * Module initialization routine
 */
static int __init
vmivme_init ( void )
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
  if (!pci_present ())
    {
      printk (KERN_ERR MOD_NAME ": PCI bios not found\n");
      return (-1);
    }
#endif

  if (0 > vmic_init ())
    {
      printk (KERN_ERR MOD_NAME
              ":Unable to initialize VMIC VMEbus extension registers\n");
      return (-1);
    }

      if (comm)
        writew (comm, vmivme_comm);
      else                      /* default */
        writew (VMIVME_COMM__MEC | VMIVME_COMM__SEC | VMIVME_COMM__VME_EN,
                vmivme_comm);

  if (0 > misc_register (&miscdev))
    {
      printk (KERN_ERR MOD_NAME ": Failed registering device\n");
      vmivme_exit ();
      return (-1);
    }

  printk (KERN_NOTICE MOD_NAME
          ": Installed VMIC VMEbus extensions module version: %s\n", version);

  return (0);
}


module_init (vmivme_init);
module_exit (vmivme_exit);
