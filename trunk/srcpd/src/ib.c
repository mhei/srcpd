/***************************************************************************
                            ib.c  -  description
                             -------------------
    begin                : Don Apr 19 17:35:13 MEST 2001
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

/***************************************************************************
 *                                                                         *
 *  Changes:                                                               *
 *                                                                         *
 *  17.01.2002 Frank Schmischke                                            *
 *   - using of kernelmodul/-device "ibox"                                 *
 *                                                                         *
 ***************************************************************************/

#include <stdincludes.h>

#ifdef linux
#include <linux/serial.h>
#include <sys/io.h>
#endif


#include "config-srcpd.h"
#include "ib.h"
#include "io.h"
#include "srcp-ga.h"
#include "srcp-gl.h"
#include "srcp-sm.h"
#include "srcp-time.h"
#include "srcp-fb.h"
#include "srcp-power.h"
#include "srcp-info.h"

#ifdef linux
#include "ibox/ibox.h"
#endif

#define __ib ((IB_DATA*)busses[busnumber].driverdata)

static struct _GASTATE tga[MAX_BUSSES][50];
static int working_IB;

static int last_type[MAX_BUSSES];
static int last_typeaddr[MAX_BUSSES];
static int last_bit[MAX_BUSSES];

static int init_line_IB(int);

void readconfig_intellibox(xmlDocPtr doc, xmlNodePtr node, int busnumber)
{
  xmlNodePtr child = node->children;
  DBG(busnumber, DBG_INFO, "reading configuration for intellibox at bus %d", busnumber);

  busses[busnumber].type = SERVER_IB;
  busses[busnumber].init_func = &init_bus_IB;
  busses[busnumber].term_func = &term_bus_IB;
  busses[busnumber].thr_func = &thr_sendrec_IB;
  busses[busnumber].driverdata = malloc(sizeof(struct _IB_DATA));
  busses[busnumber].flags |= FB_16_PORTS;
  busses[busnumber].baudrate = B38400;
  busses[busnumber].numberOfSM = 9999;
  strcpy(busses[busnumber].description, "GA GL FB SM POWER LOCK DESCRIPTION");

  __ib->number_fb = 0;            // max. 31 for S88; Loconet is missing this time
  __ib->number_ga = 256;
  __ib->number_gl = 80;

  while (child)
  {
    if (strcmp(child->name, "number_fb") == 0)
    {
      char *txt = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
      __ib->number_fb = atoi(txt);
      free(txt);
    }

    if (strcmp(child->name, "number_gl") == 0)
    {
      char *txt = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
      __ib->number_gl = atoi(txt);
      free(txt);
    }
    if (strcmp(child->name, "number_ga") == 0)
    {
      char *txt = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
      __ib->number_ga = atoi(txt);
      free(txt);
    }
    child = child->next;
  }

  if(init_GA(busnumber, __ib->number_ga))
  {
    __ib->number_ga = 0;
    DBG(busnumber, DBG_ERROR, "Can't create array for assesoirs");
  }

  if(init_GL(busnumber, __ib->number_gl))
  {
    __ib->number_gl = 0;
    DBG(busnumber, DBG_ERROR, "Can't create array for locomotivs");
  }
  if(init_FB(busnumber, __ib->number_fb*16))
  {
    __ib->number_fb = 0;
    DBG(busnumber, DBG_ERROR, "Can't create array for feedback");
  }
}

int cmpTime(struct timeval *t1, struct timeval *t2)
{
  int result;

  result = 0;
  if(t2->tv_sec > t1->tv_sec)
  {
    result = 1;
  }
  else
  {
    if(t2->tv_sec == t1->tv_sec)
    {
      if(t2->tv_usec > t1->tv_usec)
      {
        result = 1;
      }
    }
  }
  return result;
}

