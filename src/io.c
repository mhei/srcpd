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
    if(status < 0)
      DBG(bus, DBG_ERROR, "IOCTL   status: %d with errno = %d", status, errno);

    // read only, if there is really an input
    if ((i > 0) || (wait == 1))
    {
      DBG(bus, DBG_DEBUG, "on bus %d (fd = %d), there are %d bytes to read.", bus, busses[bus].fd, i);
      i = read(busses[bus].fd, the_byte, 1);
      if(i < 0)
        DBG(bus, DBG_ERROR, "READ    status: %d with errno = %d", i, errno);
        if (i > 0)
          DBG(bus, DBG_DEBUG, "bus %d byte read: 0x%02x", bus, *the_byte);
      }
  }
  return (i > 0 ? 0 : -1);
}

void writeByte(int bus, unsigned char *b, unsigned long msecs)
{
  if(busses[bus].debuglevel <= DBG_DEBUG)
  {
    write(busses[bus].fd, b, 1);
    tcdrain(busses[bus].fd);
  }
  DBG(bus, DBG_DEBUG, "bus %d (FD: %d) byte sent: 0x%02x", bus, busses[bus].fd, *b);
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
