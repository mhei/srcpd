/***************************************************************************
                          srcp-info.c  -  description
                             -------------------
    begin                : Mon May 20 2002
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

/*
   This code manages INFO SESSIONs. Every hardware driver must call
   (directly or via set<devicegroup> functions) the queueInfoMessage().
   This function copies the preformated string into an internal buffer
   (allocated) and adjusts the current writer position (variable »in«).
   To avoid memory access confusion, a mutex protects the writing
   process.

   On the other end of the pipe are numerous threads waiting to read new
   data.  Each of these threads maintains its own reader position
   (parameter »current«) to unqueue the recently added messages.

   When a new INFO sessions starts, it will first send all savailable
   status data and then enter the reader cycle.
 */

#include "stdincludes.h"

#include "io.h"
#include "config-srcpd.h"
#include "srcp-gl.h"
#include "srcp-ga.h"
#include "srcp-fb.h"
#include "srcp-sm.h"
#include "srcp-power.h"
#include "srcp-info.h"
#include "srcp-error.h"
#include "srcp-descr.h"
#include "srcp-time.h"
#include "srcp-session.h"
#include "syslogmessage.h"


#define QUEUELENGTH_INFO 1000

static char *info_queue[QUEUELENGTH_INFO];
static int in = 0;

static pthread_mutex_t queue_mutex_info = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t info_available = PTHREAD_COND_INITIALIZER;

/* queue a pre-formatted message */
int queueInfoMessage(char *msg)
{
    pthread_mutex_lock(&queue_mutex_info);
    /* Queue contains copies of all message strings */
    free(info_queue[in]);
    info_queue[in] = calloc(strlen(msg) + 1, 1);
    strcpy(info_queue[in], msg);
    /* now increase the in value, the readers may see new data! */
    in++;
    if (in == QUEUELENGTH_INFO)
        in = 0;

    /*tell all waiting info clients about new message*/
    pthread_cond_broadcast(&info_available);
    pthread_mutex_unlock(&queue_mutex_info);
    return SRCP_OK;
}


static int queueIsEmptyInfo(int current)
{
    return (in == current);
}

/* returns value for next item or -1, resets fifo buffer pointer! */
static int unqueueNextInfo(int current, char *info)
{
    if (in == current)
        return -1;

    pthread_mutex_lock(&queue_mutex_info);
    strcpy(info, info_queue[current]);
    pthread_mutex_unlock(&queue_mutex_info);

    /* calculation can be outside of lock because "current" is local
     * to socket sender thread */
    current++;
    if (current == QUEUELENGTH_INFO)
        current = 0;
    return current;
}

/*clear info message queue and level indicator*/
int startup_INFO(void)
{
    in = 0;
    memset(info_queue, 0, sizeof(info_queue));
    return SRCP_OK;
}

/**
 * Endless loop for new info mode client
 * terminates on write failure
 **/
int doInfoClient(client_thread_t* ctd)
{
    int i, current, number, value;
    char reply[1000], description[1000];

    /* send start up-information to a new client */
    struct timeval cmp_time;
    bus_t busnumber;
    current = in;
    syslog_bus(0, DBG_DEBUG, "New INFO client requested %ld", ctd->session);

    for (busnumber = 0; busnumber <= num_buses; busnumber++) {
        pthread_testcancel();
        syslog_bus(busnumber, DBG_DEBUG,
            "send all data for bus number %d to new client", busnumber);
        /* first some global bus data */
        /* send Descriptions for buses */
        describeBus(busnumber, reply);
        if (socket_writereply(ctd->socket, reply) < 0)
            return -1;
        strcpy(description, reply);
        *reply = 0x00;
        
        if (strstr(description, "POWER")) {
            infoPower(busnumber, reply);
            if (socket_writereply(ctd->socket, reply) < 0)
                return -1;
            *reply = 0x00;
        }
        
        if (strstr(description, "TIME")) {
            describeTIME(reply);
            if (socket_writereply(ctd->socket, reply) < 0)
                return -1;
            *reply = 0x00;
            infoTIME(reply);
            if (socket_writereply(ctd->socket, reply) < 0)
                return -1;
            *reply = 0x00;
        }

        /* send all needed generic locomotives */
        if (strstr(description, "GL")) {
            number = getMaxAddrGL(busnumber);
            for (i = 1; i <= number; i++) {
                if (isInitializedGL(busnumber, i)) {
                    sessionid_t lockid;
                    cacheDescribeGL(busnumber, i, reply);
                    if (socket_writereply(ctd->socket, reply) < 0)
                        return -1;
                    *reply = 0x00;
                    cacheInfoGL(busnumber, i, reply);
                    if (socket_writereply(ctd->socket, reply) < 0)
                        return -1;
                    *reply = 0x00;
                    cacheGetLockGL(busnumber, i, &lockid);
                    if (lockid != 0) {
                        describeLOCKGL(busnumber, i, reply);
                        if (socket_writereply(ctd->socket, reply) < 0)
                            return -1;
                        *reply = 0x00;
                    }
                }
            }
        }
        
        /* send all needed generic assesoires */
        if (strstr(description, "GA")) {
            number = get_number_ga(busnumber);
            for (i = 1; i <= number; i++) {
                if (isInitializedGA(busnumber, i)) {
                    sessionid_t lockid;
                    int rc, port;
                    describeGA(busnumber, i, reply);
                    if (socket_writereply(ctd->socket, reply) < 0)
                        return -1;
                    *reply = 0x00;
                    for (port = 0; port <= 1; port++) {
                        rc = infoGA(busnumber, i, port, reply);
                        if ((rc == SRCP_INFO)) {
                            if (socket_writereply(ctd->socket, reply) < 0)
                                return -1;
                            *reply = 0x00;
                        }
                    }
                    getlockGA(busnumber, i, &lockid);
                    if (lockid != 0) {
                        describeLOCKGA(busnumber, i, reply);
                        if (socket_writereply(ctd->socket, reply) < 0)
                            return -1;
                        *reply = 0x00;
                    }

                }
            }
        }

        /* send all needed feedbacks */
        if (strstr(description, "FB")) {
            number = get_number_fb(busnumber);
            for (i = 1; i <= number; i++) {
                int rc = getFB(busnumber, i, &cmp_time, &value);
                if (rc == SRCP_OK && value != 0) {
                    infoFB(busnumber, i, reply);
                    if (socket_writereply(ctd->socket, reply) < 0)
                        return -1;
                    *reply = 0x00;
                }
            }
        }
    }
    syslog_bus(0, DBG_DEBUG, "All messages send to new INFO client "
            "(session: %ld)\n", ctd->session);

    /* There is a kind of race condition: Newly queued messages may be
     * ignored until we reach this point. But there is no message loss
     * because the following while loop will detect and send them.
     */
    
    while (1) {
        pthread_testcancel();

        /*get mutex lock and wait for new messages*/
        pthread_mutex_lock(&queue_mutex_info);
        while (queueIsEmptyInfo(current))
            pthread_cond_wait(&info_available, &queue_mutex_info);
        pthread_mutex_unlock(&queue_mutex_info);

        /* loop to send all new messages to SRCP client */
        while (!queueIsEmptyInfo(current)) {
            current = unqueueNextInfo(current, reply);
            syslog_bus(0, DBG_DEBUG, "reply-length = %d", strlen(reply));
            if (socket_writereply(ctd->socket, reply) < 0)
                return -1;
        }
    }
    return 0;
}