int init_bus_IB(int busnumber)
{
  int status;

  status = 0;
  printf("Bus %d mit Debuglevel %d\n", busnumber, busses[busnumber].debuglevel);
  if(busses[busnumber].type != SERVER_IB)
  {
    status = -2;
  }
  else
  {
    if(busses[busnumber].fd > 0)
      status = -3;              // bus is already in use
  }

  if (status == 0)
  {
    working_IB = 0;
  }

  if (busses[busnumber].debuglevel < 7)
  {
    if (status == 0)
      status = init_line_IB(busnumber);
  }
  else
     busses[busnumber].fd = 9999;
  if (status == 0)
    working_IB = 1;

  printf("INIT_BUS_IB exited with code: %d\n", status);
  last_type[busnumber] = -1;
  return status;
}

int term_bus_IB(int busnumber)
{
  if(busses[busnumber].type != SERVER_IB)
    return 1;

  if(busses[busnumber].pid == 0)
    return 0;

  working_IB = 0;

  pthread_cancel(busses[busnumber].pid);
  busses[busnumber].pid = 0;
  close_comport(busnumber);
  return 0;
}

void* thr_sendrec_IB(void *v)
{
  unsigned char byte2send;
  int status;
  unsigned char rr;
  int busnumber;
  int zaehler1, fb_zaehler1, fb_zaehler2;

  busnumber = (int) v;
    DBG(busnumber, DBG_INFO, "thr_sendrec_IB is startet as bus %i", busnumber);

  // initialize tga-structure
  for(zaehler1=0;zaehler1<50;zaehler1++)
    tga[busnumber][zaehler1].id = 0;
    
  fb_zaehler1 = 0;
  fb_zaehler2 = 1;

  while(1)
  {
    if(busses[busnumber].power_changed)
    {
      char msg[110];
      byte2send = busses[busnumber].power_state ? 0xA7 : 0xA6;
      writeByte(busnumber, byte2send, 250);
      status = readByte(busnumber, 1, &rr);
      while(status == -1)
      {
        usleep(100000);
        status = readByte(busnumber, 1, &rr);
      }
      if(rr == 0x00)                  // war alles OK ?
        busses[busnumber].power_changed = 0;
      if(rr == 0x06)                  // power on not possible - overheating
      {
        busses[busnumber].power_changed = 0;
        busses[busnumber].power_state   = 0;
      }
      infoPower(busnumber, msg);
      queueInfoMessage(msg);
    }

    if(busses[busnumber].power_state==0)
    {
      usleep(1000);
      continue;
    }

    send_command_gl_ib(busnumber);
    send_command_ga_ib(busnumber);
    check_status_ib(busnumber);
    send_command_sm_ib(busnumber);
    usleep(50000);
  }      // Ende WHILE(1)
}

