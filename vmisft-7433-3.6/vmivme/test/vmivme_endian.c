
/*
 * Test the VMEbus endian conversion
 *
 * Compile command: cc vmivme_endian.c -o vmivme_endian
 */


#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <linux/vmivme.h>


/*===========================================================================
 * Main routine
 */
int main(int argc, char **argv)
{
	int fd, c, rval = 0;

	if (-1 == (fd = open("/dev/vmivme", O_RDWR))) {
		perror("open");
		return -1;
	}

	while (-1 != (c = getopt(argc, argv, "m:s:"))) {
		switch (c) {
		case 'm':	/* Master endian conversion */
			/* no-zero value means enable */
			if (strtol(optarg, NULL, 0)) {
				if (ioctl(fd, VMIVME_MEC_ENABLE)) {
					perror("ioctl");
					rval = -1;
				}
			} else {
				if (ioctl(fd, VMIVME_MEC_DISABLE)) {
					perror("ioctl");
					rval = -1;
				}
			}
			break;
		case 's':	/* Slave endian conversion */
			/* non-zero value means enable */
			if (strtol(optarg, NULL, 0)) {
				if (ioctl(fd, VMIVME_SEC_ENABLE)) {
					perror("ioctl");
					rval = -1;
				}
			} else {	/*  0 means disable */

				if (ioctl(fd, VMIVME_SEC_DISABLE)) {
					perror("ioctl");
					rval = -1;
				}
			}
			break;
		default:
			fprintf(stderr,
				"USAGE: vmivme_endian [-m value][-s value]");
		}
	}

	if (close(fd)) {
		perror("close");
		rval = -1;
	}

	return rval;
}
