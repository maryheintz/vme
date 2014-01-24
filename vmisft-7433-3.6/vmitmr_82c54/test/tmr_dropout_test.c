
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <linux/hwtimer.h>
#include <sys/ioctl.h>


/*===========================================================================
 */
void print_usage(void)
{
	fprintf(stderr, "USAGE: tmr_dropout_test [-h][-c count][-r rate]"
		"[-t tics_per_run]\n"
		"          -c - test count value\n"
		"          -h - show help\n"
		"          -n - number of passes\n"
		"          -r - timer rate value in kilohertz\n"
		"          -t - tics per run\n");
}


/*===========================================================================
 * Assumptions: The reference count timeout must be greater than the test
 *              timeout.
 */
int main(int argc, char **argv)
{
	unsigned int test_count = 2000;
	unsigned int ref_count;
	int num_runs = 100;
	int tics_per_run = 25;
	int rate = 1000;
	int ref_fd, test_fd, nfds, tics, ii, c;
	int test_fail = 0;
	fd_set fds;

	while (-1 != (c = getopt(argc, argv, "c:hn:r:t:")))
		switch (c) {
		case 'c':
			test_count = strtol(optarg, NULL, 0);
			break;
		case 'n':
			num_runs = strtol(optarg, NULL, 0);
			break;
		case 'r':
			rate = strtol(optarg, NULL, 0);
			switch (rate) {
			case 1000:
			case 2000:
				break;
			default:
				print_usage();
				return -1;
			}
		case 't':
			tics_per_run = strtol(optarg, NULL, 0);
			break;
		case 'h':
		default:
				print_usage();
			return -1;
		}

	if (0xffff < test_count) {
		fprintf(stderr, "Value of count is too large\n");
		return -1;
	}

	ref_count = test_count * tics_per_run;
	if (0xffff < ref_count) {
		fprintf(stderr, "Value of reference count is too large\n");
		return -1;
	}

	fprintf(stderr, "Testing timer at %.2f hz\n",
		(rate * 1000) / (float) test_count);

	nfds = ref_fd = open("/dev/timer3", O_RDWR);
	write(ref_fd, &ref_count, sizeof (ref_count));
	ioctl(ref_fd, TIMER_INTERRUPT_ENABLE);

	test_fd = open("/dev/timer1", O_RDWR);
	if (test_fd > nfds)
		nfds = test_fd;
	write(test_fd, &test_count, sizeof (test_count));
	ioctl(test_fd, TIMER_INTERRUPT_ENABLE);

	ioctl(ref_fd, TIMER_START);
	ioctl(test_fd, TIMER_START);

	for (ii = 0; ii <= num_runs; ++ii) {
		tics = 0;
		do {
			FD_ZERO(&fds);
			FD_SET(ref_fd, &fds);
			FD_SET(test_fd, &fds);

			select(nfds + 1, NULL, NULL, &fds, NULL);
			if (FD_ISSET(test_fd, &fds)) {
				++tics;
			}
		}
		while (!FD_ISSET(ref_fd, &fds));

		/* Test ii == 0, throw the first pass out. Wait until we're
		   steady state.
		 */
		if (ii) {
			fprintf(stderr, "pass %d, actual tics = %d", ii, tics);
			if (tics != tics_per_run) {
				fprintf(stderr, " <= ERROR\n");
				++test_fail;
			} else {
				fprintf(stderr, "\n");
			}
		}
	}

	printf("Test for dropped interrupts: %s\n",
	       (test_fail) ? "FAIL" : "PASS");

	ioctl(test_fd, TIMER_INTERRUPT_DISABLE);
	ioctl(test_fd, TIMER_STOP);
	close(test_fd);

	ioctl(ref_fd, TIMER_INTERRUPT_DISABLE);
	ioctl(ref_fd, TIMER_STOP);
	close(ref_fd);

	return 0;
}