void send_command_ga_ib(int busnumber)
{
  int i, i1;
  int temp;
  int addr;
  unsigned char byte2send;
  unsigned char status;
  unsigned char rr;
  struct _GASTATE gatmp;
  struct timeval akt_time, cmp_time;

  gettimeofday(&akt_time, NULL);
  // zuerst eventuell Decoder abschalten
  for(i=0;i<50;i++)
  {
    if(tga[busnumber][i].id)
    {
      DBG(busnumber, DBG_DEBUG, "Zeit %i,%i", (int)akt_time.tv_sec, (int)akt_time.tv_usec);
      cmp_time = tga[busnumber][i].t;
      if(cmpTime(&cmp_time, &akt_time))      // Ausschaltzeitpunkt erreicht ?
      {
        gatmp = tga[busnumber][i];
        addr = gatmp.id;
        byte2send = 0x90;
        writeByte(busnumber, byte2send, 0);
        temp = gatmp.id;
        temp &= 0x00FF;
        byte2send = temp;
        writeByte(busnumber, byte2send, 0);
        temp = gatmp.id;
        temp >>= 8;
        byte2send = temp;
        if(gatmp.port)
        {
          byte2send |= 0x80;
        }
        writeByte(busnumber, byte2send, 2);
        readByte(busnumber, 1, &rr);
        gatmp.action=0;
        setGA(busnumber, addr, gatmp);
        tga[busnumber][i].id=0;
      }
    }
  }

  // Decoder einschalten
  if(! queue_GA_isempty(busnumber))
  {
    unqueueNextGA(busnumber, &gatmp);
    addr = gatmp.id;
    byte2send = 0x90;
    writeByte(busnumber, byte2send, 0);
    temp = gatmp.id;
    temp &= 0x00FF;
    byte2send = temp;
    writeByte(busnumber, byte2send, 0);
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
    writeByte(busnumber, byte2send, 2);
    status = 0;
    // reschedule event: turn off --tobedone--
    if(gatmp.action && (gatmp.activetime > 0))
    {
      status = 1;
      for(i1=0;i1<50;i1++)
      {
        if(tga[busnumber][i1].id == 0)
        {
          gatmp.t = akt_time;
          gatmp.t.tv_sec += gatmp.activetime / 1000;
          gatmp.t.tv_usec += (gatmp.activetime % 1000) * 1000;
          if(gatmp.t.tv_usec > 1000000)
          {
            gatmp.t.tv_sec++;
            gatmp.t.tv_usec -= 1000000;
          }
          tga[busnumber][i1] = gatmp;
          DBG(busnumber, DBG_DEBUG, "GA %i für Abschaltung um %i,%i auf %i", tga[busnumber][i1].id,
            (int)tga[busnumber][i1].t.tv_sec, (int)tga[busnumber][i1].t.tv_usec, i1);
          break;
        }
      }
    }
    readByte(busnumber, 1, &rr);
    if(status)
    {
      setGA(busnumber, addr, gatmp);
    }
  }
}

void send_command_gl_ib(int busnumber)
{
  int temp;
  int addr=0;
  unsigned char byte2send;
  unsigned char status;
  struct _GLSTATE gltmp, glakt;

  /* Lokdecoder */
  //fprintf(stderr, "LOK's... ");
  /* nur senden, wenn wirklich etwas vorliegt */
  if(! queue_GL_isempty(busnumber))
  {
    unqueueNextGL(busnumber, &gltmp);
    addr  = gltmp.id;
    getGL(busnumber, addr, &glakt);

    // speed, direction or function changed ?
    if((gltmp.direction != glakt.direction) ||
       (gltmp.speed != glakt.speed) ||
       (gltmp.funcs != glakt.funcs))
    {
      // Lokkommando soll gesendet werden
      byte2send = 0x80;
      writeByte(busnumber, byte2send, 0);
      // send lowbyte of adress
      temp = gltmp.id;
      temp &= 0x00FF;
      byte2send = temp;
      writeByte(busnumber, byte2send, 0);
      // send highbyte of adress
      temp = gltmp.id;
      temp >>= 8;
      byte2send = temp;
      writeByte(busnumber, byte2send, 0);
      if(gltmp.direction == 2)       // Nothalt ausgelöst ?
      {
        byte2send = 1;              // Nothalt setzen
      }
      else
      {
		
		// IB scales speeds INTERNALLY down!
		// but gltmp.speed can already contain down-scaled speed
		
		// IB has general range of 0..127, independent of decoder type!
		byte2send = (unsigned char) ((gltmp.speed * 126) / glakt.n_fs);
		
		//printf("send_cmd_gl(): speed = %d, n_fs = %d\n", gltmp.speed, glakt.n_fs); 
		//printf("send_cmd_gl(): sending speed %d to IB\n", byte2send);
		
        if(byte2send > 0)
        {
          byte2send++;
        }
      }
      writeByte(busnumber, byte2send, 0);
      // setting direction, light and function
      byte2send = (gltmp.funcs >> 1) + (gltmp.funcs & 0x01?0x10:0);
      byte2send |= 0xc0;
      if(gltmp.direction)
      {
        byte2send |= 0x20;
      }
      writeByte(busnumber, byte2send, 2);
      readByte(busnumber, 1, &status);
      if((status == 0) || (status == 0x41) || (status == 0x42))
      {
        setGL(busnumber, addr, gltmp);
      }
    }
  }
}

