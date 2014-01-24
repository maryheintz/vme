#
#==============================================================================
#                           COPYRIGHT NOTICE
#
#   Copyright (C) 2002-2005 GE Fanuc
#   International Copyright Secured.  All Rights Reserved.
#
#------------------------------------------------------------------------------
#   Description: Makefile for the GE Fanuc watchdog timer driver (2.6 Kernel)
#------------------------------------------------------------------------------
#
#==============================================================================
#


# Project variables
PROJNAME:=vmiwdtd
DEVFILE:=/dev/watchdog
DEVMAJOR:=10 
DEVMINOR:=130

ifneq ($(KERNELRELEASE),)
obj-m := vmiwdtd.o
CFLAGS_vmiwdtd.o += -DMOD_MAJOR=$(DEVMAJOR) -DMOD_NAME=\"$(PROJNAME)\" -DMOD_VERSION=\"$(MOD_VERSION)\"
else 

KDIR	:= /lib/modules/$(shell uname -r)/build
PWD	:= $(shell pwd) 

# Install paths
moduledir:= /lib/modules/$(shell uname -r)

all:
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) modules $@;

install: all $(DEVFILE)
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) modules_install
	/sbin/depmod

uninstall: clean
	rm -f $(moduledir)/extra/$(PROJNAME).*
	/sbin/depmod

default:
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) modules

# Major and minor device numbers and device names come from devices.txt
# in kernel source documentation
$(DEVFILE):
	mknod --mode=666 $@ c $(DEVMAJOR) $(DEVMINOR)

endif
