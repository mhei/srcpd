/***************************************************************************
                          ddl-s88.c  -  description
                             -------------------
    begin                : Wed Aug 1 2001
    copyright            : (C) 2001 by Dipl.-Ing. Frank Schmischke
    email                : frank.schmischke@t-online.de

    This source based on errdcd-sourcecode by Torsten Vogt.
    full header statement below!
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "stdincludes.h"

#ifdef linux
#include <linux/lp.h>
#include <sys/io.h>
#endif
#ifdef __FreeBSD__
#include <dev/ppbus/ppi.h>
#include <dev/ppbus/ppbconf.h>
#else
#error Dieser Treiber ist fuer Ihr Betriebssystem nicht geeignet
#endif

#include "config-srcpd.h"
#include "srcp-fb.h"
#include "ddl-s88.h"
#include "io.h"


/***************************************************************/
/* erddcd - Electric Railroad Direct Digital Command Daemon    */
/*    generates without any other hardware digital commands    */
/*    to control electric model railroads                      */
/*                                                             */
/* file: maerklin_s88.c                                        */
/* job : some routines to read s88 data from the printer-port  */
/*                                                             */
/* Torsten Vogt, Dieter Schaefer, october 1999                 */
/* Martin Wolf, november 2000                                  */
/*                                                             */
/* last changes: Torsten Vogt, march 2000                      */
/*               Martin Wolf, november 2000                    */
/* modified for srcpd: Matthias Trute, may 2002 */
/* modified for FreeBSD: Michael Meiszl, jan 2003              */
/*                                                             */
/***************************************************************/

// signals on the S88-Bus
#define S88_QUIET 0x00    // all signals low
#define S88_RESET 0x04    // reset signal high
#define S88_LOAD  0x02    // load signal high
#define S88_CLOCK 0x01    // clock signal high
#define S88_DATA1 0x40    // mask for data form S88 bus 1 (ACK)
#define S88_DATA2 0x80    // mask for data from S88 bus 2 (BUSY) !inverted
#define S88_DATA3 0x20    // mask for data from S88 bus 3 (PEND)
#define S88_DATA4 0x10    // mask for data from S88 bus 4 (SEL)

// Output a Signal to the Bus
#define S88_WRITE(x) for (i=0;i<S88CLOCK_SCALE;i++) outb(x,S88PORT)

// possible io-addresses for the parallel port
const unsigned long LPT_BASE[] = { 0x378, 0x278, 0x3BC };
// number of possible parallel ports
const unsigned int LPT_NUM = 3;
// values of the bits in a byte
const char BIT_VALUES[] = { 0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01 };

#define __ddl_s88 ((DDL_S88_DATA *) busses[busnumber].driverdata)

