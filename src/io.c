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
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <termios.h>
#include <unistd.h>

#include "io.h"
#include "config-srcpd.h"

int readByte(int bus, unsigned char *the_byte)
{
  int i;

  // with debuglevel 7, we will not really work on hardware
  if(busses[bus].debuglevel > 6)
  {  
    i = 1;
    *the_byte = 0;
  }
  else
  {
    ioctl(busses[bus].fd, FIONREAD, &i);
    if(busses[bus].debuglevel > 5)
      syslog(LOG_INFO, "on bus %d bytes to read: %d", bus, i);
    if (i > 0)
    {
      // read only, if there is really an input
      i = read(busses[bus].fd, the_byte, 1);
      if(i == 1)
      {
        if(busses[bus].debuglevel > 5)
          syslog(LOG_INFO, "bus %d byte read: 0x%02x", bus, *the_byte);
      }
      else
      {
        if(busses[bus].debuglevel > 5)
          syslog(LOG_INFO, "no byte read");
      }
    }
  }
  return (i == 1 ? 0 : -1);
}

void writeByte(int bus, unsigned char *b, unsigned long msecs)
{
  if(busses[bus].debuglevel <= 3)
  {
    write(busses[bus].fd, b, 1);
    tcflush(busses[bus].fd, TCOFLUSH);
  }
  if(busses[bus].debuglevel >= 2)
  {
    syslog(LOG_INFO, "bus %d (FD: %d) byte sent: 0x%02x", bus, busses[bus].fd, *b);
  }
  usleep(msecs * 1000);
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

  syslog(LOG_INFO, "Restoreing attributes for serial line %s", busses[bus].device);
  fd=open(busses[bus].device,O_RDWR);
  if (fd == -1)
  {
    syslog(LOG_INFO, "dammit, couldn't open device.");
  }
  else
  {
    syslog(LOG_INFO, "alte Werte werden wiederhergestellt");
    tcsetattr(fd, TCSANOW, &busses[bus].devicesettings);
    close(fd);
    syslog(LOG_INFO, "erfolgreich wiederhergestellt");
  }
}

void close_comport(int bus)
{
  struct termios interface;
  syslog(LOG_INFO, "Closing serial line");

  tcgetattr(busses[bus].fd, &interface);
  cfsetispeed(&interface, B0);
  cfsetospeed(&interface, B0);
  tcsetattr(busses[bus].fd, TCSANOW, &interface);
  close(busses[bus].fd);
}
