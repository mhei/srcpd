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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <fcntl.h>
#include <linux/lp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <sys/ioctl.h>
#include <sys/io.h>
#include <unistd.h>

#include "config-srcpd.h"
#include "srcp-fb.h"
#include "ddl-s88.h"

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
/*                                                             */
/***************************************************************/

// signals on the S88-Bus
#define S88_QUIET 0x00		// all signals low
#define S88_RESET 0x04		// reset signal high
#define S88_LOAD  0x02		// load signal high
#define S88_CLOCK 0x01		// clock signal high
#define S88_DATA1 0x40		// mask for data form S88 bus 1 (ACK)
#define S88_DATA2 0x80		// mask for data from S88 bus 2 (BUSY) !inverted
#define S88_DATA3 0x20		// mask for data from S88 bus 3 (PEND)
#define S88_DATA4 0x10		// mask for data from S88 bus 4 (SEL)

// Output a Signal to the Bus
#define S88_WRITE(x) for (i=0;i<S88CLOCK_SCALE;i++) outb(x,S88PORT)

// possible io-addresses for the parallel port
const unsigned long LPT_BASE[] = { 0x3BC, 0x378, 0x278 };
// number of possible parallel ports
const unsigned int LPT_NUM = 3;
// values of the bits in a byte
// const char BIT_VALUES[] = { 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80 };
const char BIT_VALUES[] =
    { 0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01 };