void readconfig_DDL_S88(xmlDocPtr doc, xmlNodePtr node, int busnumber)
{
  int i;
  xmlNodePtr child = node->children;
  busses[busnumber].type = SERVER_S88;
  busses[busnumber].init_func = &init_bus_S88;
  busses[busnumber].term_func = &term_bus_S88;
  busses[busnumber].thr_func = &thr_sendrec_S88;
  busses[busnumber].driverdata = malloc(sizeof(struct _DDL_S88_DATA));

  __ddl_s88->port = 0x0378;
#ifdef __FreeBSD__
  // da wir ueber eine viel langsamere Schnittstelle gehen, ist es
  // unnoetig, soviel Zeit zu verblasen. Wahrscheinlich reicht sogar
  // ein einziger Schreibversuch. MAM
  __ddl_s88->clockscale = 2;
#else
  __ddl_s88->clockscale = 35;
#endif

  __ddl_s88->refresh = 100;

#ifdef __FreeBSD__
  __ddl_s88->Fd = -1; // signalisiere gechlossenes Port
#endif

  strcpy(busses[busnumber].description, "FB POWER");
  __ddl_s88->number_fb[0] = 1;
  __ddl_s88->number_fb[1] = 1;
  __ddl_s88->number_fb[2] = 1;
  __ddl_s88->number_fb[3] = 1;
  for (i = 1; i < 4; i++)
  {
    busses[busnumber + i].type = SERVER_S88;
    busses[busnumber + i].init_func = NULL;
    busses[busnumber + i].term_func = NULL;
    busses[busnumber + i].thr_func = &thr_sendrec_dummy;
    busses[busnumber + i].driverdata = NULL;
  }

  while (child)
  {
    if (strncmp(child->name, "text", 4) == 0)
    {
      child = child->next;
      continue;
    }
    if (strcmp(child->name, "ioport") == 0)
    {
      char *txt = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
      __ddl_s88->port = strtol(txt, (char **) NULL, 0);  // better than atoi(3)
      free(txt);
    }
    if (strcmp(child->name, "clockscale") == 0)
    {
      char *txt = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
      __ddl_s88->clockscale = atoi(txt);
      free(txt);
    }
    if (strcmp(child->name, "refresh") == 0)
    {
      char *txt = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
      __ddl_s88->refresh = atoi(txt);
      free(txt);
    }
    if (strcmp(child->name, "number_fb_1") == 0)
    {
      char *txt = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
      __ddl_s88->number_fb[0] = atoi(txt);
      free(txt);
    }
    if (strcmp(child->name, "number_fb_2") == 0)
    {
      char *txt = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
      __ddl_s88->number_fb[1] = atoi(txt);
      free(txt);
    }
    if (strcmp(child->name, "number_fb_3") == 0)
    {
      char *txt = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
      __ddl_s88->number_fb[2] = atoi(txt);
      free(txt);
    }
    if (strcmp(child->name, "number_fb_4") == 0)
    {
      char *txt = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
      __ddl_s88->number_fb[3] = atoi(txt);
      free(txt);
    }
    child = child->next;
  }
  if(init_FB(busnumber, __ddl_s88->number_fb[0]*16))
  {
    __ddl_s88->number_fb[0] = 0;
    DBG(busnumber, DBG_ERROR,  "Can't create array for feedback");
  }
  if(init_FB(busnumber, __ddl_s88->number_fb[1]*16))
  {
    __ddl_s88->number_fb[1] = 0;
    DBG(busnumber, DBG_ERROR, "Can't create array for feedback");
  }
  if(init_FB(busnumber, __ddl_s88->number_fb[2]*16))
  {
    __ddl_s88->number_fb[2] = 0;
    DBG(busnumber, DBG_ERROR, "Can't create array for feedback");
  }
  if(init_FB(busnumber, __ddl_s88->number_fb[3]*16))
  {
    __ddl_s88->number_fb[3] = 0;
    DBG(busnumber, DBG_ERROR, "Can't create array for feedback");
  }
  busnumber += 4;
}

/****************************************************************
* function s88init                                              *
*                                                               *
* purpose: test the parralel port for the s88bus and initializes*
*          the bus. The portadress must be one of LPT_BASE, the *
*          port must be accessable through ioperm and there must*
*          be an real device at the adress.                     *
*                                                               *
* in:      ---                                                  *
* out:     return value: 1 if testing and initializing was      *
*                        successfull, otherwise 0               *
*                                                               *
* remarks: tested MW, 20.11.2000                                *
* bit ordering is changed from erddcd code! (MT)     *
*                                                               *
****************************************************************/
int init_bus_S88(int busnumber)
{
  unsigned int i;    // loop counter
  int isin = 0;    // reminder for checking
  int S88PORT = __ddl_s88->port;
  int S88CLOCK_SCALE = __ddl_s88->clockscale;
#ifdef linux
  DBG(busnumber, DBG_INFO, "init_bus DDL(Linux) S88%d", busnumber);
#else
#ifdef __FreeBSD__
  DBG(busnumber, DBG_INFO, "init_bus DDL(FreeBSD) S88%d", busnumber);
#endif
#endif
  // is the port disabled from user, everything is fine
  if (!S88PORT)
  {
        DBG(busnumber, DBG_INFO, "   s88 port is disabled.");
    return 1;
  }
  // test, whether S88DEV is a valid io-address for a parallel device
  for (i = 0; i < LPT_NUM; i++)
	    isin = isin || (S88PORT == LPT_BASE[i]);
  if (isin)
  {
    // test, whether we can access the port
    if (ioperm(S88PORT, 3, 1) == 0)
    {
      // test, whether there is a real device on the S88DEV-port by writing and 
      // reading data to the port. If the written data is returned, a real port
      // is there
      outb(0x00, S88PORT);
      isin = (inb(S88PORT) == 0);
      outb(0xFF, S88PORT);
      isin = (inb(S88PORT) == 0xFF) && isin;
      if (isin)
      {
        // initialize the S88 by doing a reset
        // for ELEKTOR-Modul the reset must be on the load line
        S88_WRITE(S88_QUIET);
        S88_WRITE(S88_RESET & S88_LOAD);
        S88_WRITE(S88_QUIET);
      }
      else
      {
            DBG(busnumber, DBG_WARN, "   warning: There is no port for s88 at 0x%X.",
           S88PORT);
        ioperm(S88PORT, 3, 0);  // stopping access to port address
        return 1;
      }
    }
    else
    {
          DBG(busnumber, DBG_ERROR, "   warning: Access to port 0x%X denied.",
       S88PORT);
      return 1;
    }
  }
  else
  {
        DBG(busnumber, DBG_WARN, "   warning: 0x%X is not valid port adress for s88 device.",
      S88PORT);
    return 1;
  }
      DBG(busnumber, DBG_INFO, "   s88 port sucsessfully initialized at 0x%X.",
     S88PORT);
  return 0;
}

