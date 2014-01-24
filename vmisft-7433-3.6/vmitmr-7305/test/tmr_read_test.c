
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <linux/hwtimer.h>
#include <sys/ioctl.h>


int test_count(int fd, unsigned int *last_count)
{
        unsigned int current_count;

        if (-1 == read(fd, &current_count, sizeof (current_count))) {
                perror("read");
                return -1;
        }

        if (*last_count < current_count) {
                fprintf(stderr,
                        "We've gone backwards in time!\n"
                        "Last count was 0x%x current count is 0x%x\n",
                        *last_count, current_count);
                return -1;
        }

        *last_count = current_count;

        return 0;
}


/*===========================================================================
 * Main routine
 */
int main(int argc, char **argv)
{
        unsigned int short_count = 0x1000;
        unsigned int long_count = 0x9000;
        int long_fd, short_fd, nfds, tics, expected_interrupts, c;
	int rate = 1000;
        int result;
        fd_set fds;

        while (-1 != (c = getopt(argc, argv, "hs:l:"))) {
                switch (c) {
                case 's':
                        short_count = strtol(optarg, NULL, 0);
                        break;
                case 'l':
                        long_count = strtol(optarg, NULL, 0);
                        break;
                case 'h':
                default:
                        fprintf(stderr,
                                "USAGE: tmr_read_test [-h]"
                                "[-s short interval timer count]"
                                "[-l long interval timer count]");
                        return -1;
                }
        }

        nfds = long_fd = open("/dev/timer3", O_RDWR);
        write(long_fd, &long_count, sizeof (long_count));
        ioctl(long_fd, TIMER_INTERRUPT_ENABLE);

        short_fd = open("/dev/timer1", O_RDWR);
        if (short_fd > nfds)
                nfds = short_fd;
        write(short_fd, &short_count, sizeof (short_count));
        ioctl(short_fd, TIMER_INTERRUPT_ENABLE);

        ioctl(long_fd, TIMER_RATE_SET, &rate);
        ioctl(short_fd, TIMER_RATE_SET, &rate);

        ioctl(long_fd, TIMER_START);
        ioctl(short_fd, TIMER_START);

        tics = 0;
        expected_interrupts = long_count / (short_count + 1);
        while (1) {
                FD_ZERO(&fds);
                FD_SET(long_fd, &fds);
                FD_SET(short_fd, &fds);

                select(nfds + 1, NULL, NULL, &fds, NULL);

                if (FD_ISSET(long_fd, &fds)) {
                        break;
                }

                if (FD_ISSET(short_fd, &fds)) {
                        ++tics;
                        result = test_count(long_fd, &long_count);
                        if (result) {
                                goto error;
                        }
                }
        }

        if (tics != expected_interrupts) {
                fprintf(stderr,
                        "Expected %d interrupts, received %d interrupts\n",
                        expected_interrupts, tics);
                result = -1;
        }

      error:
        printf("Test for correct timer readback: %s\n",
               (result) ? "FAIL" : "PASS");

        ioctl(short_fd, TIMER_INTERRUPT_DISABLE);
        ioctl(short_fd, TIMER_STOP);
        close(short_fd);

        ioctl(long_fd, TIMER_INTERRUPT_DISABLE);
        ioctl(long_fd, TIMER_STOP);
        close(long_fd);

        return result;
}
