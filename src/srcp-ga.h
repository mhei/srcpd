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
  char protocol;  /* Protocolid */
  int id;               /* Der Identifier */
  int port;             /* Portnummer     */
  int action;           /* 0,1,2,3...     */
  long activetime;      /* Aktivierungszeit in msec bis das automatische AUS kommen soll */
  struct timeval inittime;
  struct timeval tv[MAXGAPORT]; /* Zeitpunkt der letzten Aktivierungen, ein Wert pro Port   */
  struct timeval t;     /* Auschaltzeitpunkt */
  struct timeval locktime;
  long int locked_by;     /* wer hält den Lock? */
  long int lockduration;
};

typedef struct _GA
{
  int numberOfGa;
  struct _GASTATE *gastate;
} GA;

int startup_GA(void);
int init_GA(int busnumber, int number);
int get_number_ga(int busnumber);

int queueGA(int busnumber, int addr, int port, int aktion, long activetime);
int unqueueNextGA(int busnumber, struct _GASTATE *);
int queue_GA_isempty(int busnumber);

int getGA(int busnumber, int addr, struct _GASTATE *a);
int setGA(int busnumber, int addr, struct _GASTATE a);
int initGA(int busnumber, int addr, const char protocol);
int describeGA(int busnumber, int addr, char *msg);
int infoGA(int busnumber, int addr, int port, char* msg);
int cmpGA(struct _GASTATE a, struct _GASTATE b);
int isInitializedGA(int busnumber, int addr);

int lockGA(int busnumber, int addr, long int duration, long int sessionid);
int getlockGA(int busnumber, int addr, long int *sessionid);
int unlockGA(int busnumber, int addr, long int sessionid);
void unlock_ga_bysessionid(long int);
void unlock_ga_bytime(void);
int describeLOCKGA(int bus, int addr, char *reply);
#endif
