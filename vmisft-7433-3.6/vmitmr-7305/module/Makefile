#
#==============================================================================
#                           COPYRIGHT NOTICE
#
#   Copyright (C) 2002-2005 GE Fanuc
#   International Copyright Secured.  All Rights Reserved.
#
#------------------------------------------------------------------------------
#   Description: Makefile for the GE Fanuc Timer driver (Linux 2.6 Kernel)
#------------------------------------------------------------------------------
#
#==============================================================================
#


# Project variables
PROJNAME:=vmitmr-7305
DEVFILE:=/dev/timer
DEVMAJOR:=231

ifneq ($(KERNELRELEASE),)
obj-m := vmitmr-7305.o
CFLAGS_vmitmr-7305.o += -I$(VMIINCPATH) -DMOD_MAJOR=$(DEVMAJOR) -DMOD_NAME=\"$(PROJNAME)\" -DMOD_VERSION=\"$(MOD_VERSION)\"

else

KDIR    := /lib/modules/$(shell uname -r)/build
PWD     := $(shell pwd) 

# Install paths
moduledir:= /lib/modules/$(shell uname -r)
includedir:= /usr/include

all:
	echo $(VMIINCPATH)
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) modules $@;

install: all $(DEVFILE)1 $(DEVFILE)2 $(DEVFILE)3 $(DEVFILE)4 
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) modules_install
	install -m0644 -oroot -groot ../include/linux/hwtimer.h \
		$(includedir)/linux/hwtimer.h
	/sbin/depmod

uninstall: clean
	rm -f $(includedir)/linux/hwtimer.h
	rm -f $(moduledir)/extra/$(PROJNAME).*
	/sbin/depmod

default: 
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) modules

$(DEVFILE)1:
	mknod --mode=666 $@ c $(DEVMAJOR) 1

$(DEVFILE)2:
	mknod --mode=666 $@ c $(DEVMAJOR) 2

$(DEVFILE)3:
	mknod --mode=666 $@ c $(DEVMAJOR) 3

$(DEVFILE)4:
	mknod --mode=666 $@ c $(DEVMAJOR) 4

endif
