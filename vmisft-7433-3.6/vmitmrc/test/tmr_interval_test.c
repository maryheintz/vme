
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
        const int rate = 1000;
        char filename[] = "/dev/timerx";
        int fd, timer, rval, c;
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

        /* Test each of the three timers at each clock
         */
        for (timer = 1; timer <= 3; ++timer) {
                filename[10] = '0' + timer;
                fd = open(filename, O_RDWR);
                if (-1 == fd) {
                        fprintf(stderr, "open: %s: ", filename);
                        perror("");
                        return -1;
                }

                /* This timer has some strange quirky way of loading the count.
                   Be sure to test all cases
                 */

                /* This is a standard 16-bit test value
                 */
                count = 0x10001;
                rval = test_timer(fd, count, rate, period);
                printf("Test timer %d with count %d: %s\n", timer, count,
                       (0 == rval) ? "PASS" : "FAIL");

                /* This tests the 16-bit rollover case
                 */
                count = 0x10002;
                rval = test_timer(fd, count, rate, period);
                printf("Test timer %d with count %d: %s\n", timer, count,
                       (0 == rval) ? "PASS" : "FAIL");

                /* This is a standard 32-bit test value
                 */
                count = 0x10003;
                rval = test_timer(fd, count, rate, period);
                printf("Test timer %d with count %d: %s\n", timer, count,
                       (0 == rval) ? "PASS" : "FAIL");

                /* This tests the 32-bit rollover case
                 */
                count = 0x20002;
                rval = test_timer(fd, count, rate, period);
                printf("Test timer %d with count %d: %s\n", timer, count,
                       (0 == rval) ? "PASS" : "FAIL");

                if (close(fd)) {
                        perror("close");
                        return -1;
                }
        }

        return 0;
}
