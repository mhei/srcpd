/***************************************************************************
                          ddl-s88.c  -  description
                             -------------------
    begin                : Wed Aug 1 2001
    copyright            : (C) 2001 by Dipl.-Ing. Frank Schmischke
    email                : frank.schmischke@t-online.de

    This source based on errdcd-sourcecode by Torsten Vogt.
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
#include <linux/lp.h>
#include <stdio.h>
#include <syslog.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "config-srcpd.h"
#include "ddl-s88.h"
#include "srcp-fb-s88.h"

extern volatile int fb[MAXFBS];
extern int file_descriptor[NUMBER_SERVERS];

//Initializing S88-modules (or compatible) on i.e. printerport
int init_s88(char *name)
{
  unsigned char byte2send;
  int fd;

  fd = open(name, O_RDWR, 0);
  if(fd < 0)
  {
    printf ("couldn't open device.\n");
    return fd;
  }
  byte2send = 0x00;
  write(fd, &byte2send, 1);
  byte2send = 0x02;
  write(fd, &byte2send, 1);
  byte2send = 0x04;
  write(fd, &byte2send, 1);
  byte2send = 0x00;
  write(fd, &byte2send, 1);
  return fd;
}

void load_s88(int fd)
{
  unsigned char byte2send;

  byte2send = 0x02;
  write(fd, &byte2send, 1);
  byte2send = 0x03;
  write(fd, &byte2send, 1);
  byte2send = 0x00;
  write(fd, &byte2send, 1);
  byte2send = 0x04;
  write(fd, &byte2send, 1);
  byte2send = 0x00;
  write(fd, &byte2send, 1);
}

int get_s88(int fd)
{
  int ret, i, tmp;
  unsigned int byte2send;

  ret = 0;

  for(i=0;i<16;i++)
  {
    ret <<= 1;
    ioctl(fd, LPGETSTATUS, &tmp);           // oder lieber LPGETFLAGS ??
    if(tmp & 0x40)
      ret++;
    byte2send = 0x01;
    write(fd, &byte2send, 1);
    byte2send = 0x00;
    write(fd, &byte2send, 1);
  }
  return ret;
}

void clear_s88(int fd)
{
  int i;

  load_s88(fd);
  for(i=0;i<MAXFBS;i++)
    get_s88(fd);
}

void* thr_ddl_s88(void *v)
{
  int i;
  int fd;

  fd = file_descriptor[SERVER_DDL_S88];

  while(1)
  {
    usleep(10000);
    load_s88(fd);
    for(i=0;i<MAXFBS;i++)
    {
      fb[i] = get_s88(fd);
    }
  }
}
