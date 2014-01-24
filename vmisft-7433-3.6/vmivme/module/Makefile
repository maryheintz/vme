#
#==============================================================================
#                           COPYRIGHT NOTICE
#
#   Copyright (C) 2002-2005 GE Fanuc
#   International Copyright Secured.  All Rights Reserved.
#
#------------------------------------------------------------------------------
#   Description: Makefile for the VMEbus extenstion registers (2.6 Kernel)
#------------------------------------------------------------------------------
#
#==============================================================================
#


# Project variables
PROJNAME:=vmivme
DEVFILE:=/dev/vmivme
DEVMAJOR:=10
DEVMINOR:=255   # 255 means dynamically allocate a minor number

ifneq ($(KERNELRELEASE),)
obj-m	:= vmivme.o
CFLAGS_vmivme.o += -DMOD_MAJOR=$(DEVMAJOR) -DMOD_NAME=\"$(PROJNAME)\" -DMOD_VERSION=\"$(MOD_VERSION)\" -DMOD_MINOR=$(DEVMINOR)

else

KDIR	:= /lib/modules/$(shell uname -r)/build
PWD	:= $(shell pwd)

# Install paths
moduledir:= /lib/modules/$(shell uname -r)
includedir:= /usr/include

all:
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) modules $@;

install: all $(DEVFILE)
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) modules_install
	install -m0644 -oroot -groot vmivme.h \
		$(includedir)/linux/vmivme.h
	/sbin/depmod

uninstall: clean
	rm -f $(includedir)/linux/vmivme.h
	rm -f $(moduledir)/extra/$(PROJNAME).*
	/sbin/depmod

default:
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) modules

endif
