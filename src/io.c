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
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <termios.h>
#include <unistd.h>

#include "io.h"

struct termios interface_org;
int org_status_saved = 0;

#ifdef TESTMODE
extern int testmode;
#endif

int readByte(int FD, unsigned char *the_byte)
{
  int i;

#ifdef TESTMODE
  if(testmode)
  {	
    i = 1;
    *the_byte = 0;
  }
  else
  {
    i = read(FD, the_byte, 1);
    if(i == 1)
    {
      syslog(LOG_INFO, "Byte gelesen : 0x%02x", *the_byte);
    }
    else
    {
      syslog(LOG_INFO, "kein Byte empfangen");
    }
  }
#else
  i = read(FD, the_byte, 1);
#endif
  return (i == 1 ? 0 : -1);
}

void writeByte(int FD, unsigned char *b, unsigned long msecs)
{
#ifdef TESTMODE
  syslog(LOG_INFO, "Byte geschrieben : 0x%02x", *b);
  if(testmode == 0)
  {
    write(FD, b, 1);
    tcflush(FD, TCOFLUSH);
    usleep(msecs * 1000);
  }
#else	
  write(FD, b, 1);
  tcflush(FD, TCOFLUSH);
  usleep(msecs * 1000);
#endif
}

void save_comport(char *name)
{
  int fd;

  printf("Saveing attribute for serial line %s\n", name);
  fd=open(name, O_RDWR);
  if (fd == -1)
  {
    printf("dammit, couldn't open device.\n");
  }
  else
  {
    tcgetattr(fd, &interface_org);
    org_status_saved = 1;
    close(fd);
  }
}

void restore_comport(char *name)
{
  int fd;

  syslog(LOG_INFO, "Restoreing attributes for serial line %s", name);
  fd=open(name,O_RDWR);
  if (fd == -1)
  {
    syslog(LOG_INFO, "dammit, couldn't open device.");
  }
  else
  {
    syslog(LOG_INFO, "alte Werte werden wiederhergestellt");
    tcsetattr(fd, TCSANOW, &interface_org);
    close(fd);
    syslog(LOG_INFO, "erfolgreich wiederhergestellt");
  }
}
