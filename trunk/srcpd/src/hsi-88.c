/***************************************************************************
                          hsi-88.c  -  description
                             -------------------
    begin                : Mon Oct 29 2001
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

#include <netdb.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <termios.h>
#include <unistd.h>

#include "config-srcpd.h"
#include "hsi-88.h"
#include "io.h"
#include "srcp-fb.h"

int
init_bus_HSI_88(int bus)
{
  return 0;
}

int
term_bus_HSI_88(int bus)
{
  return 0;
}

int
init_lineHSI88(char *name)
{
  int status;
  int fd;
  int i;
  int ctr;
  unsigned char byte2send;
  unsigned char rr;
  char v_text[50];
  struct termios interface;

  printf("try opening serial line %s for 9600 baud\n", name);
  fd = open(name, O_RDWR);
  if (fd == -1)
  {
    printf("dammit, couldn't open device.\n");
  }
  else
  {
    tcgetattr(fd, &interface);
    interface.c_oflag = ONOCR;
    interface.c_cflag = CS8 | CRTSCTS | CSTOPB | CLOCAL | CREAD | HUPCL;
    interface.c_iflag = IGNBRK;
    interface.c_lflag = IEXTEN;
    cfsetispeed(&interface, B9600);
    cfsetospeed(&interface, B9600);
    interface.c_cc[VMIN] = 0;
    interface.c_cc[VTIME] = 1;
    tcsetattr(fd, TCSANOW, &interface);
    status = 0;
    sleep(1);
    byte2send = 0x0d;
    for(i=0;i<10;i++)
      writeByte(fd, &byte2send, 2);
#ifdef TESTMODE
    if(testmode == 0)
#endif
    while(readByte(fd, &rr) == 0)
    {
    }

    // HSI-88 initialisieren
    // ausschalten Terminalmode
    i = 1;
    while(i)
    {
      byte2send = 't';
      writeByte(fd, &byte2send, 0);
      byte2send = 0x0d;
      writeByte(fd, &byte2send, 2);
      rr = 0;
      ctr = 0;
      status = readByte(fd, &rr);
      while(rr != 't')
      {
        usleep(100000);
        status = readByte(fd, &rr);
        if(status == -1)
          ctr++;
        if(ctr > 20)
          return -1;
      }
      readByte(fd, &rr);
      if(rr == '0')
        i = 0;
      readByte(fd, &rr);
    }
    // Version abfragen
    byte2send = 'v';
    writeByte(fd, &byte2send, 0);
    byte2send = 0x0d;
    writeByte(fd, &byte2send, 2);

    for(i=0;i<49;i++)
    {
      status = readByte(fd, &rr);
      if(status == -1)
        break;
      v_text[i] = (char)rr;
    }
    v_text[i] = 0x00;
    printf("%s\n", v_text);
  }
  return fd;
}

void*
thr_sendrec_HSI_88(void *v)
{
  int fd, bus;
  int anzahl, i, temp;
  unsigned char byte2send;
  unsigned char rr;
  int status;
  int zaehler1, fb_zaehler1, fb_zaehler2;

  bus = (int)v;
#ifndef TESTMODE
  syslog(LOG_INFO, "thr_sendrec_hsi_88 gestartet");
#else
  syslog(LOG_INFO, "thr_sendrec_hsi_88 gestartet, testmode=%i",testmode);
#endif

  fd = busses[bus].fd;
  zaehler1 = 0;
  fb_zaehler1 = 0;
  fb_zaehler2 = 1;

  status = 1;
  while(status)
  {
    // Modulbelegung initialisieren
    byte2send = 's';
    writeByte(fd, &byte2send, 0);
    byte2send = busses[bus].number_fb[0];
    writeByte(fd, &byte2send, 0);
    byte2send = busses[bus].number_fb[1];
    writeByte(fd, &byte2send, 0);
    byte2send = busses[bus].number_fb[2];
    writeByte(fd, &byte2send, 0);
    byte2send = 0x0d;
    writeByte(fd, &byte2send, 0);
    byte2send = 0x0d;
    writeByte(fd, &byte2send, 0);
    byte2send = 0x0d;
    writeByte(fd, &byte2send, 0);
    byte2send = 0x0d;
    writeByte(fd, &byte2send, 5);

    rr = 0;
    readByte(fd, &rr);            // Antwort lesen drei Byte
    while(rr != 's')
    {
      usleep(100000);
      readByte(fd, &rr);
    }
    readByte(fd, &rr);            // Anzahl angemeldeter Module
    anzahl = (int)rr;
    syslog(LOG_INFO, "Anzahl Module: %i", anzahl);
    anzahl -= busses[bus].number_fb[0];
    anzahl -= busses[bus].number_fb[1];
    anzahl -= busses[bus].number_fb[2];
    if(anzahl == 0)         // HSI initialisation correct ?
    {
      status = 0;
    }
    else
    {
      syslog(LOG_INFO, "Fehler bei Initialisierung");
      sleep(1);
      while(readByte(fd, &rr) == 0)
      {
      }
    }
  }

  while(1)
  {
    rr = 0;
    while(rr != 'i')
    {
      usleep(10000);
      readByte(fd, &rr);
    }
    readByte(fd, &rr);            // Anzahl zu meldender Module
    anzahl = (int)rr;
    for(zaehler1=0;zaehler1<anzahl;zaehler1++)
    {
      readByte(fd, &rr);
      rr--;
      i = rr;
      readByte(fd, &rr);
      temp = rr;
      temp <<= 8;
      readByte(fd, &rr);
      setFBmodul(bus, i, temp | rr);
      syslog(LOG_INFO, "Rückmeldung %i mit 0x%02x", i, temp|rr);
    }
    readByte(fd, &rr);            // <CR>
#ifdef TESTMODE
    if(testmode)
    {
      zaehler1++;
      if(zaehler1 > 10000)
      {
        zaehler1 = 0;
        fb[fb_zaehler1] = fb_zaehler2;
        if(fb_zaehler2 == 0)
        {
          fb_zaehler2 = 0x8000;
          fb_zaehler1++;
          if(fb_zaehler1 > 30)
            fb_zaehler1 = 0;
        }
        else
          fb_zaehler2 >>= 1;
      }
    }
#endif
  }
}
