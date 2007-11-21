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
#include "stdincludes.h"

#include "config-srcpd.h"
#include "io.h"
#include "ttycygwin.h"

int readByte(bus_t bus, int wait, unsigned char *the_byte)
{
    ssize_t b_read;
    int status;

    /* with debug level beyond DBG_DEBUG, we will not really work on hardware */
    if (buses[bus].debuglevel > DBG_DEBUG) {
        b_read = 1;
        *the_byte = 0;
    }
    else {
        status = ioctl(buses[bus].fd, FIONREAD, &b_read);
        if (status == -1) {
            DBG(bus, DBG_ERROR,
                "readbyte(): ioctl failed: %s (errno = %d)\n",
                strerror(errno), errno);
            return -1;
        }
        DBG(bus, DBG_DEBUG,
            "readbyte(): (fd = %d), there are %d bytes to read.",
            buses[bus].fd, b_read);
        /* read only, if there is really an input */
        if ((b_read > 0) || (wait == 1)) {
            b_read = read(buses[bus].fd, the_byte, 1);
            if (b_read == -1) {
                DBG(bus, DBG_ERROR,
                    "readbyte(): read failed: %s (errno = %d)\n",
                    strerror(errno), errno);
            }
            if (b_read > 0)
                DBG(bus, DBG_DEBUG, "readbyte(): byte read: 0x%02x",
                    *the_byte);
        }
    }
    return (b_read > 0 ? 0 : -1);
}

void writeByte(bus_t bus, unsigned char b, unsigned long msecs)
{
    ssize_t b_written = 0;
    char byte = b;

    if (buses[bus].debuglevel <= DBG_DEBUG) {
        b_written = write(buses[bus].fd, &byte, 1);
        tcdrain(buses[bus].fd);
    }
    if (b_written == -1) {
        DBG(bus, DBG_ERROR, "(FD: %d) write failed: %s (errno = %d)\n",
                buses[bus].fd, errno, strerror(errno));
    }
    else {
        DBG(bus, DBG_DEBUG, "(FD: %d) %i byte sent: 0x%02x (%d)",
                buses[bus].fd, b_written, b, b);
    }
    usleep(msecs * 1000);
}

void writeString(bus_t bus, unsigned char *s, unsigned long msecs)
{
    size_t i, l;

    l = strlen((char *) s);
    for (i = 0; i < l; i++) {
        writeByte(bus, s[i], msecs);
    }
}

void save_comport(bus_t bus)
{
    int fd;

    fd = open(buses[bus].device.filename.path, O_RDWR);
    if (fd == -1) {
        DBG(bus, DBG_ERROR, "Open serial line failed: %s (errno = %d).\n",
                strerror(errno), errno);
    }
    else {
        tcgetattr(fd, &buses[bus].devicesettings);
        close(fd);
    }
}

void restore_comport(bus_t bus)
{
    int fd;

    DBG(bus, DBG_INFO, "Restoring attributes for serial line %s",
        buses[bus].device.filename.path);
    fd = open(buses[bus].device.filename.path, O_RDWR);
    if (fd == -1) {
        DBG(bus, DBG_ERROR, "Open serial line failed: %s (errno = %d).\n",
                strerror(errno), errno);
    }
    else {
        DBG(bus, DBG_INFO, "Restoring old values...");
        tcsetattr(fd, TCSANOW, &buses[bus].devicesettings);
        close(fd);
        DBG(bus, DBG_INFO, "Old values successfully restored");
    }
}

void close_comport(bus_t bus)
{
    struct termios interface;
    DBG(bus, DBG_INFO, "Closing serial line");

    tcgetattr(buses[bus].fd, &interface);
    cfsetispeed(&interface, B0);
    cfsetospeed(&interface, B0);
    tcsetattr(buses[bus].fd, TCSANOW, &interface);
    close(buses[bus].fd);
}

/* Zeilenweises Lesen vom Socket      */
/* nicht eben trivial!                */
int isvalidchar(char c)
{
    return ((c >= 0x20 && c <= 127) || c == 0x09 || c == '\n');
}

int socket_readline(int Socket, char *line, int len)
{
    char c;
    int i = 0;
    ssize_t b_read = read(Socket, &c, 1);

    if (b_read == -1) {
        DBG(0, DBG_ERROR, "read from socket %d failed: %s (errno = %d)\n",
                Socket, strerror(errno), errno);
        return -1;
    }

    else if (b_read == 0)
        return 0;
    
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
    DBG(0, DBG_DEBUG, "socket %d, read %s", Socket, line);
    return 0;
}

/**
 * noch ganz klar ein schoenwetter code!
 **/
int socket_writereply(int Socket, const char *line)
{
    char chunk[MAXSRCPLINELEN], tmp[MAXSRCPLINELEN];
    ssize_t b_written = 0;
    size_t i, linelen;

    i = 0;
    linelen = strlen(line);

    if (linelen <= 0)
        return 0;

    DBG(0, DBG_INFO, "socket %d, write %s", Socket, line);
    
    while (i <= linelen - MAXSRCPLINELEN - 1 && b_written != -1) {
        memset(tmp, 0, sizeof(tmp));
        strncpy(tmp, line + i, MAXSRCPLINELEN - 2);
        sprintf(chunk, "%s\\\n", tmp);
        b_written = write(Socket, chunk, strlen(chunk));
        i += MAXSRCPLINELEN - 2;
    }
    
    if (i < linelen && b_written != -1) {
        b_written = write(Socket, line + i, linelen - i);
    }

    if (b_written == -1)
        DBG(0, DBG_ERROR, "write to socket %d failed: %s (errno = %d)\n",
                Socket, strerror(errno), errno);

    return b_written;
}
