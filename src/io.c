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
#include <stdincludes.h>

#include "io.h"
#include "config-srcpd.h"

int readByte(int bus, int wait, unsigned char *the_byte)
{
  int i;
  int status;

  // with debuglevel beyond DBG_DEBUG, we will not really work on hardware
  if (busses[bus].debuglevel > DBG_DEBUG)
  {  
    i = 1;
    *the_byte = 0;
  }
  else
  {
    status = ioctl(busses[bus].fd, FIONREAD, &i);
    if(status < 0) {
      char msg[200];
      strerror_r(errno, msg, sizeof(msg));
      DBG(bus, DBG_ERROR, "readbyte(): IOCTL   status: %d with errno = %d: %s", status, errno, *msg);
    }
    DBG(bus, DBG_DEBUG, "readbyte(): (fd = %d), there are %d bytes to read.", busses[bus].fd, i);
    // read only, if there is really an input
    if ((i > 0) || (wait == 1))
    {
      i = read(busses[bus].fd, the_byte, 1);
      if(i < 0) {
        char emsg[200];
        strerror_r(errno, emsg, sizeof(emsg));
        DBG(bus, DBG_ERROR, "readbyte(): read status: %d with errno = %d: %s", i, errno, *emsg);
      }
      if (i > 0)
        DBG(bus, DBG_DEBUG, "readbyte(): byte read: 0x%02x", *the_byte);
    }
  }
  return (i > 0 ? 0 : -1);
}

void writeByte(int bus, unsigned char b, unsigned long msecs)
{
  int i=0;
  char byte=b;
  if(busses[bus].debuglevel <= DBG_DEBUG)
  {
    i = write(busses[bus].fd, &byte, 1);
    tcdrain(busses[bus].fd);
  }
  DBG(bus, DBG_DEBUG, "(FD: %d) %i byte sent: 0x%02x (%d)",  busses[bus].fd, i, b, b);
  usleep(msecs * 1000);
}

void writeString(int bus, unsigned char *s, unsigned long msecs)
{
  int l = strlen(s);
  int i;
  for(i=0; i<l;i++) {
    writeByte(bus, s[i], msecs);
  }
}

void save_comport(int businfo)
{
  int fd;

  fd=open(busses[businfo].device, O_RDWR);
  if (fd == -1)
  {
    printf("dammit, couldn't open device.\n");
  }
  else
  {
    tcgetattr(fd, &busses[businfo].devicesettings);
    close(fd);
  }
}

void restore_comport(int bus)
{
  int fd;

  DBG(bus, DBG_INFO, "Restoreing attributes for serial line %s", busses[bus].device);
  fd=open(busses[bus].device,O_RDWR);
  if (fd == -1)
  {
    DBG(bus, DBG_ERROR, "dammit, couldn't open device.");
  }
  else
  {
    DBG(bus, DBG_INFO, "alte Werte werden wiederhergestellt");
    tcsetattr(fd, TCSANOW, &busses[bus].devicesettings);
    close(fd);
    DBG(bus, DBG_INFO, "erfolgreich wiederhergestellt");
  }
}

void close_comport(int bus)
{
  struct termios interface;
  DBG(bus, DBG_INFO,  "Closing serial line");

  tcgetattr(busses[bus].fd, &interface);
  cfsetispeed(&interface, B0);
  cfsetospeed(&interface, B0);
  tcsetattr(busses[bus].fd, TCSANOW, &interface);
  close(busses[bus].fd);
}

/* Zeilenweises Lesen vom Socket      */
/* nicht eben trivial!                */
int socket_readline(int Socket, char *line, int len)
{
    char c;
    int i = 0;
    int bytes_read = read(Socket, &c, 1);
    if (bytes_read <= 0) {
	return -1;
    } else {
	line[0] = c;
	/* die reihenfolge beachten! */
	while (c != '\n' && c != 0x00 && read(Socket, &c, 1) > 0) {
	    /* Ende beim Zeilenende */
	    if (c == '\n')
		break;
	    /* Folgende Zeichen ignorieren wir */
	    if (c == '\r' || c == (char) 0x00 || i >= len)
		continue;
	    line[++i] = c;
	}
    }
    line[++i] = 0x00;
    return 0;
}

/******************
 * noch ganz klar ein schoenwetter code!
 *
 */
int socket_writereply(int Socket, const char *line)
{
    int status;
    long int linelen = strlen(line);
    DBG(0, DBG_DEBUG, "socket %d, write %s", Socket, line);
    if (linelen >= 1000) {
	/* split line into chunks as specified in SRCP, not yet implemented */
    }
    status = write(Socket, line, strlen(line));
    DBG(0, DBG_DEBUG, "status from write: %d", status);
    return status;
}
