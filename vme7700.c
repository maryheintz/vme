#include <stdio.h>
#include <assert.h>
#include <vme/vme.h>
#include <vme/vme_api.h>
#include <stdio.h>

#define ADDRESS_MODIFIER  VME_A32SD
#define NBYTES            0x01

int
main(int argc, char **argv)
{
 char *rw = 0;
 int  addr = 0;
 int iarg = 0;
 int slot=0;
 
 if (++iarg>=argc) goto usage;
 rw = argv[iarg];
 if (++iarg>=argc) goto usage;
 sscanf(argv[iarg], "%d", &slot);
 
 if (++iarg>=argc) goto usage;
  sscanf(argv[iarg], "%x", &addr);
  addr=slot*(0x8000000) + addr;
 
 if (!strcmp(rw, "read")) {
    if (++iarg!=argc) goto usage;
    vme_bus_handle_t bus_handle;
	vme_master_handle_t window_handle;
	uint32_t *ptr;
    
    if (vme_init(&bus_handle)) {
		perror("Error initializing the VMEbus");
		return -1;
	}

	if (vme_master_window_create(bus_handle, &window_handle,
				     addr, ADDRESS_MODIFIER, NBYTES,
				     VME_CTL_PWEN, NULL)) {
		perror("Error creating the window");
		vme_term(bus_handle);
		return -1;
	}
    
    ptr = vme_master_window_map(bus_handle, window_handle, 0);
	if (!ptr) {
		perror("Error mapping the window");
		vme_master_window_release(bus_handle, window_handle);
		vme_term(bus_handle);
		return -1;
	}

    printf("%08x\n ", *ptr);

    if (vme_master_window_unmap(bus_handle, window_handle)) {
		perror("Error unmapping the window");
		vme_master_window_release(bus_handle, window_handle);
		vme_term(bus_handle);
		return -1;
	}

	if (vme_master_window_release(bus_handle, window_handle)) {
		perror("Error releasing the window");
		vme_term(bus_handle);
		return -1;
	}

	if (vme_term(bus_handle)) {
		perror("Error terminating");
		return -1;
	}
  }
 else if (!strcmp(rw, "write")) {
    int value=0;
    if (++iarg>=argc) goto usage;
  sscanf(argv[iarg], "%x", &value);
    if (++iarg!=argc) goto usage;

    vme_bus_handle_t bus_handle;
	vme_master_handle_t window_handle;
	uint32_t *ptr;
    
    if (vme_init(&bus_handle)) {
		perror("Error initializing the VMEbus");
		return -1;
	}

	if (vme_master_window_create(bus_handle, &window_handle,
				     addr, ADDRESS_MODIFIER, NBYTES,
				     VME_CTL_PWEN, NULL)) {
		perror("Error creating the window");
		vme_term(bus_handle);
		return -1;
	}
    
    ptr = vme_master_window_map(bus_handle, window_handle, 0);
	if (!ptr) {
		perror("Error mapping the window");
		vme_master_window_release(bus_handle, window_handle);
		vme_term(bus_handle);
		return -1;
	}

    *ptr=value;

    if (vme_master_window_unmap(bus_handle, window_handle)) {
		perror("Error unmapping the window");
		vme_master_window_release(bus_handle, window_handle);
		vme_term(bus_handle);
		return -1;
	}

	if (vme_master_window_release(bus_handle, window_handle)) {
		perror("Error releasing the window");
		vme_term(bus_handle);
		return -1;
	}

	if (vme_term(bus_handle)) {
		perror("Error terminating");
		return -1;
	}
  }
 else {
    goto usage;
  }
 return 0;
usage:
  fprintf(stderr, "Usage: %s read <slot number> <addr> \n", argv[0]);
  fprintf(stderr, "       %s write <slot number> <addr> <value>\n", argv[0]);
  fprintf(stderr, "where slot is dec, addr and value are hex\n");
  return -1;
}
  
    
    
       
    
    
 