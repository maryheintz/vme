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
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/reboot.h>
#include <linux/watchdog.h>
#include <linux/kernel.h>
#include <linux/version.h>


/* Older 2.4.x kernel versions did not have *TIMEOUT operations in "watchdog.h"
 */
#ifndef WDIOC_SETTIMEOUT
#define WDIOC_SETTIMEOUT        _IOWR(WATCHDOG_IOCTL_BASE, 6, int)
#define WDIOC_GETTIMEOUT        _IOR(WATCHDOG_IOCTL_BASE, 7, int)
#define WDIOF_SETTIMEOUT        0x0080
#endif /* WDIOC_SETTIMEOUT */


#ifdef DEBUG
#define DPRINTF(x...)  printk(KERN_DEBUG x)
#else
#define DPRINTF(x...)
#endif


#define VMIWDT_OSC                      0x09
#define VMIWDT_CMD                      0x0B
#define VMIWDT_TO                       0x0C
#define VMIWDT_ROUTING                  0x40

#define VMIWDT_OSC__EOSC                0x80
#define VMIWDT_OSC__ESQW                0x40
#define VMIWDT_CMD__TE                  0x80
#define VMIWDT_CMD__IPSW                0x40
#define VMIWDT_CMD__PULVL               0x10
#define VMIWDT_CMD__WAM                 0x08
#define VMIWDT_CMD__TDM                 0x04
#define VMIWDT_CMD__WAF                 0x02
#define VMIWDT_CMD__TDF                 0x01
#define VMIWDT_ROUTING__SERR            0x01
#define VMIWDT_ROUTING__RESET           0x02


MODULE_AUTHOR ("GE Fanuc <www.gefanuc.com/embedded>");
MODULE_DESCRIPTION ("GE Fanuc Watchdog Timer Driver");
#ifdef MODULE_LICENSE
MODULE_LICENSE ("Dual BSD/GPL");
#endif


static int vmiwdt_open (struct inode *inode, struct file *file_ptr);
static int vmiwdt_close (struct inode *inode, struct file *file_ptr);
static ssize_t vmiwdt_write (struct file *file_ptr, const char *buffer,
                             size_t nbytes, loff_t * off);
static int vmiwdt_ioctl (struct inode *inode, struct file *file_ptr,
                         unsigned int cmd, unsigned long arg);
static int vmiwdt_notify (struct notifier_block *notify, unsigned long event,
                          void *reserved);


static struct file_operations file_ops = {
  owner:THIS_MODULE,
  write:vmiwdt_write,
  ioctl:vmiwdt_ioctl,
  open:vmiwdt_open,
  release:vmiwdt_close
};

static void *vmiwdt_base = NULL;
static int vmiwdt_count = 0;
static char *version = MOD_VERSION;
static unsigned long phys_addr = 0;
static int reset = 1;
static int serr = 0;
static int level = 0;
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


MODULE_PARM (phys_addr, "i");
MODULE_PARM (reset, "i");
MODULE_PARM (serr, "i");
MODULE_PARM (level, "i");


/*============================================================================
 * Hook for notification of system events
 */
static int
vmiwdt_notify (struct notifier_block *notify, unsigned long event,
               void *reserved)
{
  /* Disable the timer upon shutdown or reboot
   */
  if ((SYS_DOWN == event) || (SYS_HALT == event))
    writeb (readb (vmiwdt_base + VMIWDT_CMD) | VMIWDT_CMD__WAM,
            vmiwdt_base + VMIWDT_CMD);
  writew (0, vmiwdt_base + VMIWDT_TO);

  return (NOTIFY_DONE);
}


/*============================================================================
 * Hook for the write file operation. Writing the the watchdog device performs
 * a keepalive.
 */
static ssize_t
vmiwdt_write (struct file *file_ptr, const char *buffer, size_t nbytes,
              loff_t * off)
{
  DPRINTF (MOD_NAME ": Keepalive\n");
  return (readb (vmiwdt_base + VMIWDT_TO));
}


/*============================================================================
 * Hook to the ioctl file operation
 */