int read_register(int busnumber, int reg)
{
  unsigned char byte2send;
  unsigned char status;

  byte2send = 0xEC;
  writeByte(busnumber, byte2send, 0);
  byte2send = reg;
  writeByte(busnumber, byte2send, 0);
  byte2send = 0;
  writeByte(busnumber, byte2send, 2);

  readByte(busnumber, 1, &status);

  return status;
}

int read_cv(int busnumber, int cv)
{
  unsigned char byte2send;
  unsigned char status;
  int tmp;

  byte2send = 0xF0;
  writeByte(busnumber, byte2send, 0);
  // low-byte of cv
  tmp = cv & 0xFF;
  byte2send = tmp;
  writeByte(busnumber, byte2send, 0);
  // high-byte of cv
  tmp = cv >> 8;
  byte2send = tmp;
  writeByte(busnumber, byte2send, 2);

  readByte(busnumber, 1, &status);

  return status;
}

int read_cvbit(int busnumber, int cv, int bit)
{
  unsigned char byte2send;
  unsigned char status;
  int tmp;

  byte2send = 0xF2;
  writeByte(busnumber, byte2send, 0);
  // low-byte of cv
  tmp = cv & 0xFF;
  byte2send = tmp;
  writeByte(busnumber, byte2send, 0);
  // high-byte of cv
  tmp = cv >> 8;
  byte2send = tmp;
  writeByte(busnumber, byte2send, 2);

  readByte(busnumber, 1, &status);

  return status;
}

int write_register(int busnumber, int reg, int value)
{
  unsigned char byte2send;
  unsigned char status;

  byte2send = 0xED;
  writeByte(busnumber, byte2send, 0);
  byte2send = reg;
  writeByte(busnumber, byte2send, 0);
  byte2send = 0;
  writeByte(busnumber, byte2send, 0);
  byte2send = value;
  writeByte(busnumber, byte2send, 2);

  readByte(busnumber, 1, &status);

  return status;
}

int write_cv(int busnumber, int cv, int value)
{
  unsigned char byte2send;
  unsigned char status;
  int tmp;

  byte2send = 0xF1;
  writeByte(busnumber, byte2send, 0);
  // low-byte of cv
  tmp = cv & 0xFF;
  byte2send = tmp;
  writeByte(busnumber, byte2send, 0);
  // high-byte of cv
  tmp = cv >> 8;
  byte2send = tmp;
  writeByte(busnumber, byte2send, 0);
  byte2send = value;
  writeByte(busnumber, byte2send, 200);

  readByte(busnumber, 1, &status);

  return status;
}

int write_cvbit(int busnumber, int cv, int bit, int value)
{
  unsigned char byte2send;
  unsigned char status;
  int tmp;

  byte2send = 0xF3;
  writeByte(busnumber, byte2send, 0);
  // low-byte of cv
  tmp = cv & 0xFF;
  byte2send = tmp;
  writeByte(busnumber, byte2send, 0);
  // high-byte of cv
  tmp = cv >> 8;
  byte2send = tmp;
  writeByte(busnumber, byte2send, 0);
  byte2send = bit;
  writeByte(busnumber, byte2send, 0);
  byte2send = value;
  writeByte(busnumber, byte2send, 2);

  readByte(busnumber, 1, &status);

  return status;
}

// program decoder on the main
int send_pom(int busnumber, int addr, int cv, int value)
{
  unsigned char byte2send;
  unsigned char status;
  int ret_val;
  int tmp;

  // send pom-command
  byte2send = 0xDE;
  writeByte(busnumber, byte2send, 0);
  // low-byte of decoder-adress
  tmp = addr & 0xFF;
  byte2send = tmp;
  writeByte(busnumber, byte2send, 0);
  // high-byte of decoder-adress
  tmp = addr >> 8;
  byte2send = tmp;
  writeByte(busnumber, byte2send, 0);
  // low-byte of cv
  tmp = cv & 0xff;
  byte2send = tmp;
  writeByte(busnumber, byte2send, 0);
  // high-byte of cv
  tmp = cv >> 8;
  byte2send = tmp;
  writeByte(busnumber, byte2send, 0);
  byte2send = value;
  writeByte(busnumber, byte2send, 2);

  readByte(busnumber, 1, &status);

  ret_val = 0;
  if (status != 0)
    ret_val = -1;
  return ret_val;
}

