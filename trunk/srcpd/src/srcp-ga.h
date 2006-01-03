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
  int state ; /* 0==dead, 1==living, 2==terminating */
  char protocol;  /* Protocolid */
  int id;               /* Der Identifier */
  int port;             /* Portnummer     */
  int action;           /* 0,1,2,3...     */
  long activetime;      /* Aktivierungszeit in msec bis das automatische AUS kommen soll */
  struct timeval inittime;
  struct timeval tv[MAXGAPORT]; /* Zeitpunkt der letzten Aktivierungen, ein Wert pro Port   */
  struct timeval t;     /* Auschaltzeitpunkt */
  struct timeval locktime;
  long int locked_by;     /* wer h�t den Lock? */
  long int lockduration;
};

typedef struct _GA
{
  int numberOfGa;
  struct _GASTATE *gastate;
} GA;

int startup_GA(void);
int init_GA(long int busnumber, int number);
int get_number_ga(long int busnumber);

int queueGA(long int busnumber, int addr, int port, int action, long int activetime);
int unqueueNextGA(long int busnumber, struct _GASTATE *);
int queue_GA_isempty(long int busnumber);

int getGA(long int busnumber, int addr, struct _GASTATE *a);
int setGA(long int busnumber, int addr, struct _GASTATE a);
int initGA(long int busnumber, int addr, const char protocol);
int describeGA(long int busnumber, int addr, char *msg);
int infoGA(long int busnumber, int addr, int port, char* msg);
int cmpGA(struct _GASTATE a, struct _GASTATE b);
int isInitializedGA(long int busnumber, int addr);

int lockGA(long int busnumber, int addr, long int duration, long int sessionid);
int getlockGA(long int busnumber, int addr, long int *sessionid);
int unlockGA(long int busnumber, int addr, long int sessionid);
void unlock_ga_bysessionid(long int);
void unlock_ga_bytime(void);
int describeLOCKGA(long int bus, int addr, char *reply);
#endif
