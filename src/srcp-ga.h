/* $Id$ */

/* 
 * Vorliegende Software unterliegt der General Public License, 
 * Version 2, 1991. (c) Matthias Trute, 2000-2001.
 *
 */


#ifndef _SRCP_GA_H
#define _SRCP_GA_H

#include <sys/time.h>

#define MAXGAS 2049
#define MAXGAPORT 2

/* Schaltdekoder */
struct _GA
{
  char *protocol;  /* Protocolid */
  int id;               /* Der Identifier */
  int port;             /* Portnummer     */
  int action;           /* 0,1,2,3...     */
  long activetime;      /* Aktivierungszeit in msec bis das 32 Kommando kommen soll */
  struct timeval tv[MAXGAPORT]; /* Zeitpunkt der letzten Aktivierungen, ein Wert pro Port   */
  struct timeval t;     /* Auschaltzeitpunkt */
  long int lockid;     /* wer hält den Lock? */

};

int startup_GA(void);

int queueGA(int bus, int addr, int port, int aktion, long activetime);
int unqueueNextGA(int bus, struct _GA *);
int queue_GA_isempty(int bus);

int getGA(int bus, int addr, struct _GA *a);
int setGA(int bus, int addr, struct _GA a);
void initGA(int bus, int addr, struct _GA a);
int infoGA(int bus, int addr, char* msg);
int cmpGA(struct _GA a, struct _GA b);

#endif
