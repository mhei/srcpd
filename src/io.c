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

int
readByte(int bus, unsigned char *the_byte)
{
  int i;

  ioctl(busses[bus].fd, FIONREAD, &i);
  syslog(LOG_INFO, "Bytes zu lesen: %d", i);
  if(busses[bus].debuglevel > 0)
  {  
    i = 1;
    *the_byte = 0;
  }
  else
  {
    i = read(busses[bus].fd, the_byte, 1);
    if(i == 1)
    {
      syslog(LOG_INFO, "Byte gelesen : 0x%02x", *the_byte);
    }
    else
    {
      syslog(LOG_INFO, "kein Byte empfangen");
    }
  }
  return (i == 1 ? 0 : -1);
}

void
writeByte(int bus, unsigned char *b, unsigned long msecs)
{
  if(busses[bus].debuglevel == 0)
  {
    write(busses[bus].fd, b, 1);
    tcflush(busses[bus].fd, TCOFLUSH);
  } else {
    if(busses[bus].debuglevel > 1)
      syslog(LOG_INFO, "Byte geschrieben : 0x%02x", *b);
  }
  usleep(msecs * 1000);
}

void
save_comport(int businfo)
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

void
restore_comport(int bus)
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

void
close_comport(int bus)
{
  struct termios interface;
  syslog(LOG_INFO, "Closing serial line");

  tcgetattr(busses[bus].fd, &interface);
  cfsetispeed(&interface, B0);
  cfsetospeed(&interface, B0);
  tcsetattr(busses[bus].fd, TCSANOW, &interface);
  close(busses[bus].fd);
}
