/* $Id$ */

/* 
 * Vorliegende Software unterliegt der General Public License, 
 * Version 2, 1991. (c) Matthias Trute, 2000-2001.
 *
 */


#ifndef _SRCP_TIME_H
#define _SRCP_TIME_H

#include <sys/time.h>

/* Zeitgeber */
struct _VTIME
{
  int day;
  int hour;
  int min;
  int sec;
  int ratio_x; /* ratio_x == 0 und die Uhr steht */
  int ratio_y;
};

int startup_TIME(void);

int setTime(int d, int h, int m, int s, int rx, int ry);
int getTime(struct _VTIME *vt);
int infoTime(struct _VTIME, char *msg);
int cmpTime(struct timeval *t1, struct timeval *t2);

int describeTIME(int bus, int addr, char *reply);
void* thr_clock(void *);

#endif
