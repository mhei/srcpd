/* $Id$ */

/* 
 * Vorliegende Software unterliegt der General Public License, 
 * Version 2, 1991. (c) Matthias Trute, 2000-2001.
 *
 */


#ifndef _SRCP_GA_H
#define _SRCP_GA_H

#include <sys/time.h>

#define MAXGAPORT 2

/* Schaltdekoder */
struct _GASTATE
{
  char *protocol;  /* Protocolid */
  int id;               /* Der Identifier */
  int port;             /* Portnummer     */
  int action;           /* 0,1,2,3...     */
  long activetime;      /* Aktivierungszeit in msec bis das automatische AUS kommen soll */
  struct timeval tv[MAXGAPORT]; /* Zeitpunkt der letzten Aktivierungen, ein Wert pro Port   */
  struct timeval t;     /* Auschaltzeitpunkt */
  long int locked_by;     /* wer hält den Lock? */
};

typedef struct _GA
{
  int numberOfGa;
  struct _GASTATE *gastate;
} GA;

int startup_GA(void);
int init_GA(int bus, int number);
int get_number_ga(int bus);

int queueGA(int bus, int addr, int port, int aktion, long activetime);
int unqueueNextGA(int bus, struct _GASTATE *);
int queue_GA_isempty(int bus);

int getGA(int bus, int addr, struct _GASTATE *a);
int setGA(int bus, int addr, struct _GASTATE a);
int initGA(int bus, int addr, const char *protocol);
int describeGA(int bus, int addr, char *msg);
int infoGA(int bus, int addr, int port, char* msg);
int cmpGA(struct _GASTATE a, struct _GASTATE b);
int isInitializedGA(int bus, int addr);

int lockGA(int bus, int addr, long int sessionid);
int getlockGA(int bus, int addr, long int sessionid);
int unlockGA(int bus, int addr, int sessionid);
void unlock_ga_bysessionid(long int);
#endif