void send_command_sm_ib(int busnumber)
{
  //unsigned char byte2send;
  //unsigned char status;
  struct _SM smakt;

  /* Lokdecoder */
  //fprintf(stderr, "LOK's... ");
  /* nur senden, wenn wirklich etwas vorliegt */
  if(! queue_SM_isempty(busnumber))
  {
    unqueueNextSM(busnumber, &smakt);

    last_type[busnumber]     = smakt.type;
    last_typeaddr[busnumber] = smakt.typeaddr;
    last_bit[busnumber]      = smakt.bit;

    DBG(busnumber, DBG_DEBUG, "in send_command_sm: last_type[%d] = %d", busnumber, last_type[busnumber]);
    switch (smakt.command)
    {
      case SET:
        if (smakt.addr == -1)
        {
          switch (smakt.type)
          {
            case REGISTER:
              write_register(busnumber, smakt.typeaddr, smakt.value);
              break;
            case CV:
              write_cv(busnumber, smakt.typeaddr, smakt.value);
              break;
            case CV_BIT:
              write_cvbit(busnumber, smakt.typeaddr, smakt.bit, smakt.value);
              break;
          }
        }
        else
        {
          send_pom(busnumber, smakt.addr, smakt.typeaddr, smakt.value);
        }
        break;
      case GET:
        switch (smakt.type)
        {
          case REGISTER:
            read_register(busnumber, smakt.typeaddr);
            break;
          case CV:
            read_cv(busnumber, smakt.typeaddr);
            break;
          case CV_BIT:
            read_cvbit(busnumber, smakt.typeaddr, smakt.bit);
            break;
        }
        break;
      case VERIFY:
        break;
    }
  }
}