/****************************************************************
* function s88load                                              *
*                                                               *
* purpose: Loads the data from the bus in s88data if the valid- *
*          timespace S88REFRESH is over. Then also the new      *
*          validity-timeout is set to s88valid.                 *
*          If port is disabled or data is valid does nothing.      *
*                                                               *
* in:      ---                                                  *
* out:     ---                                                  *
*                                                               *
* remarks: tested MW, 20.11.2000                                *
*                                                               *
****************************************************************/
void s88load(int busnumber)
{
  int i, j, k, inbyte;
  struct timeval nowtime;
  unsigned int s88data[S88_MAXPORTSB * S88_MAXBUSSES];  //valid bus-data
  int S88PORT = __ddl_s88->port;
  int S88CLOCK_SCALE = __ddl_s88->clockscale;
  int S88REFRESH = __ddl_s88->refresh;

  gettimeofday(&nowtime, NULL);
  if ((nowtime.tv_sec > __ddl_s88->s88valid.tv_sec) ||
  ((nowtime.tv_sec == __ddl_s88->s88valid.tv_sec) &&
   (nowtime.tv_usec > __ddl_s88->s88valid.tv_usec)))
  {
    // data is out of date - get new data from the bus
    // initialize the s88data array
    for (i = 0; i < S88_MAXPORTSB * S88_MAXBUSSES; i++)
      s88data[i] = 0;
    if (S88PORT)
    {
      // if port is disabled do nothing
      // load the bus
      ioperm(S88PORT, 3, 1);
      S88_WRITE(S88_LOAD);
      S88_WRITE(S88_LOAD | S88_CLOCK);
      S88_WRITE(S88_QUIET);
      S88_WRITE(S88_RESET);
      S88_WRITE(S88_QUIET);
      // reading the data 
      for (j = 0; j < S88_MAXPORTSB; j++)
      {
        for (k = 0; k < 8; k++)
        {
          // reading from port
          inbyte = inb(S88PORT + 1);
          // interpreting the four busses
          if (inbyte & S88_DATA1)
            s88data[j] += BIT_VALUES[k];
          if (!(inbyte & S88_DATA2))
            s88data[j + S88_MAXPORTSB] += BIT_VALUES[k];
          if (inbyte & S88_DATA3)
            s88data[j + 2 * S88_MAXPORTSB] += BIT_VALUES[k];
          if (inbyte & S88_DATA4)
            s88data[j + 3 * S88_MAXPORTSB] += BIT_VALUES[k];
          // getting the next data
          S88_WRITE(S88_CLOCK);
          S88_WRITE(S88_QUIET);
        }
        if (j < __ddl_s88-> number_fb[0] * 2)
          setFBmodul(busnumber, j + 1, s88data[j]);
        if (j < __ddl_s88-> number_fb[1] * 2)
          setFBmodul(busnumber + 1, j + 1, s88data[j + S88_MAXPORTSB]);
        if (j < __ddl_s88-> number_fb[2] * 2)
          setFBmodul(busnumber + 2, j + 1, s88data[j + 2 * S88_MAXPORTSB]);
        if (j < __ddl_s88-> number_fb[3] * 2)
          setFBmodul(busnumber + 3, j + 1, s88data[j + 3 * S88_MAXPORTSB]);
      }
      nowtime.tv_usec += S88REFRESH * 1000;
      __ddl_s88->s88valid.tv_usec = nowtime.tv_usec % 1000000;
      __ddl_s88->s88valid.tv_sec  = nowtime.tv_sec + nowtime.tv_usec / 1000000;
    }
  }
}

int term_bus_S88(int bus)
{
    return 0;
}

void *thr_sendrec_S88(void *v)
{
  int busnumber = (int) v;
  unsigned long sleepusec = 100000;

  int S88REFRESH = __ddl_s88->refresh;

  // set refresh-cycle
  if (sleepusec < S88REFRESH * 1000)
    sleepusec = S88REFRESH * 1000;
  while (1)
  {
    usleep(sleepusec);
    s88load(busnumber);
  }
}

void *thr_sendrec_dummy(void *v)
{
  while(1)
    sleep(1);
}
/*---------------------------------------------------------------------------
 * End of Linux Code
 *---------------------------------------------------------------------------*/
