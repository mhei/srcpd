/* $Id$ */

/* 
 * Vorliegende Software unterliegt der General Public License, 
 * Version 2, 1991. (c) Matthias Trute, 2000-2001.
 *
 */


#ifndef _SRCPTIME_H_
#define _SRCPTIME_H_


/* Zeitgeber */
struct _VTIME {
    int day;
    int hour;
    int min;
    int sec;
    int ratio_x; /* ratio_x == 0 und die Uhr steht */
    int ratio_y;
};

void setTime(int d, int h, int m, int s, int rx, int ry);
void getTime(struct _VTIME *vt);
void infoTime(struct _VTIME, char *msg);

void* thr_clock(void *);

extern struct _VTIME vtime;

#endif
