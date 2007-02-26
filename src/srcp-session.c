/***************************************************************************
                          srcp-session.c  -  description
                             -------------------
    begin                : Don Apr 25 2002
    copyright            : (C) 2002 by
    email                :
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#include "stdincludes.h"

#include "config-srcpd.h"
#include "srcp-session.h"
#include "srcp-ga.h"
#include "srcp-gl.h"
#include "srcp-error.h"
#include "srcp-info.h"

static pthread_mutex_t cb_mutex[MAX_BUSSES];
static pthread_cond_t cb_cond[MAX_BUSSES];
static int cb_data[MAX_BUSSES];

static sessionid_t SessionID = MAX_BUSSES + 1;
static pthread_mutex_t SessionID_mut = PTHREAD_MUTEX_INITIALIZER;

sessionid_t session_getnextID()
{
    sessionid_t result;
    pthread_mutex_lock(&SessionID_mut);
    result = SessionID++;
    pthread_mutex_unlock(&SessionID_mut);
    return result;
}


/**
 * First initialisation after program startup
 */
int startup_SESSION(void)
{
    int i;
    for (i = 0; i < MAX_BUSSES; i++) {
        pthread_mutex_init(&cb_mutex[i], NULL);
        pthread_cond_init(&cb_cond[i], NULL);
    }
    return 0;
}

int start_session(sessionid_t sessionid, int mode)
{

    char msg[1000];
    struct timeval akt_time;
    gettimeofday(&akt_time, NULL);
    DBG(0, DBG_INFO, "Session started; client-ID %ld, mode %d", sessionid,
        mode);
    sprintf(msg, "%lu.%.3lu 101 INFO 0 SESSION %lu %s\n", akt_time.tv_sec,
            akt_time.tv_usec / 1000, sessionid,
            (mode == 1 ? "COMMAND" : "INFO"));
    queueInfoMessage(msg);
    return SRCP_OK;
}

/**
 * called by netserver after finishing the session-loop
 */
int stop_session(sessionid_t sessionid)
{
    char msg[1000];
    struct timeval akt_time;
    gettimeofday(&akt_time, NULL);

    DBG(0, DBG_INFO, "Session terminated client-ID %ld", sessionid);
    // clean all locks
    unlock_ga_bysessionid(sessionid);
    unlock_gl_bysessionid(sessionid);
    sprintf(msg, "%lu.%.3lu 102 INFO 0 SESSION %lu\n", akt_time.tv_sec,
            akt_time.tv_usec / 1000, sessionid);
    queueInfoMessage(msg);
    return SRCP_OK;
}

int describeSESSION(bus_t bus, sessionid_t sessionid, char *reply)
{
    return SRCP_UNSUPPORTEDOPERATION;
}

/**
 * called by srcp command session to finish a session;
 * return negative value of SRCP_OK to ack the request.
 */
int termSESSION(bus_t bus, sessionid_t sessionid, sessionid_t termsessionid,
                char *reply)
{
    if (sessionid == termsessionid || termsessionid == 0) {
        return -SRCP_OK;
    }
    return SRCP_FORBIDDEN;
}

int session_preparewait(bus_t busnumber)
{
    DBG(busnumber, DBG_DEBUG, "SESSION prepare wait for bus %d",
        busnumber);
    return pthread_mutex_lock(&cb_mutex[busnumber]);
}

int session_cleanupwait(bus_t busnumber)
{
    DBG(busnumber, DBG_DEBUG, "SESSION cleanup wait for bus %d",
        busnumber);
    return pthread_mutex_unlock(&cb_mutex[busnumber]);
}

int session_wait(bus_t busnumber, unsigned int timeout, int *result)
{
    int rc;
    struct timespec stimeout;
    struct timeval now;
    gettimeofday(&now, NULL);
    stimeout.tv_sec = now.tv_sec + timeout;
    stimeout.tv_nsec = now.tv_usec * 1000;

    DBG(busnumber, DBG_DEBUG, "SESSION start wait1 for bus %d", busnumber);
    rc = pthread_cond_timedwait(&cb_cond[busnumber], &cb_mutex[busnumber],
                                &stimeout);
    *result = cb_data[busnumber];
    DBG(busnumber, DBG_DEBUG, "SESSION start wait2 for bus %d", busnumber);
    return rc;
}

int session_endwait(bus_t busnumber, int returnvalue)
{
    DBG(busnumber, DBG_DEBUG, "SESSION end wait1 for bus %d", busnumber);
    cb_data[busnumber] = returnvalue;
    pthread_cond_broadcast(&cb_cond[busnumber]);
    pthread_mutex_unlock(&cb_mutex[busnumber]);
    DBG(busnumber, DBG_DEBUG, "SESSION end wait2 for bus %d", busnumber);
    return returnvalue;
}

int session_processwait(bus_t busnumber)
{
    int rc;
    DBG(busnumber, DBG_DEBUG, "SESSION process wait1 for bus %d",
        busnumber);
    rc = pthread_mutex_lock(&cb_mutex[busnumber]);
    DBG(busnumber, DBG_DEBUG, "SESSION process wait2 for bus %d",
        busnumber);
    return rc;
}
