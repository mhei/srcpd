/* $Id$ */

/* 
 * Vorliegende Software unterliegt der General Public License, 
 * Version 2, 1991. (c) Matthias Trute, 2000-2001.
 *
 */


#ifndef _SRCP_TIME_H
#define _SRCP_TIME_H

#include <sys/time.h>

/* timer */
typedef struct _VTIME
{
  int day;
  int hour;
  int min;
  int sec;
  int ratio_x; /* ratio_x == 0 und die Uhr steht */
  int ratio_y;
  struct timeval inittime;
} vtime_t;

/*global variable*/
vtime_t vtime;

int startup_TIME(void);
int setTIME(int d, int h, int m, int s);
int initTIME(int fx, int fy);
int getTIME(struct _VTIME *vt);
int infoTIME(char *msg);
int waitTIME(int d, int h, int m, int s, char *reply);
int describeTIME(char *reply);

void* thr_clock(void *);

#endif
