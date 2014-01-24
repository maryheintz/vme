#
#==============================================================================
#                           COPYRIGHT NOTICE
#
#   Copyright (C) 2002-2005 GE Fanuc
#   International Copyright Secured.  All Rights Reserved.
#
#------------------------------------------------------------------------------
#   Description: Top level Makefile for vmisft-7433
#------------------------------------------------------------------------------
#
#==============================================================================
#


# Project variables
# 'uname -r' sets kernel revision to the currently running kernel revision
# KERNELSRC must point to the Linux kernel source code.
MAJOR:=3
MINOR:=6
VERSION:=$(MAJOR).$(MINOR)$(if $(PATCH),.)$(PATCH)
KERNELREV:=$(shell uname -r)
KERNELSRC:=/lib/modules/$(KERNELREV)/build
PROJNAME:=vmisft-7433
PATCHLEVEL:=$(shell uname -r | cut -d. -f2)


SUBDIRS24:=vme_universe \
         vmieep-7809 \
         vminvr \
         vmitmr_82c54 \
         vmitmrc \
         vmitmrf \
         vmitmr-7305 \
         vmitmr-7326 \
         vmivme \
         vmiwdtd \
         vmiwdtf \
         vmiwdt-7305 \
         vmiwdt-7326 \
         vmiwdt-7760 \
         vmiwdt-7806

SUBDIRS26:=vme_universe \
         vmieep-7809 \
         vminvr \
         vmitmr_82c54 \
         vmitmrc \
         vmitmrf \
         vmitmr-7305 \
         vmitmr-7326 \
         vmivme \
         vmiwdtd \
         vmiwdtf \
         vmiwdt-7305 \
         vmiwdt-7326 \
         vmiwdt-7760 \
         vmiwdt-7806

ifeq ($(PATCHLEVEL), 4)
SUBDIRS:=$(SUBDIRS24)
else
SUBDIRS:=$(SUBDIRS26)
endif


.PHONY: all install uninstall clean


all: $(KERNELSRC)/include/linux/module.h
	for directory in $(SUBDIRS); do \
                $(MAKE) -C $$directory $@; \
        done


install: $(KERNELSRC)/include/linux/module.h
	for directory in $(SUBDIRS); do \
                $(MAKE) -C $$directory $@; \
        done


uninstall:
	for directory in $(SUBDIRS); do \
                $(MAKE) -C $$directory $@; \
        done


clean:
	for directory in $(SUBDIRS); do \
                $(MAKE) -C $$directory $@; \
        done

$(KERNELSRC)/include/linux/module.h:
	$(error It appears that your kernel source is not installed. \
                To build this module, the kernel source must be accessible \
                via the path $(KERNELSRC).)

