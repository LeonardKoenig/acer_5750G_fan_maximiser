#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>

#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
/*
 * You must compile with -O or -O2 or similar.  The functions are  defined  as
 * inline macros, and will not be substituted in without optimization enabled,
 * causing unresolved references at link time.
 */
#include <sys/io.h>

#define PROGNAME fc
#define ERRPREF "[" STRINGIFY(PROGNAME) "]: "
#define _STRINGIFY(s) #s
#define STRINGIFY(s) _STRINGIFY(s)

#define MAX 0x77
#define NORMAL 0x76
#define IOPORTS "/dev/port"

/**
 * Sets up IOPORTS interface.
 *
 * @return filedescriptor for interface
 */
int ec_intro_sequence();

/**
 * Cleans up IOPORTS.
 *
 * @param filedescriptor to use
 * @return 0 on success, -1 on failure
 */
int ec_outro_sequence();

/**
 * Waits for a bitmask to appear
 *
 * Loops for some time until the given port on the filedescriptor has value
 * if for specified bitmask.
 *
 * @param filedescriptor to use
 * @param port to read from
 * @param bitmask to apply
 * @param value to check. Must be positive.
 * @return 0 when bitmask appears, -1 if it doesn't
 */
int wait_until_bitmask_is_value(unsigned short port, unsigned char bitmask,
		unsigned char value);

/**
 * Prints usage.
 *
 * @param stream to use (eg. stdout/stderr)
 * @param progname to display (usually argv[0])
 */
void print_usage(FILE *stream, char *progname);


int main(int argc, char *argv[])
{
	if (argc < 2) {
		fprintf(stderr, ERRPREF "No arguments supplied (-h for usage)\n");
		exit(1);
	}

	unsigned char cmd = 0;
	switch (argv[1][1]) {
		case 'm':
		case 'M':
			cmd = MAX;
			break;
		case 'n':
		case 'N':
			cmd = NORMAL;
			break;
		case 'h':
			print_usage(stdout, argv[0]);
			exit(1);
		default:
			fprintf(stderr, ERRPREF "Invalid arguments!\n");
			exit(1);
	}

	assert(cmd == MAX || cmd == NORMAL);

	ioperm(0x80, 1, 1); // needed for inb_p()
	ioperm(0x68, 5, 1);

	if (ec_intro_sequence() < 0) {
		fprintf(stderr, ERRPREF "Unable to perform intro sequence. "
				"Cleaning up\n");
		ec_outro_sequence();
		exit(1);
	}

	if (wait_until_bitmask_is_value(0x6C, 0x02, 0x00) != 0) {
		fprintf(stderr, ERRPREF "Error waiting for magic value. "
				"Cleaning up\n");
		ec_outro_sequence();
		exit(1);
	}

	outb(cmd, 0x68);

	ec_outro_sequence();
}

int ec_intro_sequence()
{
	if (wait_until_bitmask_is_value(0x6C, 0x80, 0x00) != 0) {
		return -1;
	}
	inb(0x68);

	if (wait_until_bitmask_is_value(0x6C, 0x02, 0x00) != 0) {
		return -1;
	}

	outb(0x59, 0x6C);
	return 0;
}

int ec_outro_sequence()
{
	inb(0x68);
	if (wait_until_bitmask_is_value(0x6C, 0x02, 0x00) != 0) {
		return -1;
	}
	outb(0xFF, 0x6C);

	return 0;
}


int wait_until_bitmask_is_value(unsigned short port, unsigned char bitmask,
		unsigned char value)
{
	assert(value > 0);

	for (int i = 0; i < 10000; i++) {
		// inb_p sleeps for a short time.
		if ((inb_p(port) & bitmask) == value) {
			return 0;
		}
	}
	fprintf(stderr, ERRPREF "Timeout waiting for mask %x with value %x on port %x\n",
			bitmask, value, port);
	return -1;
}

void print_usage(FILE *stream, char *progname)
{
	fprintf(stream, "Usage: %s [options]\n"
			"Options:\n"
			" -m\tSet fan to max setting\n"
			" -n\tSet fan to normal setting\n"
			" -h\tPrint this help message\n", progname);
}
