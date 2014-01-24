
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <linux/hwtimer.h>
#include <sys/ioctl.h>


static const int period = 10;   /* The amount of time we are going to
                                   measure in seconds. */


/*===========================================================================
 * Test timer
 */
int test_timer(int fd, unsigned int count, int rate)
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

        /* Just making sure that the read function is working. Don't try to
           compare this value to the value you just wrote. We write to the
           load count register, and read from the current count register, but
           the current count register won't be updated until the timer is
           actually running.
         */
        if (-1 == read(fd, &count, sizeof (count))) {
                perror("Error reading current count");
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
                   every 4 iterations.
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
        int fd, timer, c, rate, rval;

        rate = 1000;            /* Default clock rate */

        while (-1 != (c = getopt(argc, argv, "r:"))) {
                switch (c) {
                case 'r':      /* Clockrate in khz */
                        rate = strtol(optarg, NULL, 0);
                        switch (rate) {
                        case 1000:
                        case 2000:
                                break;
                        default:
                                fprintf(stderr, "Invalid clock rate\n");
                                return -1;
                        }
                        break;
                default:
                        fprintf(stderr, "USAGE: tmr_test [-r khz]");
                }
        }

        printf("Have patience. Each test takes %d seconds to run.\n", period);

        /* Test each of the three timers at each clock rate
         */
        for (timer = 1; timer <= 3; ++timer) {
                filename[10] = '0' + timer;
                fd = open(filename, O_RDWR);
                if (-1 == fd) {
                        fprintf(stderr, "open: %s: ", filename);
                        perror("");
                        return -1;
                }

                rval = test_timer(fd, 50000, rate);
                printf("Test timer %d at %d khz: %s\n", timer, rate,
                       (rval) ? "FAIL" : "PASS");

                if (close(fd)) {
                        perror("close");
                        return -1;
                }
        }

        return 0;
}
