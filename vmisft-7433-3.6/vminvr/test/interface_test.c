/*
 * Test the interface to the NVRAM driver
 */


#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>


/*===========================================================================
 * Main routine
 */
int main()
{
	off_t size;
	char *ptr = NULL;
	char data, pattern;
	int fd;

	fd = open("/dev/nvram", O_RDWR);
	if (-1 == fd) {
		perror("open");
		return -1;
	}

	/* Use lseek to determine the size of the NVRAM
	 */
	size = lseek(fd, 0, SEEK_END);
	if (0 >= size) {
		perror("lseek");
		goto err_out;
	}

	fprintf(stderr, "NVRAM size is 0x%x bytes\n", (int) size);

	fprintf(stdout, "First pass read/write: ");

	pattern = 0x55;

	lseek(fd, -1, SEEK_CUR);
	write(fd, &pattern, sizeof (pattern));
	lseek(fd, size - 1, SEEK_SET);
	read(fd, &data, sizeof (data));

	if (data != pattern) {
		fprintf(stdout, "FAILED\n");
		goto err_out;
	}

	fprintf(stdout, "PASSED\n");

	fprintf(stdout, "Second pass read/write: ");

	pattern = ~pattern;

	lseek(fd, size - 1, SEEK_SET);
	write(fd, &pattern, sizeof (pattern));
	lseek(fd, -1, SEEK_CUR);
	read(fd, &data, sizeof (data));

	if (data != pattern) {
		fprintf(stdout, "FAILED\n");
		goto err_out;
	}

	fprintf(stdout, "PASSED\n");

	fprintf(stdout, "Memory-mapped access: ");

	/* NVRAM on some devices does not start on a page boundry; mmap() on
           these devices is not supported, use it at your own risk.
	 */
	if (size % getpagesize()) {
		fprintf(stdout, "NOT SUPPORTED\n");
	} else {
		ptr = mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
		if (MAP_FAILED == ptr) {
			perror("mmap");
			goto err_out;
		}

		if (ptr[size - 1] != pattern) {
			fprintf(stdout, "FAILED\n");
			goto err_out;
		}

		fprintf(stdout, "PASSED\n");

		if (munmap(ptr, size)) {
			perror("munmap");
			goto err_out;
		}
	}

	if (close(fd)) {
		perror("close");
		return -1;
	}

	return 0;

      err_out:
	close(fd);
	return -1;
}
