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

#include <libxml/tree.h>

#include "config-srcpd.h"
#include "hsi-88.h"
#include "io.h"
#include "srcp-fb.h"

#define __hsi ((HSI_88_DATA*)busses[busnumber].driverdata)

static int working_HSI88;

void readconfig_HSI_88(xmlDocPtr doc, xmlNodePtr node, int busnumber)
{
  xmlNodePtr child = node->children;

  busses[busnumber].type = SERVER_HSI_88;
  busses[busnumber].init_func = &init_bus_HSI_88;
  busses[busnumber].term_func = &term_bus_HSI_88;
  busses[busnumber].thr_func = &thr_sendrec_HSI_88;
  busses[busnumber].driverdata = malloc(sizeof(struct _HSI_88_DATA));
  strcpy(busses[busnumber].description, "FB POWER");
  __hsi->refresh = 10000;
  __hsi->number_fb[0] = 0;
  __hsi->number_fb[1] = 0;
  __hsi->number_fb[2] = 0;

  while (child)
  {
    if (strncmp(child->name, "text", 4) == 0)
    {
      child = child -> next;
      continue;
    }
    if (strcmp(child->name, "refresh") == 0)
    {
      char *txt = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
      __hsi->refresh = atoi(txt);
      free(txt);
    }
    if (strcmp(child->name, "number_fb_left") == 0)
    {
      char *txt = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
      __hsi->number_fb[0] = atoi(txt);
      free(txt);
    }
    if (strcmp(child->name, "number_fb_center") == 0)
    {
      char *txt = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
      __hsi->number_fb[1] = atoi(txt);
      free(txt);
    }
    if (strcmp(child->name, "number_fb_right") == 0)
    {
      char *txt = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
      __hsi->number_fb[2] = atoi(txt);
      free(txt);
    }
    child = child->next;
  }
}

static int open_lineHSI88(char *name)
{
  int fd;
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
  }
  return fd;
}

static int init_lineHSI88(int busnumber, int modules_left, int modules_center, int modules_right)
{
  int status;
  int i;
  int ctr;
  unsigned char byte2send;
  unsigned char rr;

  sleep(1);
  byte2send = 0x0d;
  for(i=0;i<10;i++)
    writeByte(busnumber, &byte2send, 2);

  while(readByte(busnumber, &rr) == 0)
  {
  }

  // HSI-88 initialize
  // switch off terminal-mode
  i = 1;
  while(i)
  {
    byte2send = 't';
    writeByte(busnumber, &byte2send, 0);
    byte2send = 0x0d;
    writeByte(busnumber, &byte2send, 2);
    rr = 0;
    ctr = 0;
    status = readByte(busnumber, &rr);
    while(rr != 't')
    {
      usleep(100000);
      status = readByte(busnumber, &rr);
      if(status == -1)
        ctr++;
      if(ctr > 20)
        return -1;
    }
    readByte(busnumber, &rr);
    if(rr == '0')
      i = 0;
    readByte(busnumber, &rr);
  }
  // looking for version of HSI
  byte2send = 'v';
  writeByte(busnumber, &byte2send, 0);
  byte2send = 0x0d;
  writeByte(busnumber, &byte2send, 2);

  for(i=0;i<49;i++)
  {
    status = readByte(busnumber, &rr);
    if(status == -1)
      break;
    __hsi->v_text[i] = (char)rr;
  }
  __hsi->v_text[i] = 0x00;
  printf("%s\n", __hsi->v_text);

  status = 1;
  while(status)
  {
    // Modulbelegung initialisieren
    // up to "GO", non feedback-module
    byte2send = 's';
    writeByte(busnumber, &byte2send, 0);
    byte2send = 0;
    writeByte(busnumber, &byte2send, 0);
    writeByte(busnumber, &byte2send, 0);
    writeByte(busnumber, &byte2send, 0);
    byte2send = 0x0d;
    writeByte(busnumber, &byte2send, 0);
    byte2send = 0x0d;
    writeByte(busnumber, &byte2send, 0);
    byte2send = 0x0d;
    writeByte(busnumber, &byte2send, 0);
    byte2send = 0x0d;
    writeByte(busnumber, &byte2send, 5);

    rr = 0;
    readByte(busnumber, &rr);    // read answer (three bytes)
    while(rr != 's')
    {
      usleep(100000);
      readByte(busnumber, &rr);
    }
    readByte(busnumber, &rr);    // Anzahl angemeldeter Module
    status = 0;
  }
  return 0;
}

