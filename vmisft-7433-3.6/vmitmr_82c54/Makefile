#
#==============================================================================
#                           COPYRIGHT NOTICE
#
#   Copyright (C) 2002-2005 GE Fanuc
#   International Copyright Secured.  All Rights Reserved.
#
#------------------------------------------------------------------------------
#   Description: Top level Makefile for the GE Fanuc hadware timer driver
#------------------------------------------------------------------------------
#
#==============================================================================
#


# Project variables
# 'uname -r' sets kernel revision to the currently running kernel revision
# KERNELSRC must point to the Linux kernel source code.

MOD_MAJOR:=1
MOD_MINOR:=3
MOD_VERSION:=$(MOD_MAJOR).$(MOD_MINOR)$(if $(MOD_PATCH),.)$(MOD_PATCH)
KERNELREV:=$(shell uname -r)
KERNELSRC:=/lib/modules/$(KERNELREV)/build
PROJNAME:=vmitmr_82c54
PATCHLEVEL:=$(shell uname -r | cut -d. -f2)
VMIINCPATH:=$(shell pwd)/include

export MOD_VERSION
export VMIINCPATH

ifeq ($(PATCHLEVEL), 4)
MOD_MAKE:=Makefile2.4
else
MOD_MAKE:=Makefile
endif


.PHONY: all install uninstall clean


all: $(KERNELSRC)/include/linux/module.h
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

$(DEVFILE)1:
	mknod --mode=666 $@ c $(DEVMAJOR) 1

$(DEVFILE)2:
	mknod --mode=666 $@ c $(DEVMAJOR) 2

$(DEVFILE)3:
	mknod --mode=666 $@ c $(DEVMAJOR) 3

