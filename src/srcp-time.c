
/* 
 * Vorliegende Software unterliegt der General Public License, 
 * Version 2, 1991. (c) Matthias Trute, 2000-2001.
 *
 */
#include "stdincludes.h"

#include "srcp-time.h"
#include "srcp-error.h"

struct _VTIME vtime;

int startup_TIME(void)
{
  return 0;
}

int setTime(int d, int h, int m, int s)
{
  if(d<0 || h<0 || h>23 || m<0 || m>59 || s<0 || s>59)
    return SRCP_WRONGVALUE;
  vtime.day     = d;
  vtime.hour    = h;
  vtime.min     = m;
  vtime.sec     = s;
  return SRCP_OK;
}

int initTime(int fx, int fy)
{
  if(fx<0 || fy<=0)
    return SRCP_WRONGVALUE;
  vtime.ratio_x = fx;
  vtime.ratio_y = fy;
  return SRCP_OK;
}

int getTime(struct _VTIME *vt)
{
  *vt = vtime;
  return SRCP_OK;
}

int infoTime(struct _VTIME vt, char *msg)
{
  struct timeval akt_time;
  gettimeofday(&akt_time, NULL);
  sprintf(msg, "%ld.%ld 100 INFO 0 TIME %d %d %d %d", akt_time.tv_sec, akt_time.tv_usec/1000, vt.day, vt.hour, vt.min, vt.sec);
  return SRCP_OK;
}

int cmpTime(struct timeval *t1, struct timeval *t2)
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


int describeTIME(int bus, int addr, char *reply) {
	
	struct timeval tv;
	
	gettimeofday(&tv, NULL);
	
  //sprintf(reply, "INFO 0 TIME %d %d", vtime.ratio_x, vtime.ratio_y);
	sprintf(reply, "%ld.%ld 100 INFO 0 TIME %d %d\n",
		tv.tv_sec, tv.tv_usec/1000,
		vtime.ratio_x, vtime.ratio_y);
  
	return SRCP_OK;
}

/***********************************************************************
 * Zeitgeber, aktualisiert die Datenstrukturen im Modellsekundenraster *
 ***********************************************************************/
void* thr_clock(void* v)
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
      // queueInfoTIME();
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
