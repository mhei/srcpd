/***************************************************************************
                           li100.c  -  description
                             -------------------
    begin                : Tue Jan 22 11:35:13 MEST 2002
    copyright            : (C) 2002 by Dipl.-Ing. Frank Schmischke
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

/***************************************************************************
 *                                                                         *
 *  Changes:                                                               *
 *                                                                         *
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
#include <sys/io.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <linux/serial.h>

#include "config-srcpd.h"
#include "li100.h"
#include "io.h"
#include "srcp-ga.h"
#include "srcp-gl.h"
#include "srcp-time.h"

#ifdef TESTMODE
extern int testmode;
#endif

static volatile struct _GA tga[50];

#if 0
void*
thr_sendrecli100(void *v)
{
  int fd;
  unsigned char byte2send;
  unsigned char rr;
  int status;

  int zaehler1, fb_zaehler1, fb_zaehler2;

#ifndef TESTMODE
  syslog(LOG_INFO, "thr_sendrecli100 gestartet");
#else
  syslog(LOG_INFO, "thr_sendrecli100 gestartet, testmode=%i",testmode);
#endif

  fd = file_descriptor[SERVER_LI100];
  zaehler1 = 0;
  fb_zaehler1 = 0;
  fb_zaehler2 = 1;

  while(1)
  {
  //  syslog(LOG_INFO, "thr_sendrecli100 Start in Schleife");
    /* Start/Stop */
    //fprintf(stderr, "START/STOP... ");
    if(power_changed)
    {
      byte2send = power_state ? 0xA7 : 0xA6;
      writeByte(fd, &byte2send, 250);
      status = readByte(fd, &rr);
      while(status == -1)
      {
        usleep(100000);
        status = readByte(fd, &rr);
      }
      if(rr == 0x00)                  // war alles OK ?
        power_changed = 0;
    }

    send_command_gl_li(fd);
    send_command_ga_li(fd);
    check_status_li(fd);
  }      // Ende WHILE(1)
}

void
send_command_ga_li(int fd)
{
  int i, i1;
  int temp;
  int addr;
  unsigned char byte2send;
  unsigned char status;
  unsigned char rr;
  struct _GA gatmp;
  struct timeval akt_time, cmp_time;

  gettimeofday(&akt_time, NULL);
  // zuerst eventuell Decoder abschalten
  for(i=0;i<50;i++)
  {
    if(tga[i].id)
    {
      syslog(LOG_INFO, "Zeit %i,%i", (int)akt_time.tv_sec, (int)akt_time.tv_usec);
      cmp_time = tga[i].t;
      if(cmpTime(&cmp_time, &akt_time))      // Ausschaltzeitpunkt erreicht ?
      {
        gatmp = tga[i];
        addr = gatmp.id;
        byte2send = 0x90;
        writeByte(fd, &byte2send,0);
        temp = gatmp.id;
        temp &= 0x00FF;
        byte2send = temp;
        writeByte(fd, &byte2send,0);
        temp = gatmp.id;
        temp >>= 8;
        byte2send = temp;
        if(gatmp.port)
        {
          byte2send |= 0x80;
        }
        writeByte(fd, &byte2send, 2);
        readByte(fd, &rr);
        ga[addr]=gatmp;
        tga[i].id=0;
      }
    }
  }

  // Decoder einschalten
  if (!queue_ga_empty()) {
	    unqueueNextGA(&gatmp);
	    addr = gatmp.id;
        byte2send = 0x90;
        writeByte(fd, &byte2send,0);
        temp = gatmp.id;
        temp &= 0x00FF;
        byte2send = temp;
        writeByte(fd, &byte2send,0);
        temp = gatmp.id;
        temp >>= 8;
        byte2send = temp;
        if(gatmp.action)
        {
          byte2send |= 0x40;
        }
        if(gatmp.port)
        {
          byte2send |= 0x80;
        }
        writeByte(fd, &byte2send, 0);
        status = 1;
        if(gatmp.action && (gatmp.activetime > 0))
        {
          for(i1=0;i1<50;i1++)
          {
            if(tga[i1].id == 0)
            {
              gatmp.t = akt_time;
              gatmp.t.tv_sec += gatmp.activetime / 1000;
              gatmp.t.tv_usec += (gatmp.activetime % 1000) * 1000;
              if(gatmp.t.tv_usec > 1000000)
              {
                gatmp.t.tv_sec++;
                gatmp.t.tv_usec -= 1000000;
              }
              tga[i] = gatmp;
              status = 0;
              syslog(LOG_INFO, "GA %i für Abschaltung um %i,%i auf %i", tga[i].id, (int)tga[i].t.tv_sec, (int)tga[i].t.tv_usec, i);
              break;
            }
          }
        }
        readByte(fd, &rr);
        if(status)
        {
          ga[addr] = gatmp;
        }
      }
}