void check_status_ib(int busnumber)
{
  int i;
  int temp;
  unsigned char byte2send;
  unsigned char rr;
  unsigned char xevnt1, xevnt2, xevnt3;
  struct _GLSTATE gltmp, glakt;
  struct _GASTATE gatmp;

  /* Abfrage auf Statusänderungen :
     1. Änderungen an S88-Modulen
     2. manuelle Lokbefehle
     3. manuelle Weichenbefehle */

//#warning add loconet

  byte2send = 0xC8;
  writeByte(busnumber, byte2send, 2);
  xevnt2 = 0x00;
  xevnt3 = 0x00;
  readByte(busnumber, 1, &xevnt1);
  if(xevnt1 & 0x80)
  {
    readByte(busnumber, 1, &xevnt2);
    if(xevnt2 & 0x80)
      readByte(busnumber, 1, &xevnt3);
  }

  if(xevnt1 & 0x01)        // mindestens eine Lok wurde von Hand gesteuert
  {
    byte2send = 0xC9;
    writeByte(busnumber, byte2send, 2);
    readByte(busnumber, 1, &rr);
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
      // 2. byte functions
      readByte(busnumber, 1, &rr);
      gltmp.funcs = rr & 0xf0;
      // 3. byte adress (low-part A7..A0)
      readByte(busnumber, 1, &rr);
      gltmp.id = rr;
      // 4. byte adress (high-part A13..A8), direction, light
      readByte(busnumber, 1, &rr);
      if((rr & 0x80) && (gltmp.direction == 0))
        gltmp.direction = 1;    // Richtung ist vorwärts
      if(rr & 0x40)
        gltmp.funcs |= 0x010;    // Licht ist an
      rr &= 0x3F;
      gltmp.id |= rr << 8;
	  
	  //printf("got GL data from IB: lok# = %d, speed = %d, dir = %d\n", 
	  //	gltmp.id, gltmp.speed, gltmp.direction); 

      // initialize the GL if not done by user,
	  // because IB can report uninited GLs...
	  if (!isInitializedGL(busnumber, gltmp.id))
	  {
		  DBG(busnumber, DBG_INFO, "IB reported unintialized GL. performing default init for %d", gltmp.id);
		  initGL(busnumber, gltmp.id, 'P', 1, 126, 5);
	  }
      // get old data, to know which FS the user wants to have...
	  getGL(busnumber, gltmp.id, &glakt);
	  // recalc speed
	  gltmp.speed = (gltmp.speed * glakt.n_fs) / 126; 
      setGL(busnumber, gltmp.id, gltmp);
	  
	  // 5. byte real speed (is ignored)
      readByte(busnumber, 1, &rr);
	  //printf("speed reported in 5th byte: speed = %d\n", rr);
	  
      // next 1. byte
      readByte(busnumber, 1, &rr);
    }
  }

  if(xevnt1 & 0x04)        // mindestens eine Rückmeldung hat sich geändert
  {
    byte2send = 0xCB;
    writeByte(busnumber, byte2send, 2);
    readByte(busnumber, 1, &rr);
    while(rr != 0x00)
    {
      int aktS88 = rr;
      readByte(busnumber, 1, &rr);
      temp = rr;
      temp <<= 8;
      readByte(busnumber, 1, &rr);
      setFBmodul(busnumber, aktS88+1, temp|rr);
      readByte(busnumber, 1, &rr);
    }
  }

  if(xevnt1 & 0x20)        // mindestens eine Weiche wurde von Hand geschaltet
  {
    byte2send = 0xCA;
    writeByte(busnumber, byte2send, 2);
    readByte(busnumber, 1, &rr);
    temp = rr;
    for(i=0;i<temp;i++)
    {
      readByte(busnumber, 1, &rr);
      gatmp.id = rr;
      readByte(busnumber, 1, &rr);
      gatmp.id    |= (rr & 0x07) << 8;
      gatmp.port   = (rr & 0x80) ? 1 : 0;
      gatmp.action = (rr & 0x40) ? 1 : 0;
      setGA(busnumber, gatmp.id, gatmp);
    }
  }

  if(xevnt2 & 0x3f)       // overheat, short on track etc.
  {
    DBG(busnumber, DBG_DEBUG, "on bus %i short detected; old-state is %i", busnumber, getPower(busnumber));
    if(getPower(busnumber))
    {
      char msg[500];
      if(xevnt2 & 0x20)
        setPower(busnumber, -1, "Overheating condition detected");
      if(xevnt2 & 0x10)
        setPower(busnumber, -1, "non-allowed electrical connection between programming track and rest of layout");
      if(xevnt2 & 0x08)
        setPower(busnumber, -1, "Overload on DCC-Booster or Loconet");
      if(xevnt2 & 0x04)
        setPower(busnumber, -1, "short on internal booster");
      if(xevnt2 & 0x02)
        setPower(busnumber, -1, "Overload on Lokmaus-bus");
      if(xevnt2 & 0x01)
        setPower(busnumber, -1, "short on external booster");
      infoPower(busnumber, msg);
      queueInfoMessage(msg);
    }
  }

  // power off ?
  // we should send an XStatus-command
  if((xevnt1 & 0x08) || (xevnt2 & 0x40))
  {
    byte2send = 0xA2;
    writeByte(busnumber, byte2send, 2);
    readByte(busnumber, 1, &rr);
    if(!(rr & 0x08))
    {
      DBG(busnumber, DBG_DEBUG, "on bus %i no power detected; old-state is %i", busnumber, getPower(busnumber));
      if(getPower(busnumber))
      {
        char msg[500];
        setPower(busnumber, -1, "Emergency Stop");
        infoPower(busnumber, msg);
        queueInfoMessage(msg);
      }
    }
    if(rr & 0x80)
      readByte(busnumber, 1, &rr);
  }


  if(xevnt3 & 0x01)        // we should send an XPT_event-command
    check_status_pt_ib(busnumber);
}

