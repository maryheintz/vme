
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <linux/hwtimer.h>
#include <sys/ioctl.h>


/*===========================================================================
 * Main routine
 */
int main()
{
	int fd1, fd2, count, ii;
	fd_set fds;
	int max_count = 0xffff;	/* This is a 16-bit counter */
	int rate = 1000;
	float num_passes = 100.0;

	/* Set up the timers
	 */
	if (-1 == (fd1 = open("/dev/timer1", O_RDWR))) {
		perror("open");
		return -1;
	}

	if (-1 == (fd2 = open("/dev/timer2", O_RDWR))) {
		perror("open");
		return -1;
	}

	ioctl(fd1, TIMER_RATE_SET, &rate);
	ioctl(fd2, TIMER_RATE_SET, &rate);

	write(fd2, &max_count, sizeof (max_count));
	ioctl(fd2, TIMER_START);

	/* Start a countdown, read the timer num_passes times, then find the
	   average read access time.
	   WARNING: Rollover is not accounted for, so don't make num_passes
	   too big.
	 */
	ioctl(fd1, TIMER_STOP);
	write(fd1, &max_count, sizeof (max_count));
	ioctl(fd1, TIMER_START);

	for (ii = 1; ii <= num_passes; ++ii) {
		read(fd1, &count, sizeof (count));
	}
	printf("Read access time is %.2fus\n",
	       (max_count - count) / num_passes);

	/* Start a countdown, write to a different timer num_passes times,
	   then find the average write access time.
	 */
	FD_ZERO(&fds);
	FD_SET(fd2, &fds);

	ioctl(fd1, TIMER_STOP);
	write(fd1, &max_count, sizeof (max_count));
	ioctl(fd1, TIMER_START);

	for (ii = 1; ii <= num_passes; ++ii) {
		write(fd2, &count, sizeof (count));
	}
	read(fd1, &count, sizeof (count));
	printf("Write access time is %.2fus\n",
	       (max_count - count) / num_passes);

	ioctl(fd1, TIMER_STOP);

	/* Start a countdown, then poll a timer with the select function.
	   Then find the average select access time.
	 */
	FD_ZERO(&fds);
	FD_SET(fd2, &fds);

	ioctl(fd1, TIMER_STOP);
	write(fd1, &max_count, sizeof (max_count));
	ioctl(fd1, TIMER_START);

	for (ii = 1; ii <= num_passes; ++ii) {
		select(fd2 + 1, &fds, NULL, NULL, NULL);
	}
	read(fd1, &count, sizeof (count));
	printf("Select access time is %.2fus\n",
	       (max_count - count) / num_passes);

	/* Cleanup
	 */
	ioctl(fd1, TIMER_STOP);
	ioctl(fd2, TIMER_STOP);

	close(fd1);
	close(fd2);

	return 0;
}
