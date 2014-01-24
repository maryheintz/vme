
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <linux/hwtimer.h>
#include <sys/ioctl.h>


/*===========================================================================
 * Test timer
 */
int test_timer(int fd, unsigned int count, int rate, int period)
{
        fd_set fds;
        int iterations, elapsed_time;
        time_t start_time, end_time;

        FD_ZERO(&fds);
        FD_SET(fd, &fds);

        /* Figure out how many iterations / period
         */
        iterations = (rate * 1000 * period) / count;

        /* Make sure the timer is stopped.
         */
        if (ioctl(fd, TIMER_STOP)) {
                perror("Error halting the timer\n");
                goto timer_error;
        }

        /* Set the timer count
         */
        if (-1 == write(fd, &count, sizeof (count))) {
                perror("Error setting count");
                goto timer_error;
        }

        if (ioctl(fd, TIMER_RATE_SET, &rate)) {
                perror("Error setting rate");
                goto timer_error;
        }

        if (ioctl(fd, TIMER_INTERRUPT_ENABLE)) {
                perror("Error enabling periodic interrupts\n");
                goto timer_error;
        }

        if (ioctl(fd, TIMER_START)) {
                perror("Error starting the timer\n");
                goto timer_error;
        }

        start_time = time(NULL);

        while (iterations--) {
                /* Alternate between using select and using blocking waits
                   every 4 intervals
                 */
                if (4 & iterations) {
                        if (-1 == select(fd + 1, NULL, NULL, &fds, NULL)) {
                                perror("Error waiting on interrupt");
                                goto timer_error;
                        }
                } else {
                        if (ioctl(fd, TIMER_INTERRUPT_WAIT)) {
                                perror("Error waiting for timer interrupt\n");
                                goto timer_error;
                        }
                }
        }

        end_time = time(NULL);

        if (ioctl(fd, TIMER_INTERRUPT_DISABLE)) {
                perror("Error disabling periodic interrupts\n");
                goto timer_error;
        }

        if (ioctl(fd, TIMER_STOP)) {
                perror("Error halting the timer\n");
                goto timer_error;
        }

        elapsed_time = (int) difftime(end_time, start_time);

        /* Obviously I'm not testing the timer for accuracy, I just want to
           know if the driver is programming it correctly. If this test is
           within +/-1 second, we probably got it correct.
         */
        if (((period + 1) < elapsed_time) || ((period - 1) > elapsed_time)) {
                fprintf(stderr, "This timer seems to be out of whack!\n");
                fprintf(stderr, "Elapsed time was %d seconds\n", elapsed_time);
                return -1;
        }

        return 0;

      timer_error:
        ioctl(fd, TIMER_INTERRUPT_DISABLE);
        ioctl(fd, TIMER_STOP);

        return -1;

}


/*===========================================================================
 * Main routine
 */
int main(int argc, char **argv)
{
        char filename[] = "/dev/timerx";
        int fd, timer, rate, rval, c;
        int period = 10;
        unsigned int count;

        while (-1 != (c = getopt(argc, argv, "hp:"))) {
                switch (c) {
                case 'p':
                        period = strtol(optarg, NULL, 0);
                        break;
                case 'h':
                default:
                        fprintf(stderr,
                                "USAGE: tmr_interval_test [-h]"
                                "[-p period in seconds]");
                        return -1;
                }
        }

        printf("Have patience. Each test takes %d seconds to run.\n", period);

        /* Test each of the four timers at each clock rate
         */
        for (timer = 1; timer <= 4; ++timer) {
                filename[10] = '0' + timer;
                fd = open(filename, O_RDWR);
                if (-1 == fd) {
                        fprintf(stderr, "open: %s: ", filename);
                        perror("");
                        return -1;
                }

                /* Timers 1 & 2 are 16 bits, timers 3 & 4 are 32 bits. We set
                   the count to expire every 25 milliseconds for timers 1 & 2
                   and every 2 seconds for timers 3 & 4.
                 */
                count = ((1 == timer) || (2 == timer)) ? 50000 : 4000000;
                /* With each pass, the frequency is halved, so we also halve
                   the count to avoid getting too few iterations.
                 */
                for (rate = 2000; rate >= 250; rate >>= 1, count >>= 1) {
                        rval = test_timer(fd, count, rate, period);
                        printf("Test timer %d at %d khz: %s\n", timer, rate,
                               (rval) ? "FAIL" : "PASS");
                }

                if (close(fd)) {
                        perror("close");
                        return -1;
                }
        }

        return 0;
}