void check_status_pt_ib(int busnumber)
{
  int i;
  //int temp;
  //int status;
  unsigned char byte2send;
  unsigned char rr[7];

  i = -1;
  byte2send = 0xCE;
  while(i == -1)
  {
    writeByte(busnumber, byte2send, 2);
    i = readByte(busnumber, 1, &rr[0]);
    if (i == 0)
    {
      // wait for an answer of our programming
      if (rr[0] == 0xF5)
      {
        // sleep for one second, if answer is not avalible yet
        i = -1;
        sleep(1);
      }
    }
  }
  for (i=0;i<(int)rr[0];i++)
    readByte(busnumber, 1, &rr[i+1]);

  if(last_type[busnumber] != -1)
  {
    setSM(busnumber, last_type[busnumber], -1, last_typeaddr[busnumber], last_bit[busnumber], (int)rr[2], (int)rr[1]);
    last_type[busnumber]     = -1;
  }
}

static int open_comport(int busnumber, speed_t baud)
{
  int fd;
  char *name = busses[busnumber].device;
#ifdef linux
  unsigned char rr;
  int status;
#endif
  struct termios interface;

  DBG(busnumber, DBG_INFO, "try opening serial line %s for %i baud", name, (2400 * (1 << (baud - 11))));
  fd = open(name, O_RDWR);
  DBG(busnumber, DBG_DEBUG, "fd after open(...) = %d", fd);
  busses[busnumber].fd = fd;
  if (fd < 0)
  {
    DBG(busnumber, DBG_ERROR,"dammit, couldn't open device.\n");
  }
  else
  {
#ifdef linux
    tcgetattr(fd, &interface);
    interface.c_oflag = ONOCR;
    interface.c_cflag = CS8 | CRTSCTS | CSTOPB | CLOCAL | CREAD | HUPCL;
    interface.c_iflag = IGNBRK;
    interface.c_lflag = IEXTEN;
    cfsetispeed(&interface, baud);
    cfsetospeed(&interface, baud);
    interface.c_cc[VMIN] = 0;
    interface.c_cc[VTIME] = 1;
    tcsetattr(fd, TCSANOW, &interface);
    status = 0;
    sleep(1);
    while(status != -1)
      status = readByte(busnumber, 1, &rr);
#else
  interface.c_ispeed = interface.c_ospeed = baud;
  interface.c_cflag = CREAD  | HUPCL | CS8 | CSTOPB | CRTSCTS;
  cfmakeraw(&interface);
  tcsetattr(fd, TCSAFLUSH|TCSANOW, &interface);
#endif

  }
  return fd;
}

