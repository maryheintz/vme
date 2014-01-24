/*
 * vme.c
 * Allows simple single-word VME I/O from UNIX shell
 * (based on Pangburn's demo user code for FISION library)
 * by WJA 3/99
 *
 * To compile on a Fermilab UNIX machine:
 *   setenv FISION_LIB $FISION_DIR/$fision_ld_path/$fision_mach_arch
 *   cc vme.c -o vme -L$FISION_LIB -lVISIONclient
 */

#include <stdio.h>
#include <assert.h>

typedef void *VISION_SLAVE;
extern int VISIONopen(VISION_SLAVE *slavep, char *name, char *server);
extern int VISIONread(VISION_SLAVE slave, unsigned int offset, 
		      unsigned int req_bytes, unsigned int *real_bytesp,
		      void *data);
extern int VISIONwrite(VISION_SLAVE slave, unsigned int offset,
		       unsigned int req_bytes, unsigned int *real_bytesp,
		       void *data);

int
main(int argc, char **argv)
{
  int retval = 0;
  int iarg = 0;
  char *server = 0, *rw = 0;
  int slot = 0, addr = 0;
  char name[64] = "";
  int status = 0;
  VISION_SLAVE slave;
  /*
   * Parse arg list for server, read/write, slot, address
   */
  if (++iarg>=argc) goto usage;
  server = argv[iarg];
  if (++iarg>=argc) goto usage;
  rw = argv[iarg];
  if (++iarg>=argc) goto usage;
  slot = atoi(argv[iarg]);
  if (++iarg>=argc) goto usage;
  sscanf(argv[iarg], "%x", &addr);
  /*
   * Construct a VISION handle to the server
   */
  sprintf(name, "geo32:/slot=%d", slot);
  status = VISIONopen(&slave, name, server);
  if (status) {
    fprintf(stderr, 
	    "VISIONopen(name=\"%s\", server=\"%s\") returns %d\n",
	    name, server, status);
    return -1;
  }
  if (!strcmp(rw, "read")) {
    int value = 0, nbytesread = 0;
    /*
     * Check for extra arguments
     */
    if (++iarg!=argc) goto usage;
    /*
     * Debug message
     */
    fprintf(stderr, 
	    "vme read from %s slot %d addr 0x%x\n", 
	    server, slot, addr);
    /*
     * Do the read
     */
    assert(sizeof(value)==4);
    status = VISIONread(slave, addr, sizeof(value), &nbytesread, &value);
    assert(nbytesread==sizeof(value));
    if (status) {
      fprintf(stderr, 
	      "VISIONread(addr=0x%x, size=%d) returns %d\n",
	      addr, sizeof(value), status);
      retval = -1;
    } else {
      printf("%d 0x%x\n", value, value);
    }
  } else if (!strcmp(rw, "write")) {
    int value = 0, nbyteswritten = 0;
    /*
     * Parse next argument for value to write
     */
    if (++iarg>=argc) goto usage;
    sscanf(argv[iarg], "%x", &value);
    /*
     * Check for extra arguments
     */
    if (++iarg!=argc) goto usage;
    /*
     * Debug message
     */
    fprintf(stderr,
	    "vme write to %s slot %d addr 0x%x, value 0x%x\n",
	    server, slot, addr, value);
    /*
     * Do the write
     */
    assert(sizeof(value)==4);
    status = VISIONwrite(slave, addr, sizeof(value), &nbyteswritten, &value);
    if (status) {
      fprintf(stderr, 
	      "VISIONwrite(addr=0x%x, size=%d, value=0x%x) returns %d\n",
	      addr, sizeof(value), value, status);
      retval = -1;
    }
  } else {
    goto usage;
  }
  /*
   * Destroy the VISION handle
   */
  status = VISIONclose(slave);
#if 0
  if (status) {
    fprintf(stderr, "VISIONclose() returns %d\n", status);
    retval = -1;
  }
#endif
  return retval;
 usage:
  fprintf(stderr, "Usage: %s <server> read  <slot> <addr>\n", argv[0]);
  fprintf(stderr, "       %s <server> write <slot> <addr> <value>\n", argv[0]);
  fprintf(stderr, "where slot is decimal and addr, value are hex\n");
  return -1;
}