int init_bus_HSI_88(int busnumber)
{
  int fd;
  int status;
  int anzahl;

  status = 0;
  if(busses[busnumber].type != SERVER_HSI_88)
  {
    status = -1;
  }
  else
  {
    if(busses[busnumber].fd > 0)
      status = -1;              // bus is already in use
  }

  if (status == 0)
  {
    working_HSI88 = 0;
    anzahl  = __hsi->number_fb[0];
    anzahl += __hsi->number_fb[1];
    anzahl += __hsi->number_fb[2];

    if (anzahl > 31)
    {
      printf("number of feedback-modules greater than 31 !!!");
      status = -1;
    }
  }

  if (status == 0)
  {
    fd = open_lineHSI88(busses[busnumber].device);
    if(fd > 0)
    {
      busses[busnumber].fd = fd;
      status = 0;
      status = init_lineHSI88(busnumber, __hsi->number_fb[0],
        __hsi->number_fb[1], __hsi->number_fb[2]);
    }
    else
      status = -1;
  }

  if (status == 0)
    working_HSI88 = 1;

  printf("INIT_BUS_HSI mit Code: %d\n", status);
  return status;
}

int term_bus_HSI_88(int busnumber)
{
  if(busses[busnumber].type != SERVER_HSI_88)
    return 1;

  if(busses[busnumber].pid == 0)
    return 0;

  working_HSI88 = 0;

  pthread_cancel(busses[busnumber].pid);
  busses[busnumber].pid = 0;
  close_comport(busnumber);
  return 0;
}

void* thr_sendrec_HSI_88(void *v)
{
  int busnumber, refresh_time;
  int anzahl, i, temp;
  unsigned char byte2send;
  unsigned char rr;
  int status;
  int zaehler1, fb_zaehler1, fb_zaehler2;

  busnumber = (int)v;
  refresh_time = __hsi->refresh;
  syslog(LOG_INFO, "thr_sendrec_hsi_88 is startet");

  zaehler1 = 0;
  fb_zaehler1 = 0;
  fb_zaehler2 = 1;

  status = 1;
  while(status)
  {
    // Modulbelegung initialisieren
    byte2send = 's';
    writeByte(busnumber, &byte2send, 0);
    byte2send = __hsi->number_fb[0];
    writeByte(busnumber, &byte2send, 0);
    byte2send = __hsi->number_fb[1];
    writeByte(busnumber, &byte2send, 0);
    byte2send = __hsi->number_fb[2];
    writeByte(busnumber, &byte2send, 0);
    byte2send = 0x0d;
    writeByte(busnumber, &byte2send, 0);
    byte2send = 0x0d;
    writeByte(busnumber, &byte2send, 0);
    byte2send = 0x0d;
    writeByte(busnumber, &byte2send, 0);
    byte2send = 0x0d;
    writeByte(busnumber, &byte2send, 5);

    rr = 0;
    readByte(busnumber, &rr);           // read answer (three bytes)
    while(rr != 's')
    {
      usleep(100000);
      readByte(busnumber, &rr);
    }
    readByte(busnumber, &rr);           // Anzahl angemeldeter Module
    anzahl = (int)rr;
    syslog(LOG_INFO, "Anzahl Module: %i", anzahl);
    anzahl -= __hsi->number_fb[0];
    anzahl -= __hsi->number_fb[1];
    anzahl -= __hsi->number_fb[2];
    if(anzahl == 0)                     // HSI initialisation correct ?
    {
      status = 0;
    }
    else
    {
      syslog(LOG_INFO, "Fehler bei Initialisierung");
      sleep(1);
      while(readByte(busnumber, &rr) == 0)
      {
      }
    }
  }

  while(1)
  {
    rr = 0;
    while(rr != 'i')
    {
      usleep(refresh_time);
      readByte(busnumber, &rr);
    }
    readByte(busnumber, &rr);            // Anzahl zu meldender Module
    anzahl = (int)rr;
    for(zaehler1=0;zaehler1<anzahl;zaehler1++)
    {
      readByte(busnumber, &rr);
      rr--;
      i = rr;
      readByte(busnumber, &rr);
      temp = rr;
      temp <<= 8;
      readByte(busnumber, &rr);
      setFBmodul(busnumber, i, temp | rr);
      if (busses[busnumber].debuglevel > 6)
        syslog(LOG_INFO, "Rückmeldung %i mit 0x%02x", i, temp|rr);
    }
    readByte(busnumber, &rr);            // <CR>
  }
}