void
send_command_gl_li(int fd)
{
  int i;
  int temp;
  int addr;
  int commands_ok;
  unsigned char byte2send;
  unsigned char status;
  struct _GL gltmp;

  /* Lokdecoder */
  //fprintf(stderr, "LOK's... ");
  /* nur senden, wenn wirklich etwas vorliegt */
  if (!queue_gl_empty()) {
	    unqueueNextGL(&gltmp);
	    addr = gltmp.id;

        // Fahrrichtung, Geschwindigkeit od. eine Funktion geändert ?
        if((gltmp.direction != gl[addr].direction) ||
           (gltmp.speed != gl[addr].speed) ||
           (gltmp.flags != gl[addr].flags))
        {
          // Lokkommando soll gesendet werden
          byte2send = 0x80;
          writeByte(fd, &byte2send,0);
          // lowbyte der Adresse senden
          temp = gltmp.id;
          temp &= 0x00FF;
          byte2send = temp;
          writeByte(fd, &byte2send,0);
          // highbyte der Adresse senden
          temp = gltmp.id;
          temp >>= 8;
          byte2send = temp;
          writeByte(fd, &byte2send,0);
          if(gltmp.direction == 2)       // Nothalt ausgelöst ?
          {
            byte2send = 1;              // Nothalt setzen
          }
          else
          {
            byte2send = calcspeed(gltmp.speed, gltmp.maxspeed, 126); // Geschwindigkeit senden
            if(byte2send > 0)
            {
              byte2send++;
            }
          }
          writeByte(fd, &byte2send,0);
          // Richtung, Licht und Funktionen setzen
          byte2send = gltmp.flags;
          byte2send |= 0xc0;
          if(gltmp.direction)
          {
            byte2send |= 0x20;
          }
          writeByte(fd, &byte2send,2);
          readByte(fd, &status);
          if((status == 0) || (status == 0x41) || (status == 0x42))
          {
            gl[addr] = gltmp;
          }
    }
  }
}

void
check_status_li(int fd)
{
  int i;
  int temp;
  int addr;
  unsigned char byte2send;
  unsigned char rr;
  unsigned char xevnt1, xevnt2, xevnt3;
  struct _GL gltmp;
  struct _GA gatmp;

  /* Abfrage auf Statusänderungen :
     1. Änderungen an S88-Modulen
     2. manuelle Lokbefehle
     3. manuelle Weichenbefehle */
  byte2send = 0xC8;
  writeByte(fd, &byte2send, 2);
  xevnt2 = 0x00;
  xevnt3 = 0x00;
  readByte(fd, &xevnt1);
  if(xevnt1 & 0x80)
  {
    readByte(fd, &xevnt2);
    if(xevnt2 & 0x80)
      readByte(fd, &xevnt3);
  }

  if(xevnt1 & 0x01)        // mindestens eine Lok wurde von Hand gesteuert
  {
    byte2send = 0xC9;
    writeByte(fd, &byte2send,2);
    readByte(fd, &rr);
    while(rr != 0x80)
    {
      if(rr == 1)
      {
        gltmp.speed = 0;        // Lok befindet sich im Nothalt
        gltmp.direction = 2;
      }
      else
      {
        gltmp.speed = rr;        // Lok fährt mit dieser Geschwindigkeit
        gltmp.direction = 0;
        if(gltmp.speed > 0)
          gltmp.speed--;
      }
      readByte(fd, &rr);
      gltmp.flags = rr & 0xf0;;
      readByte(fd, &rr);
      gltmp.id = rr;
      readByte(fd, &rr);
      if((rr & 0x80) && (gltmp.direction == 0))
        gltmp.direction = 1;    // Richtung ist vorwärts
      if(rr & 0x40)
        gltmp.flags |= 0x010;    // Licht ist an
      rr &= 0x3F;
      gltmp.id |= rr << 8;
      readByte(fd, &rr);
      readByte(fd, &rr);
    }
  }

  if(xevnt1 & 0x04)        // mindestens eine Rückmeldung hat sich geändert
  {
    byte2send = 0xCB;
    writeByte(fd, &byte2send,2);
    readByte(fd, &rr);
    while(rr != 0x00)
    {
      rr--;
      i = rr;
      readByte(fd, &rr);
      temp = rr;
      temp <<= 8;
      readByte(fd, &rr);
      fb[i] = temp | rr;
      readByte(fd, &rr);
      syslog(LOG_INFO, "Rückmeldung %i mit 0x%02x", i, fb[i]);
    }
  }

  if(xevnt1 & 0x20)        // mindestens eine Weiche wurde von Hand geschaltet
  {
    byte2send = 0xCA;
    writeByte(fd, &byte2send,0);
    readByte(fd, &rr);
    temp = rr;
    for(i=0;i<temp;i++)
    {
      readByte(fd, &rr);
      gatmp.id = rr;
      readByte(fd, &rr);
      gatmp.id |= (rr & 0x07) << 8;
    }
  }

  if(xevnt2 & 0x40)        // we should send an XStatus-command
  {
    byte2send = 0xA2;
    writeByte(fd, &byte2send,2);
    readByte(fd, &rr);
    if(rr & 0x80)
      readByte(fd, &rr);
  }
}

int
send_command(int fd, char *str)
{
  int ctr, i;
  int status;

  str[19] = 0x00;                   // control-byte for xor
  ctr = str[0] & 0x0f;              // generate length of command
  ctr++;
  for(i=0;i<ctr;i++)                // send command
  {
    str[19] ^= str[i];
    writeByte(fd, &str[i], 0);
  }

  status = -1;                      // wait for answer
  while(status == -1)
  {
    usleep(2000);
    status = readByte(fd, &str[0]);
  }
  ctr = str[0] & 0x0f;              // generate length of answer
  ctr += 2;
  for(i=1;i<ctr;i++)                // read answer
    readByte(fd, &str[i]);

//9216 PRINT #2, SEND$; : IST$ = "--": GOSUB 9000           'senden
//9218 '
//9220 IF IST$ = "OK" THEN RETURN                           'alles klar
//9222 IF IST$ = "TA" THEN GOSUB 9240                       'fehler timeout
//9224 IF IST$ = "PC" THEN GOSUB 9250                       'fehler PC -> LI
//9226 IF IST$ = "LZ" THEN GOSUB 9260                       'fehler LI -> LZ
//9228 IF IST$ = "??" THEN GOSUB 9270                       'befehl unbekannt
//9230 IF IST$ = "KA" THEN GOSUB 9280                       'keine antwort
//9232 RETURN
}

int
init_lineLI100(char *name)
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
  }
}

#endif
