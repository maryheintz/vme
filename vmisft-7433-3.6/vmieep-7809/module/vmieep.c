/*  This program is an example of writing and reading an EEPROM device via 
    SMBus on a GE Fanuc Embedded Systems, Inc. VMIVME-7809 Single Board
    Computer.

    To compile this program:
    
        gcc -O vmieep.c -o vmieep


    Before running this program, log in as root, then load the following 
    modules using:

       /sbin/modprobe i2c-core
       /sbin/modprobe i2c-dev
       /sbin/modprobe i2c-i801

   Loading the i2c-i801 module will create /dev/i2c-0, with 
   permissions = CRW- --- ---.  Either run the vmieep program as root, 
   or change the permissions to CRW- RW- RW- as shown:

       chmod 666 /dev/i2c-0

*/


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <sys/time.h>


/* The inline smbus function definitions may or may not be in i2c-dev.h,  
   depending on the Linux distribution.  Comment or uncomment the following 
   #include as necessary. */
#if 1
#  include "vmieep.h"
#endif


#define EEPROM_SIZE   256    /* Adjust for actual number of bytes in EEPROM */
#define EEPROM_SMBUS_ADDR  0xAC  /* Do NOT change! */

int gef_eeprom_read(int fd, unsigned char start_offset, unsigned char *buffer,
                    unsigned short buflen);
int gef_eeprom_write(int fd, unsigned char start_offset, unsigned char *buffer,
                    unsigned short buflen);
void gef_msec_delay(unsigned int msecs);


int main(int argc, char **argv)
{
    int fd;   /* File descriptor initialized with open() */
    int status; /* Return code from ioctl() */
    int adapter_num = 0; /* To match with /dev/i2c-0 */
    char filename[20];  /* Name of special device file */
    int i2c_addr = EEPROM_SMBUS_ADDR; /* SMBus address of EEPROM */
    unsigned short offset; /* Which byte to access in the EEPROM */
    unsigned short dataSize = 16; /* No more than EEPROM_SIZE */
    unsigned char rbuffer[16]; /* Data read from EEPROM */
    unsigned char wbuffer[16] = 
    {
        /* Data written to EEPROM */
        0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
        0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F
    };


    /* Open the special device file for the SMBus */
    sprintf(filename, "/dev/i2c-%d", adapter_num);
    fd = open(filename, O_RDWR);
    if (fd < 0)
    {
        printf("ERROR: open(%s) failed\n", filename);
        printf("errno = %d, %s\n", errno, strerror(errno));
        return -1;
    }
    printf("SUCCESS: open(%s) passed\n", filename);


    /* Specify the EEPROM as the device we want to access.
       *** IMPORTANT ***
       The address is actually in the 7 LSBs, so shift
       i2c_addr one bit to the right.*/
    status = ioctl(fd, I2C_SLAVE, i2c_addr>>1);
    if (status < 0)
    {
        printf("ERROR: ioctl(fd, I2C_SLAVE, 0x%02X) failed\n", i2c_addr);
        printf("errno = %d, %s\n", errno, strerror(errno));
        close(fd);
        return -1;
    }
    printf("SUCCESS: ioctl(fd, I2C_SLAVE, 0x%02X>>1) passed\n", i2c_addr);


    /* Read data from the EEPROM */
    offset = 0;
    if (gef_eeprom_read(fd, offset, rbuffer, dataSize) < 0)
    {
        printf("ERROR: gef_eeprom_read() failed\n");
        close(fd);
        return -1;
    }


    /* Dump the data read from the EEPROM */
    for (offset=0; offset<dataSize; offset++)
    {
        printf("Offset: %02d   Data: 0x%02X\n", offset, rbuffer[offset]);
    }


    /* Write data to the EEPROM */
    printf("\nPress ENTER to write to the EEPROM");
    getchar();
    offset = 0;
    if (gef_eeprom_write(fd, offset, wbuffer, dataSize) < 0)
    {
        printf("ERROR: gef_eeprom_write() failed\n");
        close(fd);
        return -1;
    }
    printf("SUCCESS: Data written to the EEPROM\n");


    /* Read data again from the EEPROM */
    printf("\nPress ENTER to read again from the EEPROM");
    getchar();
    offset = 0;
    if (gef_eeprom_read(fd, offset, rbuffer, dataSize) < 0)
    {
        printf("ERROR: gef_eeprom_read() failed\n");
        close(fd);
        return -1;
    }

    /* Dump the data read from the EEPROM */
    for (offset=0; offset<dataSize; offset++)
    {
        printf("Offset: %02d   Data: 0x%02X\n", offset, rbuffer[offset]);
    }
    printf("\n");


    /* Close the special device file */
    close(fd);

    return 0;
}