#ifdef __FreeBSD__
/*---------------------------------------------------------------------------
 * Start of FreeBSD Emulation of ioperm, inb and outb
 * MAM 01/06/03
 *---------------------------------------------------------------------------*/

int FBSD_ioperm(int Port,int KeineAhnung, int DesiredAccess,int busnumber)
{
	int i;
	int found=0;
	char DevName[256];
	int Fd;
	

	// Simpel: soll geschlossen werden?
	if (DesiredAccess == 0)
		{
		if (__ddl_s88->Fd != -1) 
		{
		   close(__ddl_s88->Fd);
    	   	   DBG(busnumber, DBG_DEBUG,  "FBSD DDL-S88 closing port %04X",Port);
		}
		else
		{
		   DBG(busnumber, DBG_WARN,  "FBSD DDL-S88 closing NOT OPEN port %04X",Port);
			}
		__ddl_s88->Fd=-1;
		return 0;
		}

	// ist schon offen???
	if (__ddl_s88->Fd != -1)
	{
		// DBG(busnumber, DBG_INFO,  "FBSD DDL-S88 trying to re-open port %04X (ignored)",Port);

		return 0; // gnaedig ignorieren
	}

	// Also oeffnen, das ist schon trickreicher :-)
	/* Erst einmal rausbekommen, welches Device denn gemeint war
	 * Dazu muessen wir einfach die Portposition aus dem Array
	 * ermitteln
	 */

	__ddl_s88->Fd=-1;	// traue keinem :-)

	for (i=0;i<LPT_NUM;i++)
	{
	    if (Port == LPT_BASE[i]) { found=1;break; }
	}
	if (found == 0) return -1;

	sprintf(DevName,"/dev/ppi%d",i);

	Fd = open(DevName,O_RDWR);

	if (Fd < 0) 
	{

    	   DBG(busnumber, DBG_ERROR,  "FBSD DDL-S88 cannot open port %04X on %s",Port,DevName);
	   return Fd;
	}
       DBG(busnumber, DBG_INFO,  "FBSD DDL-S88 success opening port %04X on %s",Port,DevName);

	__ddl_s88->Fd = Fd;
	return 0;
}


unsigned char FBSD_inb(int Woher,int busnumber)
{
	// Aufpassen! Manchmal wird das Datenport, manchmal die Steuer
	// leitungen angesprochen !

	unsigned char i=0;
	int	WelchesPort;
	int	WelcherIoctl;

  //DBG(busnumber, DBG_DEBUG,  "FBSD DDL-S88 InB start on port %04X",Woher);
	if (__ddl_s88->Fd == -1)
	{
    	   DBG(busnumber, DBG_ERROR,  "FBSD DDL-S88 Device not open for reading");
		exit (1) ;
	}

	WelchesPort=Woher - __ddl_s88->port;

	switch (WelchesPort) 
	{
		case 0:	WelcherIoctl=PPIGDATA; break;
		case 1:	WelcherIoctl=PPIGSTATUS; break;
		case 2: WelcherIoctl=PPIGCTRL; break;
		default:
			DBG(busnumber,DBG_FATAL,"FBSD DDL-S88 Lesezugriff auf Port %04X angefordert, nicht umsetzbar!",Woher);
			return 0;
	}
	ioctl(__ddl_s88->Fd,WelcherIoctl,&i);
  //DBG(busnumber, DBG_DEBUG,  "FBSD DDL-S88 InB finished Data %02X",i);
	return i;
}

unsigned char FBSD_outb(unsigned char Data, int Wohin,int busnumber)
{
	// suchen wir uns den richtigen ioctl zur richtigen Adresse...

	int	WelchesPort;
	int	WelcherIoctl;

  //DBG(busnumber, DBG_DEBUG,  "FBSD DDL-S88 OutB %d on Port %04X",Data,Wohin);
		if (__ddl_s88->Fd == -1)
	{
    	   DBG(busnumber, DBG_ERROR,  "FBSD DDL-S88 Device not open for writing Byte %d",Data);
		exit (1) ;
		return -1;
	}
	
	WelchesPort= Wohin - __ddl_s88->port;

	switch (WelchesPort) 
	{
		case 0:	WelcherIoctl=PPISDATA; break;
		case 1:	WelcherIoctl=PPISSTATUS; break;
		case 2: WelcherIoctl=PPISCTRL; break;
		default:
			DBG(busnumber,DBG_FATAL,"FBSD DDL-S88 Schreibzugriff auf Port %04X angefordert, nicht umsetzbar!",Wohin);
			return 0;
	}
	ioctl(__ddl_s88->Fd,WelcherIoctl,&Data);
  //DBG(busnumber, DBG_DEBUG,  "FBSD DDL-S88 OutB finished");
	return Data;
}

#endif

