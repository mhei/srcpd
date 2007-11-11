/**
 *  C Implementation: portio
 *
 * Description: 
 * A wrapper around the serial interface (termios)
 *
 * Author: Ing. Gerard van der Sel
 *
 * Copyright: See COPYING file that comes with this distribution
 *
 * Information about configuring the Comport is found on:
 * Serial programming HOW-TO:
 * http://www.tldp.org/HOWTO/Serial-Programming-HOWTO/index.html
 * Test for serial transmission and reception for thread and interrupt control
 * http://www.easysw.com/~mike/serial
 */

#include "stdincludes.h"

#include "config-srcpd.h"
#include "ttycygwin.h"
#include "portio.h"

/**
 * open_port:
 * Open and initialise the serial port
 * Port is opened for read/write
 *         8 bits and 1 stop bit, no parity.
 *         No timeout at reading the port
 * Input: Number of the bus
 * Output: >=0 a device is successfully attached
 *          <0 a error is reported.
 */
int open_port(bus_t bus)
{
        int serial;
        struct termios settings;

        serial  = open(buses[bus].filename.path, O_RDWR | O_NOCTTY);
        if (serial < 0) {
                DBG(bus, DBG_ERROR, 
					"Error, could not open %s.\nReported error number: %d.\n",
                    buses[bus].filename.path, errno); /* , str_errno(errno)); */
                buses[bus].fd = -1;
                return -errno;
        }
        /* Get default settings from OS */
        tcgetattr(serial, &buses[bus].devicesettings);
        /* Ignore default setting from the OS */
        bzero(&settings, sizeof(struct termios));
        /* Setup settings for serial port */
        /* Baud rate, enable read, local mode, 8 bits, 1 stop bit, no parity */
        settings.c_cflag = buses[bus].baudrate | CREAD | CLOCAL | CS8;
        /* No parity control or generation */
        settings.c_iflag = IGNPAR;
        settings.c_oflag = 0;
        settings.c_lflag = 0;
        settings.c_cc[VMIN] = 1;      /* Block size is 1 character */
        settings.c_cc[VTIME] = 0;     /* never a timeout */
        /* Apply baud rate (new style) */
        cfsetospeed(&settings, buses[bus].baudrate);
        cfsetispeed(&settings, buses[bus].baudrate);
        /* Apply settings for serial port */
        tcsetattr(serial, TCSANOW, &settings);
        /* Flush serial buffers */
        tcflush(serial, TCIFLUSH);
        tcflush(serial, TCOFLUSH);
        buses[bus].fd = serial;
        return serial;
}

/**
 * close_port:
 * closes a comport and restores its old settings
 */
void close_port(bus_t bus)
{
        tcsetattr(buses[bus].fd, TCSANOW, &buses[bus].devicesettings);
        close(buses[bus].fd);
        buses[bus].fd = -1;
}

/**
 * write_port
 */
void write_port(bus_t bus, unsigned char b)
{
        ssize_t i;

        i = 0;
        if (buses[bus].debuglevel <= DBG_DEBUG) {
                /* Wait for transmit queue to go empty */
                tcdrain(buses[bus].fd);
                i = write(buses[bus].fd, &b, 1);
        }
        if (i < 0) {
                /* Error reported from write */
                DBG(bus, DBG_ERROR, 
                        "write_port(): External error: errno %d",
                        errno); /* , str_errno(errno)); */
        }
        if (i == 0) {
                /* Error reported from write */
                DBG(bus, DBG_ERROR, 
                        "write_port(): No data written to port.\n");
        }
}


/**
 * read_port
 */
unsigned int read_port(bus_t bus)
{
        ssize_t i;
        unsigned int in;

        /* Default value in case of no real port or debugging */
        in = 0x00;
        if (buses[bus].debuglevel <= DBG_DEBUG) {
                /* read input port */
                i = read(buses[bus].fd, &in, 1);
                if (i < 0) {
                        /* Error reading port */
                        DBG(bus, DBG_ERROR,
                            "read_port(): Read status: %d with errno = %d.\n",
                            i, errno);/* , str_errno(errno)); */
                        in = 0x200 + errno;		/* Result all blocked */
                }
                if (i == 0) {
                        /* Empty port */
                        DBG(bus, DBG_ERROR,
                                "read_port(): Port empty.\n");
                        in = 0x1FF;     /* Result all blocked */
                }
        }
        return in;
}


/**
 * check_port
 */
int check_port(bus_t bus)
{
        int temp;

        ioctl(buses[bus].fd, FIONREAD, &temp);
        return (temp > 0 ? -1 : 0);
}

