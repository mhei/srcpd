
/* 
 * Vorliegende Software unterliegt der General Public License, 
 * Version 2, 1991. (c) Matthias Trute, 2000-2001.
 *
 */
#include "stdincludes.h"

#include "srcp-time.h"
#include "srcp-error.h"
#include "srcp-info.h"

struct _VTIME vtime;

int startup_TIME(void)
{
  gettimeofday(&vtime.inittime, NULL);
  return 0;
}

int setTIME(int d, int h, int m, int s)
{
  if(d<0 || h<0 || h>23 || m<0 || m>59 || s<0 || s>59)
    return SRCP_WRONGVALUE;
  vtime.day     = d;
  vtime.hour    = h;
  vtime.min     = m;
  vtime.sec     = s;
  return SRCP_OK;
}

int initTIME(int fx, int fy)
{
  char msg[100];
  if(fx<0 || fy<=0)
    return SRCP_WRONGVALUE;
  vtime.ratio_x = fx;
  vtime.ratio_y = fy;
  gettimeofday(&vtime.inittime, NULL);
  describeTIME(msg);
  queueInfoMessage(msg);
  return SRCP_OK;
}

int getTIME(struct _VTIME *vt)
{
  *vt = vtime;
  return SRCP_OK;
}

int infoTIME(char *msg)
{
  struct timeval akt_time;
  gettimeofday(&akt_time, NULL);
  msg[0]=0x00;
  if(vtime.ratio_x == 0 && vtime.ratio_y == 0)
    return SRCP_NODATA;
  sprintf(msg, "%lu.%.3lu 100 INFO 0 TIME %d %d %d %d\n", akt_time.tv_sec, akt_time.tv_usec/1000,
    vtime.day, vtime.hour, vtime.min, vtime.sec);
  return SRCP_OK;
}

int describeTIME(char *reply) {
  sprintf(reply, "%lu.%.3lu 101 INFO 0 TIME %d %d\n", vtime.inittime.tv_sec, vtime.inittime.tv_usec/1000, vtime.ratio_x, vtime.ratio_y);
  return SRCP_OK;
}

/***********************************************************************
 * Zeitgeber, aktualisiert die Datenstrukturen im Modellsekundenraster *
 ***********************************************************************/
void* thr_clock(void* v)
{
  struct _VTIME vt;
  int sendinfo = 0;
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
      sendinfo = 1;
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
    if (sendinfo==1) {
      char msg[100];
      sendinfo = 0;
      infoTIME(msg);
      queueInfoMessage(msg);
    }
    // syslog(LOG_INFO, "time: %d %d %d %d %d %d", vtime.day, vtime.hour, vtime.min, vtime.sec,  vtime.ratio_x, vtime.ratio_y);
  }
}
