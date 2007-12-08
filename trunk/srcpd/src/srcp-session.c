/**************************************************************************
                          srcp-session.c
                         -------------------
    begin                : Don Apr 25 2002
    copyright            : (C) 2002 by
    email                :
 **************************************************************************/

/**************************************************************************
 *                                                                        *
 *  This program is free software; you can redistribute it and/or modify  *
 *  it under the terms of the GNU General Public License as published by  *
 *  the Free Software Foundation; either version 2 of the License, or     *
 *  (at your option) any later version.                                   *
 *                                                                        *
 **************************************************************************/

#include "stdincludes.h"

#include "srcp-session.h"
#include "srcp-ga.h"
#include "srcp-gl.h"
#include "srcp-error.h"
#include "srcp-info.h"
#include "syslogmessage.h"


/* 
 * A linked list stores the data of all connected sessions. This code was
 * mainly impressed by:
 *   http://en.wikipedia.org/wiki/Linked_list#Language_support
 */

/*session list node to store session data*/
typedef struct sn {
    sessionid_t session;
    pthread_t thread;
    struct sn *next;
} session_node_t;

/*session counter, session list root and mutex to lock access*/
static sessionid_t lastsession = 0;
static unsigned int runningsessions = 0;
static session_node_t* session_list = NULL;
static pthread_mutex_t session_list_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t session_run_cond = PTHREAD_COND_INITIALIZER;

static pthread_mutex_t cb_mutex[MAX_BUSES];
static pthread_cond_t cb_cond[MAX_BUSES];
static int cb_data[MAX_BUSES];



/*add new session data node to list*/
static session_node_t *list_add(session_node_t **p, sessionid_t sid,
        pthread_t tid)
{
    session_node_t *n = (session_node_t *)malloc(sizeof(session_node_t));
    if (n == NULL)
        return n;
    n->next = *p;
    *p = n;
    n->session = sid;
    n->thread = tid;
    return n;
}

/*remove session data node from list*/
static void list_remove(session_node_t **p)
{
    if (*p != NULL) {
        session_node_t *n = *p;
        *p = (*p)->next;
        free(n);
    }
}

/* search sessionid in list, return pointer to node */
static session_node_t **list_search_session(session_node_t **n,
        sessionid_t sid) {
    while (*n != NULL) {
        if ((*n)->session == sid) {
            return n;
        }
        n = &(*n)->next;
    }
    return NULL;
}

/* search thread id by sessionid, return thread id */
static pthread_t list_search_thread_by_sessionid(session_node_t **n,
        sessionid_t sid) {
    while (*n != NULL) {
        if ((*n)->session == sid) {
            return (*n)->thread;
        }
        n = &(*n)->next;
    }
    return 0;
}

/**
 * First initialisation after program start up
 */
int startup_SESSION(void)
{
    int i;
    for (i = 0; i < MAX_BUSES; i++) {
        pthread_mutex_init(&cb_mutex[i], NULL);
        pthread_cond_init(&cb_cond[i], NULL);
    }
    return 0;
}

/* Create new session data node and return the new sessionid.
 * On failure function returns 0. */
sessionid_t session_create(pthread_t thread)
{
    session_node_t* node = NULL;
    sessionid_t session = 0;

    pthread_mutex_lock(&session_list_mutex);
    lastsession++;
    node = list_add(&session_list, lastsession, thread);
    if (NULL != node) {
        session = lastsession;
        runningsessions++;
    }
    else
        lastsession--;
    pthread_mutex_unlock(&session_list_mutex);
    return session;
}

/* Called by client thread after session_stop() to remove a no longer
 * valid session node */
void session_destroy(sessionid_t session)
{
    pthread_mutex_lock(&session_list_mutex);
    list_remove(list_search_session(&session_list, session));
    runningsessions--;
    pthread_cond_signal(&session_run_cond);
    pthread_mutex_unlock(&session_list_mutex);
}

/* terminate a session by cancellation of client thread */
void session_terminate(sessionid_t session)
{
    pthread_t pc;

    pthread_mutex_lock(&session_list_mutex);
    pc = list_search_thread_by_sessionid(&session_list, session);
    pthread_mutex_unlock(&session_list_mutex);
    if (0 != pc)
        pthread_cancel(pc);
}

/* terminate all active sessions */
void terminate_all_sessions()
{
    if (session_list == NULL)
        return;

    session_node_t* node = session_list;   

    /*first cancel all session threads ...*/
    pthread_mutex_lock(&session_list_mutex);
    while (node != NULL) {
        pthread_cancel(node->thread);
        node = node->next;
    }

    /*... then wait for complete termination*/
    while (runningsessions != 0)
        pthread_cond_wait(&session_run_cond, &session_list_mutex);
    pthread_mutex_unlock(&session_list_mutex);
}

/*this function is used by clientservice to start the session*/
int start_session(sessionid_t sessionid, int mode)
{
    char msg[1000];
    struct timeval akt_time;
    gettimeofday(&akt_time, NULL);

    sprintf(msg, "%lu.%.3lu 101 INFO 0 SESSION %lu %s\n", akt_time.tv_sec,
            akt_time.tv_usec / 1000, sessionid,
            (mode == 1 ? "COMMAND" : "INFO"));
    queueInfoMessage(msg);

    syslog_session(sessionid, DBG_INFO, "Session started with mode %d", mode);
    return SRCP_OK;
}

/**
 * this funtion is called by clientservice when the client thread
 * terminates
 */
int stop_session(sessionid_t sessionid)
{
    char msg[1000];
    struct timeval akt_time;
    gettimeofday(&akt_time, NULL);

    /* clean all locks */
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
    if (sessionid == termsessionid) {
        stop_session(termsessionid);
        session_terminate(termsessionid);
        return -SRCP_OK;
    }
    return SRCP_FORBIDDEN;
}

int session_preparewait(bus_t busnumber)
{
    syslog_bus(busnumber, DBG_DEBUG, "SESSION prepare wait for bus %ld",
        busnumber);
    return pthread_mutex_lock(&cb_mutex[busnumber]);
}

int session_cleanupwait(bus_t busnumber)
{
    syslog_bus(busnumber, DBG_DEBUG, "SESSION cleanup wait for bus %ld",
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

    syslog_bus(busnumber, DBG_DEBUG, "SESSION start wait1 for bus %ld", busnumber);
    rc = pthread_cond_timedwait(&cb_cond[busnumber], &cb_mutex[busnumber],
                                &stimeout);
    *result = cb_data[busnumber];
    syslog_bus(busnumber, DBG_DEBUG, "SESSION start wait2 for bus %ld", busnumber);
    return rc;
}

int session_endwait(bus_t busnumber, int returnvalue)
{
    syslog_bus(busnumber, DBG_DEBUG, "SESSION end wait1 for bus %ld", busnumber);
    cb_data[busnumber] = returnvalue;
    pthread_cond_broadcast(&cb_cond[busnumber]);
    pthread_mutex_unlock(&cb_mutex[busnumber]);
    syslog_bus(busnumber, DBG_DEBUG, "SESSION end wait2 for bus %ld", busnumber);
    return returnvalue;
}

int session_processwait(bus_t busnumber)
{
    int rc;
    syslog_bus(busnumber, DBG_DEBUG, "SESSION process wait1 for bus %ld",
        busnumber);
    rc = pthread_mutex_lock(&cb_mutex[busnumber]);
    syslog_bus(busnumber, DBG_DEBUG, "SESSION process wait2 for bus %ld",
        busnumber);
    return rc;
}
