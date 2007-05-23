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
struct _GASTATE {
    int state;                  /* 0==dead, 1==living, 2==terminating */
    char protocol;              /* Protocol Id */
    int id;                     /* Der Identifier */
    int port;                   /* Portnumber     */
    int action;                 /* 0, 1, 2, 3...  */
    long activetime;            /* Aktivierungszeit in msec bis das 
                                   automatische AUS kommen soll */
    struct timeval inittime;
    struct timeval tv[MAXGAPORT];   /* Zeitpunkt der letzten Aktivierungen,
                                       ein Wert pro Port */
    struct timeval t;           /* Auschaltzeitpunkt */
    struct timeval locktime;
    sessionid_t locked_by;      /* who has the LOCK? */
    long int lockduration;
};

typedef struct _GA {
    int numberOfGa;
    struct _GASTATE *gastate;
} GA;

int startup_GA(void);
int init_GA(bus_t busnumber, int number);
int get_number_ga(bus_t busnumber);

int queueGA(bus_t busnumber, int addr, int port, int action,
            long int activetime);
int unqueueNextGA(bus_t busnumber, struct _GASTATE *);
int queue_GA_isempty(bus_t busnumber);

int getGA(bus_t busnumber, int addr, struct _GASTATE *a);
int setGA(bus_t busnumber, int addr, struct _GASTATE a);
int initGA(bus_t busnumber, int addr, const char protocol);
int describeGA(bus_t busnumber, int addr, char *msg);
int infoGA(bus_t busnumber, int addr, int port, char *msg);
int cmpGA(struct _GASTATE a, struct _GASTATE b);
int isInitializedGA(bus_t busnumber, int addr);

int lockGA(bus_t busnumber, int addr, long int duration,
           sessionid_t sessionid);
int getlockGA(bus_t busnumber, int addr, sessionid_t * sessionid);
int unlockGA(bus_t busnumber, int addr, sessionid_t sessionid);
void unlock_ga_bysessionid(sessionid_t);
void unlock_ga_bytime(void);
int describeLOCKGA(bus_t bus, int addr, char *reply);
#endif
