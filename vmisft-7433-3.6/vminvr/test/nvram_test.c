/*
 * Test NVRAM memory and functionality of the driver
 *
 * WARNING: This test will overwrite all existing data stored in NVRAM
 */

#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>


/*===========================================================================
 * Use a walking 1's test to verify data bus integrity.  This test returns 0
 * on success, or the first error data pattern on failure.
 */
int test_data_bus(int fd, unsigned char *data)
{
	unsigned char rval;

	for (*data = 1; 0 != *data; *data <<= 1) {
		lseek(fd, 0, SEEK_SET);
		write(fd, data, sizeof (*data));

		lseek(fd, -(sizeof (*data)), SEEK_CUR);
		read(fd, &rval, sizeof (rval));

		if (rval != *data)
			return -1;
	}

	return 0;
}


/*===========================================================================
 * Use a walking 1's test to verify address bus integrity.  This test returns
 * 0 on success, or -1 on failure. On failure, offset will be the offset from
 * the file base at which the error occurred.
 */
int test_address_bus(int fd, off_t size, off_t * offset)
{
	unsigned char pattern = 0xaa;
	unsigned char antipattern = 0x55;
	unsigned char data;
	off_t ii;

	/* Write and test the pattern at each power-of-two address
	 */
	for (*offset = 1; *offset & (size - 1); *offset <<= 1) {
		lseek(fd, *offset, SEEK_SET);
		write(fd, &pattern, sizeof (pattern));
	}

	/* Check for address line stuck high
	 */
	lseek(fd, 0, SEEK_SET);
	write(fd, &antipattern, sizeof (antipattern));

	for (*offset = 1; *offset & (size - 1); *offset <<= 1) {
		lseek(fd, *offset, SEEK_SET);
		read(fd, &data, sizeof (data));

		if (data != pattern)
			return -1;
	}

	/* Check for address lines shorted or stuck low
	 */
	for (ii = 1; ii & (size - 1); ii <<= 1) {
		lseek(fd, ii, SEEK_SET);
		write(fd, &antipattern, sizeof (antipattern));

		for (*offset = 1; *offset & (size - 1); *offset <<= 1) {
			lseek(fd, *offset, SEEK_SET);
			read(fd, &data, sizeof (data));

			if ((data != pattern) && (*offset != ii))
				return -1;

			lseek(fd, -(sizeof (data)), SEEK_CUR);
			write(fd, &pattern, sizeof (pattern));
		}
	}

	return 0;
}


/*===========================================================================
 * Perform an increment/decrement test over the entire memory range to verify
 * integrity. This could take awhile. Returns 0 on success, or -1 on failure.
 * On failure, offset will be the offset from the file base at which the error
 * occurred. The entire memory range will be zero'ed out at the end of a
 * successful test.
 */
int test_memory(int fd, off_t size, off_t * offset)
{
	unsigned char pattern;
	unsigned char antipattern;
	unsigned char data;

	/* Fill memory with a known pattern
	 */
	for (pattern = 1, *offset = 0; *offset < size; ++pattern, ++(*offset)) {
		lseek(fd, *offset, SEEK_SET);
		write(fd, &pattern, sizeof (pattern));
	}

	/* Check each location and invert it for the second pass
	 */
	for (pattern = 1, *offset = 0; *offset < size; ++pattern, ++(*offset)) {
		lseek(fd, *offset, SEEK_SET);
		read(fd, &data, sizeof (data));

		if (data != pattern)
			return -1;

		antipattern = ~pattern;
		lseek(fd, -(sizeof (data)), SEEK_CUR);
		write(fd, &antipattern, sizeof (antipattern));
	}

	/* Check each location for the inverted pattern, then zero out the
	   location.
	 */
	for (pattern = 1, *offset = 0; *offset < size; ++pattern, ++(*offset)) {
		lseek(fd, *offset, SEEK_SET);
		read(fd, &data, sizeof (data));

		if ((data & 0xff) != (~pattern & 0xff))
			return -1;

		data = 0;
		lseek(fd, -(sizeof (data)), SEEK_CUR);
		write(fd, &data, sizeof (data));
	}

	return 0;
}

/*===========================================================================
 * Main routine
 */
int main()
{
	off_t size;
	int fd;
	char error_data;
	off_t error_offset;

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

	fprintf(stdout, "Testing data bus: ");

	if (test_data_bus(fd, &error_data)) {
		fprintf(stdout, "FAILED\n");
		fprintf(stderr, "Data bus test failed with pattern 0x%x\n",
			error_data);
		goto err_out;
	}

	fprintf(stdout, "PASSED\n");

	fprintf(stdout, "Testing address bus: ");

	if (test_address_bus(fd, size, &error_offset)) {
		fprintf(stdout, "FAILED\n");
		fprintf(stderr, "Address bus test failed at offset 0x%x\n",
			(int) error_offset);
		goto err_out;
	}

	fprintf(stdout, "PASSED\n");

	fprintf(stdout, "Testing memory: ");

	if (test_memory(fd, size, &error_offset)) {
		fprintf(stdout, "FAILED\n");
		fprintf(stderr, "Memory test failed at offset 0x%x\n",
			(int) error_offset);
		goto err_out;
	}

	fprintf(stdout, "PASSED\n");

	if (close(fd)) {
		perror("close");
		return -1;
	}

	return 0;

      err_out:
	close(fd);
	return -1;
}
