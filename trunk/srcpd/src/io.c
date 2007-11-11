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
    ssize_t i;
    int status;

    /* with debug level beyond DBG_DEBUG, we will not really work on hardware */
    if (buses[bus].debuglevel > DBG_DEBUG) {
        i = 1;
        *the_byte = 0;
    }
    else {
        status = ioctl(buses[bus].fd, FIONREAD, &i);
        if (status < 0) {
            char msg[200];
            strcpy(msg, strerror(errno));
            DBG(bus, DBG_ERROR,
                "readbyte(): IOCTL status: %d with errno = %d: %s",
                status, errno, msg);
            return -1;
        }
        DBG(bus, DBG_DEBUG,
            "readbyte(): (fd = %d), there are %d bytes to read.",
            buses[bus].fd, i);
        /* read only, if there is really an input */
        if ((i > 0) || (wait == 1)) {
            i = read(buses[bus].fd, the_byte, 1);
            if (i < 0) {
                char emsg[200];
                strerror_r(errno, emsg, sizeof(emsg));
                DBG(bus, DBG_ERROR,
                    "readbyte(): read status: %d with errno = %d: %s", i,
                    errno, *emsg);
            }
            if (i > 0)
                DBG(bus, DBG_DEBUG, "readbyte(): byte read: 0x%02x",
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
        i = write(buses[bus].fd, &byte, 1);
        tcdrain(buses[bus].fd);
    }
    if (i < 0) {
    DBG(bus, DBG_ERROR, "(FD: %d) External error: errno %d",
        buses[bus].fd, errno); /* , str_errno(errno)); */
    }
    else {
    DBG(bus, DBG_DEBUG, "(FD: %d) %i byte sent: 0x%02x (%d)",
        buses[bus].fd, i, b, b);
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

    fd = open(buses[bus].device.filename.path, O_RDWR);
    if (fd == -1) {
       DBG(bus, DBG_ERROR, "Error, could not open device.\n");
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
        DBG(bus, DBG_ERROR, "Error, could not open device.");
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
    ssize_t bytes_read = read(Socket, &c, 1);
    if (bytes_read <= 0) {
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
    DBG(0, DBG_DEBUG, "socket %d, read %s", Socket, line);
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

    DBG(0, DBG_INFO, "socket %d, write %s", Socket, line);
    
    while (i <= linelen - MAXSRCPLINELEN - 1 && status >= 0) {
        memset(tmp, 0, sizeof(tmp));
        strncpy(tmp, line + i, MAXSRCPLINELEN - 2);
        sprintf(chunk, "%s\\\n", tmp);
        status = write(Socket, chunk, strlen(chunk));
        i += MAXSRCPLINELEN - 2;
    }
    
    if (i < linelen && status >= 0) {
        status = write(Socket, line + i, linelen - i);
    }

    DBG(0, DBG_INFO, "Status from write: %d", status);
    return status;
}