void readconfig_DDL_S88(xmlDocPtr doc, xmlNodePtr node, int busnumber)
{
    int i;
    xmlNodePtr child = node->children;
    busses[busnumber].type = SERVER_S88;
    busses[busnumber].init_func = &init_bus_S88;
    busses[busnumber].term_func = &term_bus_S88;
    busses[busnumber].thr_func = &thr_sendrec_S88;
    busses[busnumber].driverdata = malloc(sizeof(struct _DDL_S88_DATA));
    ((DDL_S88_DATA *) busses[busnumber].driverdata)->port = 0x0378;
    ((DDL_S88_DATA *) busses[busnumber].driverdata)->clockscale = 35;
    ((DDL_S88_DATA *) busses[busnumber].driverdata)->refresh = 100;
    strcpy(busses[busnumber].description, "FB POWER");
    ((DDL_S88_DATA *) busses[busnumber].driverdata)->number_fb[0] = 1;
    ((DDL_S88_DATA *) busses[busnumber].driverdata)->number_fb[1] = 1;
    ((DDL_S88_DATA *) busses[busnumber].driverdata)->number_fb[2] = 1;
    ((DDL_S88_DATA *) busses[busnumber].driverdata)->number_fb[3] = 1;
    for (i = 1; i < 4; i++) {
	busses[busnumber + i].type = SERVER_S88;
	busses[busnumber + i].init_func = NULL;
	busses[busnumber + i].term_func = NULL;
	busses[busnumber + i].thr_func = &thr_sendrec_dummy;
	busses[busnumber + i].driverdata = NULL;
    }
    while (child) {
	if (strncmp(child->name, "text", 4) == 0) {
	    child = child->next;
	    continue;
	}
	if (strcmp(child->name, "ioport") == 0) {
	    char *txt =
		xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
	    ((DDL_S88_DATA *) busses[busnumber].driverdata)->port = strtol(txt, (char **) NULL, 0);	// better than atoi(3)
	    free(txt);
	}


	if (strcmp(child->name, "clockscale") == 0) {
	    char *txt = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
	    ((DDL_S88_DATA *) busses[busnumber].driverdata)->clockscale = atoi(txt);
	    free(txt);
	}

	if (strcmp(child->name, "refresh") == 0) {
	    char *txt = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
	    ((DDL_S88_DATA *) busses[busnumber].driverdata)->refresh = atoi(txt);
	    free(txt);
	}
	if (strcmp(child->name, "number_fb_1") == 0) {
	    char *txt = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
	    ((DDL_S88_DATA *) busses[busnumber].driverdata)->number_fb[0] = atoi(txt);
	    free(txt);
	}
	if (strcmp(child->name, "number_fb_2") == 0) {
	    char *txt = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
	    ((DDL_S88_DATA *) busses[busnumber].driverdata)->number_fb[1] = atoi(txt);
	    free(txt);
	}
	if (strcmp(child->name, "number_fb_3") == 0) {
	    char *txt = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
	    ((DDL_S88_DATA *) busses[busnumber].driverdata)->number_fb[2] = atoi(txt);
	    free(txt);
	}
	if (strcmp(child->name, "number_fb_4") == 0) {
	    char *txt = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
	    ((DDL_S88_DATA *) busses[busnumber].driverdata)->number_fb[3] = atoi(txt);
	    free(txt);
	}
	child = child->next;
    }
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
*                                                               *
****************************************************************/
int init_bus_S88(int bus)
{
    unsigned int i;		// loop counter
    int isin = 0;		// reminder for checking
    int S88PORT = ((DDL_S88_DATA *) busses[bus].driverdata)->port;
    int S88CLOCK_SCALE =
	((DDL_S88_DATA *) busses[bus].driverdata)->clockscale;
    // is the port disabled from user, everything is fine
    if (!S88PORT) {
	syslog(LOG_INFO, "   s88 port is disabled.");
	return 1;
    }
    // test, whether S88DEV is a valid io-address for a parallel device 
    for (i = 0; i < LPT_NUM; i++)
	isin = isin || (S88PORT == LPT_BASE[i]);
    if (isin) {
	// test, whether we can access the port
	if (ioperm(S88PORT, 3, 1) == 0) {
	    // test, whether there is a real device on the S88DEV-port by writing and 
	    // reading data to the port. If the written data is returned, a real port
	    // is there
	    outb(0x00, S88PORT);
	    isin = (inb(S88PORT) == 0);
	    outb(0xFF, S88PORT);
	    isin = (inb(S88PORT) == 0xFF) && isin;
	    if (isin) {
		// initialize the S88 by doing a reset
		// for ELEKTOR-Modul the reset must be on the load line
		S88_WRITE(S88_QUIET);
		S88_WRITE(S88_RESET & S88_LOAD);
		S88_WRITE(S88_QUIET);
	    } else {
		syslog(LOG_INFO,
		       "   warning: There is no port for s88 at 0x%X.",
		       S88PORT);
		ioperm(S88PORT, 3, 0);	// stopping access to port address
		return 0;
	    }
	} else {
	    syslog(LOG_INFO, "   warning: Access to port 0x%X denied.",
		   S88PORT);
	    return 0;
	}
    } else {
	syslog(LOG_INFO,
	       "   warning: 0x%X is not valid port adress for s88 device.",
	       S88PORT);
	return 0;
    }
    syslog(LOG_INFO, "   s88 port sucsessfully initialized at 0x%X.",
	   S88PORT);
    return 1;
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
void s88load(int bus)
{
    int i, j, k, inbyte;
    struct timeval nowtime;
    unsigned int s88data[S88_MAXPORTSB * S88_MAXBUSSES];	//valid bus-data
    int S88PORT = ((DDL_S88_DATA *) busses[bus].driverdata)->port;
    int S88CLOCK_SCALE =
	((DDL_S88_DATA *) busses[bus].driverdata)->clockscale;
    int S88REFRESH = ((DDL_S88_DATA *) busses[bus].driverdata)->refresh;

    gettimeofday(&nowtime, NULL);
    if ((nowtime.tv_sec >
	 ((DDL_S88_DATA *) busses[bus].driverdata)->s88valid.tv_sec)
	||
	((nowtime.tv_sec ==
	  ((DDL_S88_DATA *) busses[bus].driverdata)->s88valid.tv_sec)
	 && (nowtime.tv_usec >
	     ((DDL_S88_DATA *) busses[bus].driverdata)->s88valid.tv_usec))
	) {			// data is out of date - get new data from the bus
	// initialize the s88data array
	for (i = 0; i < S88_MAXPORTSB * S88_MAXBUSSES; i++)
	    s88data[i] = 0;
	if (S88PORT) {		// if port is disabled do nothing
	    // load the bus
	    ioperm(S88PORT, 3, 1);
	    S88_WRITE(S88_LOAD);
	    S88_WRITE(S88_LOAD | S88_CLOCK);
	    S88_WRITE(S88_QUIET);
	    S88_WRITE(S88_RESET);
	    S88_WRITE(S88_QUIET);
	    // reading the data 
	    for (j = 0; j < S88_MAXPORTSB; j++) {
		for (k = 0; k < 8; k++) {
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
		if (j <
		    ((DDL_S88_DATA *) busses[bus].driverdata)->
		    number_fb[0] * 2)
		    setFBmodul8(bus, j + 1, s88data[j]);
		if (j <
		    ((DDL_S88_DATA *) busses[bus].driverdata)->
		    number_fb[1] * 2)
		    setFBmodul8(bus + 1, j + 1,
				s88data[j + S88_MAXPORTSB]);
		if (j <
		    ((DDL_S88_DATA *) busses[bus].driverdata)->
		    number_fb[2] * 2)
		    setFBmodul8(bus + 2, j + 1,
				s88data[j + 2 * S88_MAXPORTSB]);
		if (j <
		    ((DDL_S88_DATA *) busses[bus].driverdata)->
		    number_fb[3] * 2)
		    setFBmodul8(bus + 3, j + 1,
				s88data[j + 3 * S88_MAXPORTSB]);
	    }
	    nowtime.tv_usec += S88REFRESH * 1000;
	    ((DDL_S88_DATA *) busses[bus].driverdata)->s88valid.tv_usec =
		nowtime.tv_usec % 1000000;
	    ((DDL_S88_DATA *) busses[bus].driverdata)->s88valid.tv_sec =
		nowtime.tv_sec + nowtime.tv_usec / 1000000;
	}
    }
}


int term_bus_S88(int bus)
{
    return 0;
}


void *thr_sendrec_S88(void *v)
{
    int bus = (int) v;
    unsigned long sleepusec = 100000;

    int S88REFRESH = ((DDL_S88_DATA *) busses[bus].driverdata)->refresh;

    // set refresh-cycle
    if (sleepusec < S88REFRESH * 1000)
	sleepusec = S88REFRESH * 1000;
    while (1) {
	usleep(sleepusec);
	s88load(bus);
    }
}

void *thr_sendrec_dummy(void *v)
{
    while (1)
	sleep(1);
}
