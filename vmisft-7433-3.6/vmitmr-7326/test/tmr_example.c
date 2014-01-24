
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <linux/hwtimer.h>
#include <sys/ioctl.h>


/*===========================================================================
 * Main routine
 */
int main()
{
        const int buffer_size = 0x100000;
        int fd3, fd4, count, rate, ii, start_count, end_count;
        char src[buffer_size], dest[buffer_size];
        fd_set fds;

        if (-1 == (fd3 = open("/dev/timer3", O_RDWR))) {
                perror("open");
                return -1;
        }

        if (-1 == (fd4 = open("/dev/timer4", O_RDWR))) {
                perror("open");
                return -1;
       }

        /* Set up the file descriptor we are going to poll for interrupts on.
         */
        FD_ZERO(&fds);
        FD_SET(fd3, &fds);

        /* Set up timer 3 to cause a periodic interrupt every second.
         */
        rate = 1000;
        count = 1000000;
        ioctl(fd3, TIMER_RATE_SET, &rate);
        write(fd3, &count, sizeof (count));
        ioctl(fd3, TIMER_INTERRUPT_ENABLE);
        ioctl(fd3, TIMER_START);

        /* Set timer 4 for 1Mhz
         */
        ioctl(fd4, TIMER_RATE_SET, &rate);

        /* Copy some data every second for 5 iterations. Measure the
           time it took to complete the memcpy for each iteration.
         */
        printf("Copying %d bytes\n", buffer_size);
        start_count = 0xffffffff;
        for (ii = 1; ii <= 5; ++ii) {
                /* Use select to poll for an interrupt.
                 */
                select(fd3 + 1, NULL, NULL, &fds, NULL);

                /* Write an initial count and start the timer.
                 */
                write(fd4, &start_count, sizeof (start_count));
                ioctl(fd4, TIMER_START);

                /* Do some work.
                 */
                memcpy(dest, src, buffer_size);

                /* Read back the count and determine how long it took to do
                   the work.
                 */
                ioctl(fd4, TIMER_STOP);
                read(fd4, &end_count, sizeof (end_count));
                printf("Copy %d took %dus\n", ii, start_count - end_count);
        }

        ioctl(fd3, TIMER_INTERRUPT_DISABLE);
        ioctl(fd3, TIMER_STOP);

        close(fd3);
        close(fd4);

        return 0;
}
