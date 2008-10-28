/* 
 * Vorliegende Software unterliegt der General Public License, 
 * Version 2, 1991. (c) Matthias Trute, 2000-2001.
 */

#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include "config-srcpd.h"
#include "srcp-time.h"
#include "srcp-error.h"
#include "srcp-info.h"
#include "syslogmessage.h"


/*local variable for time thread*/
static pthread_t time_tid;

/*FIXME: this variable can be accessed by several threads at the
 * same time and should be protected by a lock*/
static vtime_t vtime;


int startup_TIME(void)
{
    gettimeofday(&vtime.inittime, NULL);
    return 0;
}

int setTIME(int d, int h, int m, int s)
{
    if (d < 0 || h < 0 || h > 23 || m < 0 || m > 59 || s < 0 || s > 59)
        return SRCP_WRONGVALUE;

    vtime.day = d;
    vtime.hour = h;
    vtime.min = m;
    vtime.sec = s;
    return SRCP_OK;
}

int initTIME(int fx, int fy)
{
    if (fx < 0 || fy <= 0)
        return SRCP_WRONGVALUE;

    char msg[100];
    vtime.ratio_x = fx;
    vtime.ratio_y = fy;
    gettimeofday(&vtime.inittime, NULL);
    describeTIME(msg);
    enqueueInfoMessage(msg);
    return SRCP_OK;
}

int getTIME(vtime_t *vt)
{
    *vt = vtime;
    return SRCP_OK;
}

int infoTIME(char *msg)
{
    msg[0] = '\0';

    if (vtime.ratio_x == 0 && vtime.ratio_y == 0)
        return SRCP_NODATA;

    struct timeval akt_time;
    gettimeofday(&akt_time, NULL);
    
    sprintf(msg, "%lu.%.3lu 100 INFO 0 TIME %d %d %d %d\n",
            akt_time.tv_sec, akt_time.tv_usec / 1000, vtime.day,
            vtime.hour, vtime.min, vtime.sec);
    
    return SRCP_INFO;
}

int describeTIME(char *reply)
{
    sprintf(reply, "%lu.%.3lu 101 INFO 0 TIME %d %d\n",
            vtime.inittime.tv_sec, vtime.inittime.tv_usec / 1000,
            vtime.ratio_x, vtime.ratio_y);
    return SRCP_INFO;
}

/***********************************************************************
 * Zeitgeber, aktualisiert die Datenstrukturen im Modellsekundenraster *
 ***********************************************************************/
void *thr_clock(void *v)
{
    vtime_t vt;
    bool sendinfo = false;
    vtime.ratio_x = 0;
    vtime.ratio_y = 0;

    while (1) {
        unsigned long sleeptime;
        if (vtime.ratio_x == 0 || vtime.ratio_y == 0) {
            sleep(1);
            continue;
        }
        /* delta Modellzeit = delta real time * ratio_x/ratio_y */
        sleeptime = (1000000 * vtime.ratio_y) / vtime.ratio_x;
        usleep(sleeptime);
        /* fürs Rechnen eine temporäre Kopie. Damit ist vtime immer gültig */
        vt = vtime;
        vt.sec++;
        if (vt.sec >= 60) {
            vt.sec = 0;
            vt.min++;
            sendinfo = true;
        }
        if (vt.min >= 60) {
            vt.hour++;
            vt.min = 0;
        }
        if (vt.hour >= 24) {
            vt.day++;
            vt.hour = 0;
        }
        vtime = vt;
        if (sendinfo) {
            sendinfo = false;
            char msg[100];
            infoTIME(msg);
            enqueueInfoMessage(msg);
        }
        /* syslog(LOG_INFO, "time: %d %d %d %d %d %d", vtime.day, vtime.hour, vtime.min, vtime.sec,  vtime.ratio_x, vtime.ratio_y); */
    }
}


/*create time/clock thread*/
void create_time_thread()
{
    int result;

    result = pthread_create(&time_tid, NULL, thr_clock, NULL);
    if (result != 0) {
        syslog_bus(0, DBG_ERROR, "Create time thread failed: %s "
                "(errno = %d)\n", strerror(result), result);
        return;
    }

    syslog_bus(0, DBG_DEBUG, "Time thread created.");
}

/*cancel time/clock thread*/
void cancel_time_thread()
{
    int result;

    result = pthread_cancel(time_tid);
    if (result != 0)
        syslog_bus(0, DBG_ERROR,
                "Time thread cancel failed: %s (errno = %d).",
                strerror(result), result);

    /*wait until time thread terminates*/
    result = pthread_join(time_tid, NULL);
    if (result != 0) {
        syslog_bus(0, DBG_ERROR,
                "Time thread join failed: %s (errno = %d).",
                strerror(result), result);
        return;
    }

    syslog_bus(0, DBG_DEBUG, "Time thread cancelled.");
}

