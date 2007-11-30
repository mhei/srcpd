/***************************************************************************
                          io.c  -  description
                             -------------------
    begin                : Wed Jul 4 2001
    copyright            : (C) 2001 by Dipl.-Ing. Frank Schmischke
    email                : frank.schmischke@t-online.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

/*
	changes:

	25.11.2007 Frank Schmischke
			in isvalidchar() change 'char' to 'unsigned char' to avoid compiler
			warning

*/

#include "stdincludes.h"

#include "config-srcpd.h"
#include "io.h"
#include "syslogmessage.h"
#include "ttycygwin.h"


int readByte(bus_t bus, int wait, unsigned char *the_byte)
{
    ssize_t i;
    int status;

    /* with debug level beyond DBG_DEBUG, we will not really work on hardware */
    if (buses[bus].debuglevel > DBG_DEBUG) {
        i = 1;
        *the_byte = 0;
    }
    else {
        status = ioctl(buses[bus].device.file.fd, FIONREAD, &i);
        if (status < 0) {
            syslog_bus(bus, DBG_ERROR,
                "readbyte(): ioctl failed: %s (errno = %d)\n",
                strerror(errno), errno);
            return -1;
        }
        syslog_bus(bus, DBG_DEBUG,
            "readbyte(): (fd = %d), there are %d bytes to read.",
            buses[bus].device.file.fd, i);
        /* read only, if there is really an input */
        if ((i > 0) || (wait == 1)) {
            i = read(buses[bus].device.file.fd, the_byte, 1);
            if (i < 0) {
                syslog_bus(bus, DBG_ERROR,
                    "readbyte(): read failed: %s (errno = %d)\n",
                    strerror(errno), errno);
            }
            if (i > 0)
                syslog_bus(bus, DBG_DEBUG, "readbyte(): byte read: 0x%02x",
                    *the_byte);
        }
    }
    return (i > 0 ? 0 : -1);
}

void writeByte(bus_t bus, unsigned char b, unsigned long msecs)
{
    ssize_t i = 0;
    char byte = b;

    if (buses[bus].debuglevel <= DBG_DEBUG) {
        i = write(buses[bus].device.file.fd, &byte, 1);
        tcdrain(buses[bus].device.file.fd);
    }
    if (i < 0) {
        syslog_bus(bus, DBG_ERROR, "(FD: %d) write failed: %s (errno = %d)\n",
                buses[bus].device.file.fd, strerror(errno), errno);
    }
    else {
        syslog_bus(bus, DBG_DEBUG, "(FD: %d) %i byte sent: 0x%02x (%d)\n",
        buses[bus].device.file.fd, i, b, b);
    }
    usleep(msecs * 1000);
}

void writeString(bus_t bus, unsigned char *s, unsigned long msecs)
{
    int l = strlen((char *) s);
    int i;
    for (i = 0; i < l; i++) {
        writeByte(bus, s[i], msecs);
    }
}

void save_comport(bus_t bus)
{
    int fd;

    fd = open(buses[bus].device.file.path, O_RDWR);
    if (fd == -1) {
        syslog_bus(bus, DBG_ERROR, "Open serial line failed: %s (errno = %d).\n",
                strerror(errno), errno);
    }
    else {
        tcgetattr(fd, &buses[bus].device.file.devicesettings);
        close(fd);
    }
}

void restore_comport(bus_t bus)
{
    int fd;

    syslog_bus(bus, DBG_INFO, "Restoring attributes for serial line %s",
        buses[bus].device.file.path);
    fd = open(buses[bus].device.file.path, O_RDWR);
    if (fd == -1) {
        syslog_bus(bus, DBG_ERROR, "Open serial line failed: %s (errno = %d).\n",
                strerror(errno), errno);
    }
    else {
        syslog_bus(bus, DBG_INFO, "Restoring old values...");
        tcsetattr(fd, TCSANOW, &buses[bus].device.file.devicesettings);
        close(fd);
        syslog_bus(bus, DBG_INFO, "Old values successfully restored");
    }
}

void close_comport(bus_t bus)
{
    struct termios interface;
    syslog_bus(bus, DBG_INFO, "Closing serial line");

    tcgetattr(buses[bus].device.file.fd, &interface);
    cfsetispeed(&interface, B0);
    cfsetospeed(&interface, B0);
    tcsetattr(buses[bus].device.file.fd, TCSANOW, &interface);
    close(buses[bus].device.file.fd);
}

/* Zeilenweises Lesen vom Socket      */
/* nicht eben trivial!                */
int isvalidchar(unsigned char c)
{
    return ((c >= 0x20 && c <= 127) || c == 0x09 || c == '\n');
}

int socket_readline(int Socket, char *line, int len)
{
    char c;
    int i = 0;
    ssize_t bytes_read = read(Socket, &c, 1);
    if (bytes_read <= 0) {
        syslog_bus(0, DBG_ERROR,
                "read from socket %d failed: %s (errno = %d)\n",
                Socket, strerror(errno), errno);
        return -1;
    }
    else {
        if (isvalidchar(c))
            line[i++] = c;
        /* die Reihenfolge beachten! */
        while (read(Socket, &c, 1) > 0) {
            /* Ende beim Zeilenende */
            if (c == '\n')
                break;
            if (isvalidchar(c) && (i < len - 1))
                line[i++] = c;
        }
    }
    line[i++] = 0x00;
    syslog_bus(0, DBG_DEBUG, "socket %d, read %s", Socket, line);
    return 0;
}

/**
 * noch ganz klar ein schoenwetter code!
 **/
int socket_writereply(int Socket, const char *line)
{
    ssize_t status = 0;
    int linelen = strlen(line);
    char chunk[MAXSRCPLINELEN], tmp[MAXSRCPLINELEN];
    int i = 0;

    if (linelen <= 0)
        return 0;

    syslog_bus(0, DBG_INFO, "socket %d, write %s", Socket, line);

    while (i <= linelen - MAXSRCPLINELEN - 1 && status >= 0) {
        memset(tmp, 0, sizeof(tmp));
        strncpy(tmp, line + i, MAXSRCPLINELEN - 2);
        sprintf(chunk, "%s\\\n", tmp);
        status = write(Socket, chunk, strlen(chunk));
        if (status == -1) {
            syslog_bus(0, DBG_ERROR,
                    "write to socket %d failed: %s (errno = %d)\n",
                    Socket, strerror(errno), errno);
            return status;
        }
        i += MAXSRCPLINELEN - 2;
    }

    if (i < linelen && status >= 0) {
        status = write(Socket, line + i, linelen - i);
    }

    syslog_bus(0, DBG_INFO, "Status from write: %d", status);
    return status;
}