//////////////////////////////////////////////////////////////////////////////
//
// Function name : gef_eeprom_read
//
// Description   : Read buflen bytes from the EEPROM beginning at start_offset
//     
// Return type   : 0 for success, -1 for failure
//
// Argument      : int fd : File descriptor returned by open()
// Argument      : unsigned char start_offset : Read bytes starting at this
//                     offset in the EEPROM.  The sum of buflen and 
//                     start_offset must not exceed the maximum size in bytes 
//                     of the EEPROM
// Argument      : unsigned char *buffer : Where to store the bytes read
//                     from the EEPROM.  The buffer must be large enough
//                     to store buflen bytes read from the EEPROM.
// Argument      : unsigned short buflen : The size in bytes of buffer, or
//                     how many bytes to read from the EEPROM.  The sum of
//                     buflen and start_offset must not exceed the maximum
//                     size in bytes of the EEPROM.
//

int gef_eeprom_read(int fd, unsigned char start_offset, unsigned char *buffer,
                    unsigned short buflen)
{
    int offset, index;
    int data;

    for (index=0, offset=start_offset; index<buflen && 
        offset<EEPROM_SIZE; index++, offset++)
    {
        data = i2c_smbus_read_byte_data(fd, offset);
        if (data == -1)
        {
            printf("ERROR: i2c_smbus_read_byte_data(fd, 0x%02X) failed\n", 
                offset);
            printf("errno = %d, %s\n", errno, strerror(errno));
            return -1;
        }
        buffer[index] = (unsigned char) (data);
    }

    return 0;
}


//////////////////////////////////////////////////////////////////////////////
//
// Function name : gef_eeprom_write
//
// Description   : Write buflen bytes to the EEPROM beginning at start_offset
//     
// Return type   : 0 for success, -1 for failure
//
// Argument      : int fd : File descriptor returned by open()
// Argument      : unsigned char start_offset : Write bytes starting at this
//                     offset in the EEPROM.  The sum of buflen and 
//                     start_offset must not exceed the maximum size in bytes 
//                     of the EEPROM
// Argument      : unsigned char *buffer : Where to get the bytes to write
//                     to the EEPROM.  
// Argument      : unsigned short buflen : The size in bytes of buffer.  
//                     The sum of buflen and start_offset must not exceed the 
//                     maximum size in bytes of the EEPROM.
//

int gef_eeprom_write(int fd, unsigned char start_offset, unsigned char *buffer,
                    unsigned short buflen)
{
    int offset, index;
    int status;

    for (index=0, offset=start_offset; index<buflen && 
        offset<EEPROM_SIZE; index++, offset++)
    {
        status = i2c_smbus_write_byte_data(fd, offset, buffer[index]);
        if (status < 0)
        {
            printf("ERROR: i2c_smbus_write_byte_data(fd, 0x%02X, 0x%02X) failed\n", 
                offset, buffer[index]);
            printf("errno = %d, %s\n", errno, strerror(errno));
            return -1;
        }

        /* Delay while the byte write completes */
        gef_msec_delay(10);
    }

    return 0;
}


//////////////////////////////////////////////////////////////////////////////
//
// Function name : gef_msec_delay
//
// Description   : Delay for a number of milliseconds before returning
//     
// Return type   : void 
//
// Argument      : unsigned int msecs : The number of milliseconds to delay
//

void gef_msec_delay(unsigned int msecs)
{
    struct timeval s_current, s_start;
    struct timezone tz;
    unsigned int current, start;
    
    
    /* Get initial time */
    gettimeofday(&s_start, &tz);
    start = s_start.tv_sec*1000000 + s_start.tv_usec;
    
    /* Loop until msecs time have elapsed */
    do
    {
        gettimeofday(&s_current, &tz);
        current = s_current.tv_sec*1000000 + s_current.tv_usec;
    } while ((current-start) < (msecs*1000));
}



