#
#==============================================================================
#                           COPYRIGHT NOTICE
#
#   Copyright (C) 2004-2005 GE Fanuc
#   International Copyright Secured.  All Rights Reserved.
#
#------------------------------------------------------------------------------
#   Description: Top level Makefile for the GE Fanuc watchdog timer driver
#------------------------------------------------------------------------------
#
#==============================================================================
#


# Project variables
# 'uname -r' sets kernel revision to the currently running kernel revision
# KERNELSRC must point to the Linux kernel source code.
MOD_MAJOR:=1
MOD_MINOR:=1
MOD_VERSION:=$(MOD_MAJOR).$(MOD_MINOR)$(if $(MOD_PATCH),.)$(MOD_PATCH)
KERNELREV:=$(shell uname -r)
KERNELSRC:=/lib/modules/$(KERNELREV)/build
PATCHLEVEL:=$(shell uname -r | cut -d. -f2)

export MOD_VERSION

ifeq ($(PATCHLEVEL), 4)
MOD_MAKE:=Makefile2.4
else
MOD_MAKE:=Makefile
endif


.PHONY: all install uninstall clean


all:  $(KERNELSRC)/include/linux/module.h
	$(MAKE) -C module -f $(MOD_MAKE) all;
	$(MAKE) -C test $@;


install: $(KERNELSRC)/include/linux/module.h
	$(MAKE) -C module -f $(MOD_MAKE) install;
	$(MAKE) -C test $@;


uninstall: $(KERNELSRC)/include/linux/module.h
	$(MAKE) -C module -f $(MOD_MAKE) uninstall;
	$(MAKE) -C test $@;


clean: $(KERNELSRC)/include/linux/module.h
	$(MAKE) -C module -f $(MOD_MAKE) clean;
	$(MAKE) -C test $@;


$(KERNELSRC)/include/linux/module.h:
	$(error It appears that your kernel source is not installed. \
                To build this module, the kernel source must be accessible \
                via the path $(KERNELSRC).)


# Major and minor device numbers and device names come from devices.txt
# in kernel source documentation
$(DEVFILE):
	mknod --mode=666 $@ c $(DEVMAJOR) $(DEVMINOR)

