
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <linux/types.h>
#include <linux/watchdog.h>
#include <sys/ioctl.h>


/* Older 2.4.x kernel versions did not have *TIMEOUT operations in "watchdog.h"
 */
#ifndef WDIOC_SETTIMEOUT
#define WDIOC_SETTIMEOUT        _IOWR(WATCHDOG_IOCTL_BASE, 6, int)
#define WDIOC_GETTIMEOUT        _IOR(WATCHDOG_IOCTL_BASE, 7, int)
#define WDIOF_SETTIMEOUT        0x0080
#endif				/* WDIOC_SETTIMEOUT */


/*===========================================================================
 * Main routine
 */
int main(int argc, char **argv)
{
	struct watchdog_info wdt_info;
	int c, fd, to = 33;

	while (-1 != (c = getopt(argc, argv, "s:"))) {
		switch (c) {
		case 's':	/* timeout in seconds */
			to = strtol(optarg, NULL, 0);
			break;
		default:
			fprintf(stderr, "USAGE: wd_test [-s seconds]");
		}
	}

	fd = open("/dev/watchdog", O_RDWR);
	if (-1 == fd) {
		perror("open");
		return -1;
	}

	if (ioctl(fd, WDIOC_GETSUPPORT, &wdt_info)) {
		perror("ioctl");
		goto error_exit;
	}

	printf("Watchdog module is %s\n", (char *) wdt_info.identity);
	printf("Firmware revision %d\n", wdt_info.firmware_version);
	printf("Supported options are:\n");
	if (0 == wdt_info.options)
		printf("  none\n");
	if (WDIOF_OVERHEAT & wdt_info.options)
		printf("  OVERHEAT\n");
	if (WDIOF_FANFAULT & wdt_info.options)
		printf("  FANFAULT\n");
	if (WDIOF_EXTERN1 & wdt_info.options)
		printf("  EXTERN1\n");
	if (WDIOF_EXTERN2 & wdt_info.options)
		printf("  EXTERN2\n");
	if (WDIOF_POWERUNDER & wdt_info.options)
		printf("  POWERUNDER \n");
	if (WDIOF_CARDRESET & wdt_info.options)
		printf("  CARDRESET\n");
	if (WDIOF_POWEROVER & wdt_info.options)
		printf("  POWEROVER\n");
	if (WDIOF_SETTIMEOUT & wdt_info.options)
		printf("  SETTIMEOUT\n");
	if (WDIOF_KEEPALIVEPING & wdt_info.options)
		printf("  KEEPALIVEPING\n");

	if (to) {
		if (ioctl(fd, WDIOC_SETTIMEOUT, &to)) {
			perror("ioctl");
			goto error_exit;
		}
	} else {
		if (ioctl(fd, WDIOC_GETTIMEOUT, &to)) {
			perror("ioctl");
			goto error_exit;
		}
	}

	/* Just testing the ioctl keepalive interface
	 */
	if (ioctl(fd, WDIOC_KEEPALIVE)) {
		perror("ioctl");
		goto error_exit;
	}

	printf("Watchdog will timeout in %d seconds\n", to);

	c = 0;
	while ('q' != c) {
		/* Write anything to the watchdog file to keepalive
		 */
		if (-1 == write(fd, &c, sizeof (c))) {
			perror("write");
			goto error_exit;
		}

		printf("Press <enter> to keepalive or 'q'<enter> to quit\n");
		c = getchar();
	}

	if (close(fd)) {
		perror("close");
		return -1;
	}

	return 0;

      error_exit:

	close(fd);
	return -1;
}
