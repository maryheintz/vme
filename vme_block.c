/*
 * vme.c
 * Allows simple multi-word or serial single word VME I/O from UNIX shell
 * (based on Pangburn's demo user code for FISION library)
 * modified by Wojtek Fedorko 2004/02 from code by:
 * WJA 3/99
 *
 * To compile on a Fermilab UNIX machine:
 *   setenv FISION_LIB $FISION_DIR/$fision_ld_path/$fision_mach_arch
 *   cc vme.c -o vme -L$FISION_LIB -lVISIONclient
 *
 * To compile on blue:
 *
 *   export FISION_LIB=/products/prd/fision/v2_15/Linux-2.2.16-3/i386
 *   gcc vme_block.c -o vme_block -L$FISION_LIB -lVISIONclient
 *
 *
 *  To use make a file named infile.txt in local directory.
 *  This file should have a word in hex on each line (and nothing else). 
 *  To write:
 *  ./vme <server> write <slot> <addr(hex)> <number of words> <mode>
 *  e.g.
 *  ./vme bilbo write 21 400000 15 b
 *  mode can be s -for a loop of single word or b -for block transfer.
 *  Reading exactly the same  e.g.:
 *  ./vme bilbo read 21 400000 15 b
 *
 *
 *
 *
 *
 *
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
  FILE* in_file;
  int retval = 0;
  int words_parsed_in=0;
  int iarg = 0;
  char *server = 0, *rw = 0,*mode =0;
  int slot = 0, addr = 0;
  char name[64] = "";
  int data[1000];
  int datasize=0;
  int status = 0;
  int i=0;
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
  if (++iarg>=argc) goto usage;
  datasize=atoi(argv[iarg]);
  if (++iarg>=argc) goto usage;
  mode=argv[iarg];

  if (datasize > 1000) goto usage;
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
    fprintf(stderr,"vme read from %s slot %d addr 0x%x\n", 
	    server, slot, addr);
    /*
     * Do the read
     */

    /* uncomment for debug
    printf("data array contents before read:\n");
    for(i=0;i<datasize;i++){
      printf("%d\t0x\%x\n",data[i],data[i]);
    }
    */

    assert(sizeof(value)==4);
    if (!strcmp(mode, "b")) {
      status = VISIONread(slave, addr, sizeof(value)*datasize, &nbytesread, data);
      fprintf(stderr,"read %d bytes\n",nbytesread);
      assert(nbytesread==sizeof(value)*datasize);
      if (status) {
	fprintf(stderr, 
		"VISIONread(addr=0x%x, size=%d) returns %d\n",
		addr, sizeof(value)*datasize, status);
	retval = -1;
      }
    } else if (!strcmp(mode, "s")){
      
      for(i=0;i<datasize;i++){
	status = VISIONread(slave, addr+(4*i), sizeof(value), &nbytesread, data+i);
	fprintf(stderr,"read %d bytes\n",nbytesread);
	assert(nbytesread==sizeof(value));
	if (status) {
	  fprintf(stderr, 
		  "VISIONread(addr=0x%x, size=%d) returns %d\n",
		  addr+(4*i), sizeof(value), status);
	  retval = -1;
	}
      }

    }
    
    for(i=0;i<datasize;i++){
      printf("%08x\n", data[i]);
    }

  } else if (!strcmp(rw, "write")) {
    int value = 0, nbyteswritten = 0;
     
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
        
    /* OPEN FILE FOR READING */
    printf("\n opening input file: ");
    if( (in_file=fopen("infile.txt", "r") ) ==NULL){
      printf("\nBUMMER: can't open the datafile !!!\n");
      return;
    } /* file must be open if we get here */
    printf(" -done\n");

    /* now fill in the data to write */

    
    while( !feof(in_file) && words_parsed_in < datasize ){
      fscanf(in_file, "%x", &data[words_parsed_in]);
      words_parsed_in++;
    }

    printf("\nNow I will write the following data:\n");
    for(i=0;i<datasize;i++){
      printf("%d\t0x%x\n",data[i],data[i]);
    }

    /*
     * Do the write
     */
    assert(sizeof(value)==4);
    if (!strcmp(mode, "b")) {
      status = VISIONwrite(slave, addr, sizeof(value)*datasize, &nbyteswritten,data);
      if (status) {
	fprintf(stderr, 
		"VISIONwrite(addr=0x%x, size=%d, value=0x%x) returns %d\n",
		addr, sizeof(value)*datasize, value, status);
	retval = -1;
      } else {
	printf("Written %d bytes\n",nbyteswritten);
      }
    } else if (!strcmp(mode, "s")){
      for(i=0;i<datasize;i++){
	status = VISIONwrite(slave, addr+(4*i), sizeof(value), &nbyteswritten,data+i);
	/*printf("addr %x\t%d\t0x%x\n",addr+(4*i),*(data+i),*(data+i));*/
	if (status) {
	  fprintf(stderr, 
		  "VISIONwrite(addr=0x%x, size=%d, value=0x%x) returns %d\n",
		  addr, sizeof(value)*datasize, value, status);
	  retval = -1;
	}
      }
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
  fprintf(stderr, "Usage: %s <server> read  <slot> <addr> <datasize(words)> <mode>\n", argv[0]);
  fprintf(stderr, "       %s <server> write <slot> <addr> <datasize(words)> <mode>\n", argv[0]);
  fprintf(stderr, "where slot datasize is decimal and addr is hex\n; datasize <1000;\n mode is:\n s -for single word accesses or\nb -for block transfer\n create infile.txt\nin local directory with a word on each line for 'write' operation\n");
  return -1;
}
