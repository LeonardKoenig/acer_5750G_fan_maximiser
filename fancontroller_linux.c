/**
 * Controls the fan on devices using a KB930 EC (or similar, like KB9012)
 * and a not further known fan controller via the 0x68/0x6c I/O-ports
 * interface on mentioned EC.
 *
 * A EC-Spec can be found here:
 * https://reservice.pro/upload/Datasheets/kb9012qf.pdf
 * (p. 128; pdf: 138)
 *
 * Description of the protocol:
 * http://wiki.laptop.org/go/Revised_EC_Port_6C_Command_Protocol
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>

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


/* I/O ports to control EC */
#define LPC68_CTRL_PORT 0x6c
#define LPC68_DATA_PORT 0x68

/* flags of LPC68CSR (LPC I/O 0x68/0x6C Configuration and Status Register) */
#define FLAG_IO_BSY 0x80
#define FLAG_IBF 0x02

/* if written to LPC68_CTRL_PORT clears FLAG_IO_BSY */
#define CLR_IO_BSY 0xff

#define FAN_SET 0x59 /* we want to take over fan control */
#define FAN_MAX 0x77 /* set fan to max */
#define FAN_NRM 0x76 /* set fan to normal */

#define MAX_TRIES 10000

/**
 * Sets up I/O 0x68/0x6c interface to EC.
 *
 *  - request permission
 *  - waits until I/O-interface is not busy
 *
 * @return 0 on success
 */
int ec_intro_sequence();

/**
 * Cleans up I/O ports interface.
 *
 *  - clears I/O busy flag
 *
 * @return 0 on success
 */
int ec_outro_sequence();

/**
 * Waits for a specified bitmask to appear.
 *
 * Loops for some time until either condition (inb(port) & bitmask) == 0x0
 * holds true or it times out (after MAX_TRIES tries).
 *
 * @param port to read from
 * @param bitmask to apply
 * @return 0 on success
 */
int wait_until_bitmask_is_value(unsigned short port, unsigned char bitmask);

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

	unsigned char fan_setting = 0;
	switch (argv[1][1]) {
		case 'm':
		case 'M':
			fan_setting = FAN_MAX;
			break;
		case 'n':
		case 'N':
			fan_setting = FAN_NRM;
			break;
		case 'h':
			print_usage(stdout, argv[0]);
			exit(1);
		default:
			fprintf(stderr, ERRPREF "Invalid arguments!\n");
			exit(1);
	}

	assert(fan_setting == FAN_MAX || fan_setting == FAN_NRM);

	if (ec_intro_sequence() != 0) {
		fprintf(stderr, ERRPREF "Unable to perform intro sequence. "
				"Cleaning up\n");
		ec_outro_sequence();
		exit(1);
	}

	/* wait for IBF flag to be cleared */
	if (wait_until_bitmask_is_value(LPC68_CTRL_PORT, FLAG_IBF) != 0) {
		exit(1);
	}
	/* write command (set fan) */
	outb(FAN_SET, LPC68_CTRL_PORT);

	if (wait_until_bitmask_is_value(LPC68_CTRL_PORT, FLAG_IBF) != 0) {
		exit(1);
	}
	/* write data (fan setting: max/normal) */
	outb(fan_setting, LPC68_DATA_PORT);

	ec_outro_sequence();
}

int ec_intro_sequence()
{
	/* get permissions */
	if (ioperm(FLAG_IO_BSY, 1, 1) != 0) { /* needed for inb_p() */
		perror(__func__);
		return -1;
	}
	if (ioperm(LPC68_DATA_PORT, 5, 1) != 0) {
		perror(__func__);
		return -1;
	}

	/* wait until FLAG_IO_BSY is unset */
	if (wait_until_bitmask_is_value(LPC68_CTRL_PORT, FLAG_IO_BSY) != 0) {
		return -1;
	}

	return 0;
}

int ec_outro_sequence()
{
	if (wait_until_bitmask_is_value(LPC68_CTRL_PORT, FLAG_IBF) != 0) {
		return -1;
	}

	/* clear I/O busy flag */
	outb(CLR_IO_BSY, LPC68_CTRL_PORT);

	return 0;
}


int wait_until_bitmask_is_value(unsigned short port, unsigned char bitmask)
{
	unsigned char read_value = 0;
	for (int i = 0; i < MAX_TRIES; i++) {
		/* inb_p sleeps for a short time. */
		read_value = inb_p(port);
		if ((read_value & bitmask) == 0x0) {
			return 0;
		}
	}
	fprintf(stderr, ERRPREF "Timeout waiting for mask %#x on port %#x\n"
			"last read value was %#x\n",
			bitmask, port, read_value);
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