int
vmiwdt_ioctl (struct inode *inode, struct file *file_ptr, uint32_t cmd,
              unsigned long arg)
{
  int timeout;
  uint16_t toval;
  volatile int keepalive;
  static struct watchdog_info wdt_info = {
    WDIOF_SETTIMEOUT,
    0,
    MOD_NAME
  };

  switch (cmd)
    {
    case WDIOC_GETSUPPORT:
      if (0 > copy_to_user ((void *) arg, &wdt_info, sizeof (wdt_info)))
        return (-EFAULT);
      break;
    case WDIOC_KEEPALIVE:
      keepalive = readb (vmiwdt_base + VMIWDT_TO);
      DPRINTF (MOD_NAME ": Keepalive\n");
      break;
    case WDIOC_SETTIMEOUT:
#ifdef WDIOC_SETTIMEOUT_MS
    case WDIOC_SETTIMEOUT_MS:
#endif
      if (0 > copy_from_user (&timeout, (void *) arg, sizeof (int)))
        return (-EFAULT);

      if (WDIOC_SETTIMEOUT == cmd)
        timeout *= 1000;

      /* Round up
       */
      timeout /= 10;
      timeout *= 10;

      if (timeout >= 100000)
        return (-EINVAL);

      toval = (((timeout / 10000) % 10) << 12) |
        (((timeout / 1000) % 10) << 8) | (((timeout / 100) % 10) << 4) |
        ((timeout / 10) % 10);

      writew (toval, vmiwdt_base + VMIWDT_TO);

      DPRINTF (MOD_NAME ": Timeout is now %dms\n", timeout);

      if (WDIOC_SETTIMEOUT == cmd)
        timeout /= 1000;

      if (0 > copy_to_user ((void *) arg, &timeout, sizeof (int)))
        return (-EFAULT);
      break;
    case WDIOC_GETTIMEOUT:
#ifdef WDIOC_GETTIMEOUT_MS
    case WDIOC_GETTIMEOUT_MS:
#endif
      toval = readw (vmiwdt_base + VMIWDT_TO);

      timeout = ((toval & 0x000f) * 10) + (((toval >> 4) & 0x000f) * 100) +
        (((toval >> 8) & 0x000f) * 1000) + (((toval >> 12) & 0x000f) * 10000);

      if (WDIOC_GETTIMEOUT == cmd)
        timeout /= 1000;

      if (0 > copy_to_user ((void *) arg, &timeout, sizeof (timeout)))
        return (-EFAULT);
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
vmiwdt_open (struct inode *inode, struct file *file_ptr)
{
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,5,0)
  MOD_INC_USE_COUNT;
#endif

  ++vmiwdt_count;

  /* If this is the first open, enable the timer.  The initial countdown value
     is 99.99sec
   */
  if (1 == vmiwdt_count)
    {
      writew (0x9999, vmiwdt_base + VMIWDT_TO);
      writeb (readb (vmiwdt_base + VMIWDT_CMD) & ~VMIWDT_CMD__WAM,
              vmiwdt_base + VMIWDT_CMD);
      DPRINTF (MOD_NAME ": Enabled watchdog timeout\n");
    }

  return (0);
}


/*============================================================================
 * Hook for the close file operation
 */
static int
vmiwdt_close (struct inode *inode, struct file *file_ptr)
{
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,5,0)
  MOD_DEC_USE_COUNT;
#endif

  --vmiwdt_count;

#ifndef CONFIG_WATCHDOG_NOWAYOUT
  if (!vmiwdt_count)
    {
      writeb (readb (vmiwdt_base + VMIWDT_CMD) | VMIWDT_CMD__WAM,
              vmiwdt_base + VMIWDT_CMD);
      writew (0, vmiwdt_base + VMIWDT_TO);
      DPRINTF (MOD_NAME ": Disabled watchdog timer\n");
    }
#endif /* CONFIG_WATCHDOG_NOWAYOUT */

  return (0);
}


/*============================================================================
 * Find and map a PCI watchdog device if present.
 */
static int
vmiwdt_find_pci_device (void)
{
  struct pci_dev *vmiwdt_pci_dev;
  u32 size;
  int bar;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
  if (!pci_present ())
    {
      printk (KERN_ERR MOD_NAME ": PCI bios not found\n");
      return (-1);
    }
#endif

  /* VMIVME-7696, VMIVME-7697, VMIVME-7697a */
  if ((vmiwdt_pci_dev = pci_find_device (PCI_VENDOR_ID_VMIC, 0x7696, NULL))
      || (vmiwdt_pci_dev =
          pci_find_device (PCI_VENDOR_ID_VMIC, 0x7697, NULL)))
    bar = PCI_BASE_ADDRESS_5;
  else                          /* No compatible device found */
    return (-1);

  /* Get the base address
   */
  pci_read_config_dword (vmiwdt_pci_dev, bar, (u32 *) & phys_addr);

  /* Determine the size
   */
  size = ~0;
  pci_write_config_dword (vmiwdt_pci_dev, bar, size);
  pci_read_config_dword (vmiwdt_pci_dev, bar, &size);
  pci_write_config_dword (vmiwdt_pci_dev, bar, phys_addr);
  size = ~(size & ~0xF) + 1;

  /* I know all of these devices are memory-mapped. If you add support for an
     I/O mapped device, fix this code. -DLH
   */
  phys_addr &= PCI_BASE_ADDRESS_MEM_MASK;

  if (NULL == (vmiwdt_base = ioremap_nocache (phys_addr, size)))
    {
      printk (KERN_ERR MOD_NAME
              ": Failure mapping the PCI watchdog device\n");
      return (-1);
    }

  /* These boards have an additional routing register. Whether or not a system
     reset or SERR is caused can be controlled by this register.
   */
  writeb (((reset) ? VMIWDT_ROUTING__RESET : 0) |
          ((serr) ? VMIWDT_ROUTING__SERR : 0), vmiwdt_base + VMIWDT_ROUTING);

  DPRINTF (MOD_NAME ": PCI watchdog device found\n");

  return (0);
}


