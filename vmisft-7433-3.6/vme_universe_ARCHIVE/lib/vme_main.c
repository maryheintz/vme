/*
===============================================================================
                            COPYRIGHT NOTICE

    Copyright (C) 2002-2004 GE Fanuc
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


#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include "vme/vme.h"
#include "vme/vme_api.h"


#define UNIVERSE_DEVICE  "/dev/bus/vme/ctl"


/*============================================================================
 * Map some physical address to a local virtual memory pointer
 */
void *vme_mmap_phys(int fd, unsigned long paddr, size_t size)
{
	unsigned long mapaddr, off, mapsize;
	size_t ps = getpagesize();
	char *ptr;

	/* Page align the memory mapping
	 */
	off = paddr % ps;
	mapaddr = paddr - off;
	mapsize = size + off;
	mapsize += (mapsize % ps) ? ps - (mapsize % ps) : 0;

	ptr = mmap(0, mapsize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, mapaddr);
	if (!ptr)
		return NULL;

	/* Return the pointer compenstated for offset from a page boundary
	 */
	return ptr + off;
}


/*============================================================================
 * Unmap a previously mapped physical address.
 */
int vme_unmap_phys(unsigned long vaddr, size_t size)
{
	unsigned long mapaddr, off, mapsize;
	size_t ps = getpagesize();

	/* Memory was mapped to a PAGE boundary, so we must offset back to that
	   boundary before unmapping it.
	 */
	off = vaddr % ps;
	mapaddr = vaddr - off;
	mapsize = size + off;
	mapsize += (mapsize % ps) ? ps - (mapsize % ps) : 0;

	return munmap((void *) mapaddr, mapsize);
}


/*============================================================================
 * Initialize the VMEbus driver interface.
 */
int vme_init(vme_bus_handle_t * handle)
{
	int if_version;

	if (NULL == handle) {
		errno = EINVAL;
		return -1;
	}

	*handle = (vme_bus_handle_t) open(UNIVERSE_DEVICE, O_RDWR);
	if (-1 == (int) *handle) {
		*handle = NULL;
		return -1;
	}

	if (ioctl((int) *handle, VMECTL_VERSION, &if_version)) {
		goto ERROR_INIT;
	}

	/* It should be OK if the lib has a lower minor number than the driver,
	   but since they are distributed together, there is no reason for them 
	   to be out of sync.
	 */
	if (VME_IF_VERSION != if_version) {
		errno = EPROTO;
		goto ERROR_INIT;
	}

	return 0;

	ERROR_INIT:
	close((int) handle);
	*handle = NULL;
	return -1;
}


/*============================================================================
 * Cleanup the VMEbus driver interface.
 */
int vme_term(vme_bus_handle_t handle)
{
	int status;

	if (NULL == handle) {
		errno = EINVAL;
		return -1;
	}

	status = close((int) handle);
	handle = NULL;
	return status;
}
