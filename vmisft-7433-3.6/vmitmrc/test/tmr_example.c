
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
        int fd1, fd3, count, ii, start_count, end_count;
        char src[buffer_size], dest[buffer_size];
        fd_set fds;

        if (-1 == (fd1 = open("/dev/timer1", O_RDWR))) {
                perror("open");
                return -1;
        }

        if (-1 == (fd3 = open("/dev/timer3", O_RDWR))) {
                perror("open");
                return -1;
        }

        /* Set up the file descriptor we are going to poll for interrupts on.
         */
        FD_ZERO(&fds);
        FD_SET(fd1, &fds);

        /* Set up timer 1 to cause a periodic interrupt once per second.
         */
        count = 1000000;
        write(fd1, &count, sizeof (count));
        ioctl(fd1, TIMER_INTERRUPT_ENABLE);
        ioctl(fd1, TIMER_START);

        /* Copy some data every second for 5 iterations. Measure the
           time it took to complete the memcpy for each iteration.
         */
        printf("Copying %d bytes\n", buffer_size);
        start_count = 0x10000;
        for (ii = 1; ii <= 5; ++ii) {
                /* Use select to poll for an interrupt.
                 */
                select(fd1 + 1, NULL, NULL, &fds, NULL);

                /* Write an initial count and start the timer.
                 */
                write(fd3, &start_count, sizeof (start_count));
                ioctl(fd3, TIMER_START);

                /* Do some work.
                 */
                memcpy(dest, src, buffer_size);

                /* Read back the count and determine how long it took to do
                   the work.
                 */
                ioctl(fd3, TIMER_STOP);
                read(fd3, &end_count, sizeof (end_count));
                printf("Copy %d took %dus\n", ii, start_count - end_count);
        }

        ioctl(fd1, TIMER_INTERRUPT_DISABLE);
        ioctl(fd1, TIMER_STOP);

        close(fd1);
        close(fd3);

        return 0;
}