/*============================================================================
 * Find and map a memory-mapped NVRAM device if present.
 */
static int
vmiwdt_find_memory_device (void)
{
  /* Test to see if this board has a DS1x84 or compatible device by
     looking for a valid board id.
   */

  /* VME boards
   */
  switch (isa_readw (0xd8016))
    {
    case 0x7591:
    case 0x7592:
    case 0x7695:
    case 0x7698:
    case 0x7740:
      phys_addr = 0xd8000;
      break;
    }

  /* CPCI boards
   */
  /* These boards don't have board id's, so we cannot probe for them. Use the
     module parameters to specify the address.
   */
#if 0
case 0x7593:
case 0x7594:
case 0x7696:
case 0x7697:
case 0x7710:
#endif
  if (!phys_addr)
    switch (isa_readw (0xd800e))
      {
      case 0x7699:
      case 0x7715:
      case 0x7716:
        phys_addr = 0xd8000;
        break;
      }

  if (!phys_addr)
    return (-1);

  if (NULL == (vmiwdt_base = ioremap_nocache (phys_addr, 14)))
    {
      printk (KERN_ERR MOD_NAME ": Failure mapping the watchdog device\n");
      return (-1);
    }

  DPRINTF (MOD_NAME ": Memory mapped watchdog device found\n");

  return (0);
}


/*===========================================================================
 * Module exit routine
 */
static void __exit
vmiwdt_exit(void)
{

  misc_deregister (&miscdev);

  /* Disable the device
   */
  writeb (readb (vmiwdt_base + VMIWDT_CMD) | VMIWDT_CMD__WAM,
          vmiwdt_base + VMIWDT_CMD);
  writew (0, vmiwdt_base + VMIWDT_TO);

  if (vmiwdt_base)
    iounmap (vmiwdt_base);

  unregister_reboot_notifier (&vmiwdt_notifier);

  printk (KERN_NOTICE MOD_NAME ": Exiting %s module version: %s\n", MOD_NAME,
          version);
}


/*===========================================================================
 * Module initialization routine
 */
static int __init
vmiwdt_init (void)
{
  if (phys_addr)
    {
      if (NULL == (vmiwdt_base = ioremap_nocache (phys_addr, 0x10)))
        {
          printk (KERN_ERR MOD_NAME
                  ": Failure mapping the watchdog device\n");
          return (-1);
        }
    }
  else
    {
      if ((0 != vmiwdt_find_pci_device ())
          && (0 != vmiwdt_find_memory_device ()))
        {
          printk (KERN_ERR MOD_NAME ": Not a supported board\n");
          return (-1);
        }
    }

  /* Initialize the watchdog device.
   */
  writeb (VMIWDT_CMD__TE | VMIWDT_CMD__TDM | VMIWDT_CMD__WAM |
          ((level) ? 0 : VMIWDT_CMD__PULVL), vmiwdt_base + VMIWDT_CMD);
  writew (0, vmiwdt_base + VMIWDT_TO);

  /* Disable the square wave signal and enable the oscillator.
   */
  writeb (readb (vmiwdt_base + VMIWDT_OSC) | VMIWDT_OSC__ESQW,
          vmiwdt_base + VMIWDT_OSC);
  writeb (readb (vmiwdt_base + VMIWDT_OSC) & ~VMIWDT_OSC__EOSC,
          vmiwdt_base + VMIWDT_OSC);

  register_reboot_notifier (&vmiwdt_notifier);

  if (0 > misc_register (&miscdev))
    {
      printk (KERN_ERR MOD_NAME ": Failed registering device\n");
      vmiwdt_exit ();
      return (-1);
    }

  printk (KERN_NOTICE MOD_NAME
          ": Installed VMIC watchdog timer module version: %s\n", version);

  return (0);
}


module_init (vmiwdt_init);
module_exit (vmiwdt_exit);