static int init_line_IB(int busnumber)
{
  int fd;
  int status;
  speed_t baud;
  unsigned char rr;
  unsigned char byte2send;
  struct termios interface;
#ifdef linux
  struct serial_struct serial_line;
  unsigned int LSR;
#endif
  char *name = busses[busnumber].device;
  printf("Begining to detect IB on serial line: %s\n", name);

  DBG(busnumber, DBG_INFO, "Opening serial line %s for 2400 baud\n", name);
  fd=open(name, O_RDWR);
  DBG(busnumber, DBG_INFO, "fd = %d", fd);
  if (fd == -1)
  {
    printf("dammit, couldn't open device.\n");
    return 1;
  }
  busses[busnumber].fd = fd;
  tcgetattr(fd, &interface);
  interface.c_oflag = ONOCR;
  interface.c_cflag = CS8 | CRTSCTS | CSTOPB | CLOCAL | CREAD | HUPCL;
  interface.c_iflag = IGNBRK;
  interface.c_lflag = IEXTEN;
  cfsetispeed(&interface, B2400);
  cfsetospeed(&interface, B2400);
  interface.c_cc[VMIN] = 0;
  interface.c_cc[VTIME] = 1;
  tcsetattr(fd, TCSANOW, &interface);

  status = 0;
  sleep(1);
  printf("Clearing input-buffer\n");
  while(status != -1)
    status = readByte(busnumber, 1, &rr);

#ifdef linux
  ioctl(fd, TIOCGSERIAL, &serial_line);
  close(fd);

  sleep(1);
  LSR = serial_line.port + 3;
  printf("Sending BREAK\n");
  fd = open("/dev/ibox", O_RDWR);
  DBG(busnumber, DBG_INFO, "fd for ibox-dev = %d", fd);
  if(fd < 0)
    return(-1);
  if(ioctl(fd, IB_IOCTINIT, LSR) < 0)
    return(-1);
  usleep(200000);
  if(ioctl(fd, IB_IOCTBREAK, 1) < 0)
    return(-1);
  usleep(1000000);
  if(ioctl(fd, IB_IOCTBREAK, 0) < 0)
    return(-1);
  usleep(600000);
  close(fd);
  sleep(1);
#endif
#ifdef __FreeBSD__
/*
 * Eigentlich will er ja nur ein BREAK senden, das machen wir mal
 * etwas einfacher...
 */
 DBG(busnumber,DBG_INFO,"FBSD BREAK an Ibox senden");
 ioctl(fd,TIOCSBRK,0);
  usleep(1000000);
 ioctl(fd,TIOCCBRK,0);
  usleep(6000000);
  close(fd);
#endif

  printf("BREAK sucessfully sent\n");

  baud = B2400;
  fd = open_comport(busnumber, baud);
  if(fd < 0)
  {
    printf("Init comport faild\n");
    return(-1);
  }
  sleep(1);
  byte2send = 0xC4;
  writeByte(busnumber, byte2send, 2);
  status = readByte(busnumber, 1, &rr);
  if(status == -1)
    return(1);
  if(rr=='D')
  {
    printf("Intellibox in download mode.\nDO NOT PROCEED !!!\n");
    return(2);
  }
  printf("Switching off P50-commands\n");
  byte2send = 'x';
  writeByte(busnumber, byte2send, 0);
  byte2send = 'Z';
  writeByte(busnumber, byte2send, 0);
  byte2send = 'z';
  writeByte(busnumber, byte2send, 0);
  byte2send = 'A';
  writeByte(busnumber, byte2send, 0);
  byte2send = '1';
  writeByte(busnumber, byte2send, 0);
  byte2send = 0x0d;
  writeByte(busnumber, byte2send, 2);
  status = readByte(busnumber, 1, &rr);
  if(status != 0)
    return 1;
  printf("Changing baudrate to 38400 bps\n");
  byte2send = 'B';
  writeByte(busnumber, byte2send, 0);
  byte2send = '3';
  writeByte(busnumber, byte2send, 0);
  byte2send = '8';
  writeByte(busnumber, byte2send, 0);
  byte2send = '4';
  writeByte(busnumber, byte2send, 0);
  byte2send = '0';
  writeByte(busnumber, byte2send, 0);
  writeByte(busnumber, byte2send, 0);
  byte2send = 0x0d;
  writeByte(busnumber, byte2send, 0);

  sleep(1);
  close_comport(fd);

  sleep(1);
  fd = open_comport(busnumber, busses[busnumber].baudrate);
  DBG(busnumber, DBG_DEBUG, "fd after open_comport = %d", fd);
  if(fd < 0)
  {
    printf("Init comport failed\n");
    return(-1);
  }
  byte2send = 0xC4;
  writeByte(busnumber, byte2send, 2);
  status = readByte(busnumber, 1, &rr);
  if(status == -1)
    return(1);
  if(rr=='D')
  {
    printf("Intellibox in download mode.\nDO NOT PROCEED !!!\n");
    return(2);
  }
  
  return 0;
}
