/* $Id$ */

/* 
 * Vorliegende Software unterliegt der General Public License, 
 * Version 2, 1991. (c) Matthias Trute, 2000-2001.
 *
 */


#include <unistd.h>
#include <stdio.h>

#include "srcp-time.h"
#include "srcp-error.h"

struct _VTIME vtime;

int
startup_TIME(void)
{
  return 0;
}

int
setTime(int d, int h, int m, int s, int rx, int ry)
{
  if(d<0 || h<0 || h>23 || m<0 || m>59 || s<0 || s>59 || rx<0 || ry<0)
    return SRCP_WRONGVALUE;
  vtime.day     = d;
  vtime.hour    = h;
  vtime.min     = m;
  vtime.sec     = s;
  vtime.ratio_x = rx;
  vtime.ratio_y = ry;
  return SRCP_OK;
}

int
getTime(struct _VTIME *vt)
{
  *vt = vtime;
  return SRCP_OK;
}

int
infoTime(struct _VTIME vt, char *msg)
{
  sprintf(msg, "INFO TIME %d %d %d %d %d %d\n", vt.day, vt.hour,
      vt.min, vt.sec, vt.ratio_x, vt.ratio_y);
  return SRCP_OK;
}

int
cmpTime(struct timeval *t1, struct timeval *t2)
{
  int result;

  result = 0;
  if(t2->tv_sec > t1->tv_sec)
  {
    result = 1;
  }
  else
  {
    if(t2->tv_sec == t1->tv_sec)
    {
      if(t2->tv_usec > t1->tv_usec)
      {
        result = 1;
      }
    }
  }
  return result;
}



/***********************************************************************
 * Zeitgeber, aktualisiert die Datenstrukturen im Modellsekundenraster *
 ***********************************************************************/
void*
thr_clock(void* v)
{
  struct _VTIME vt;

  vtime.ratio_x=0;
  vtime.ratio_y=0;
  while(1)
  {
    unsigned long sleeptime;
    if(vtime.ratio_x==0 || vtime.ratio_y==0)
    {
      sleep(1);
      continue;
    }
    /* delta Modellzeit = delta realzeit * ratio_x/ratio_y */
    sleeptime = (1000000 * vtime.ratio_y) / vtime.ratio_x;
    usleep(sleeptime);
    vt = vtime; // fürs Rechnen eine temporäre Kopie. Damit ist vtime immer gültig
    vt.sec ++;
    if(vt.sec>=60)
    {
      vt.sec = 0;
      vt.min ++;
    }
    if(vt.min >= 60)
    {
      vt.hour++;
      vt.min = 0;
    }
    if(vt.hour>=24)
    {
      vt.day++;
      vt.hour = 0;
    }
    vtime = vt;
    // syslog(LOG_INFO, "time: %d %d %d %d %d %d", vtime.day, vtime.hour, vtime.min, vtime.sec,  vtime.ratio_x, vtime.ratio_y);
  }
}
