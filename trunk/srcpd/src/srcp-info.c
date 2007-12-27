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
   This code manages INFO SESSIONs. Every hardware driver, alias »bus
   process«, must call (directly or via set<devicegroup> functions) the
   queueInfoMessage() function. This function copies the preformated
   string into the session specific message queues. To avoid memory
   access confusion, a mutex protects the reading and writing processes.

   On the other end of each session queue the information session
   process is waiting for new enqueued messages to write them to the
   socket file descriptor. It is blocked by a condition variable to only
   be bussy when real work has to be done.

   When a new INFO sessions starts, it will first send all available
   status data and then enter the wait for new messages state.
 */

#include <string.h>
#include <stdlib.h>

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


/* Enqueue a pre-formatted message */
int queueInfoMessage(char *msg)
{
    session_enqueue_info_message(0, msg);
    return SRCP_OK;
}

/* There is nothing to do here. */
int startup_INFO(void)
{
    return SRCP_OK;
}

/* Dequeue info message; write message to socket and free message
 * memory.
 * return values:
 *      -1 if write to file descriptor failed
 *       0 if all is OK
 */
static ssize_t process_queue_messages(session_node_t* sn)
{
    ssize_t result = 0;
    qitem_t qi;

    pthread_mutex_lock(&sn->queue_mutex);
    while (!queue_is_empty(&sn->queue) && result != -1) {
        dequeue(&qi, &sn->queue);
        result = writen_amlb(sn->socket, qi.message);
        free(qi.message);
    }
    pthread_mutex_unlock(&sn->queue_mutex);
    return result;
}

    
/**
 * Handler for info mode client thread;
 * terminates on write failure or if cancelled externaly
 **/
int doInfoClient(session_node_t* sn)
{
    int i, number, value;
    char reply[1000], description[1000];
    struct timeval cmp_time;
    bus_t busnumber;
    syslog_session(sn->session, DBG_DEBUG, "New INFO client requested.");

    /* send start up-information to a new client */
    for (busnumber = 0; busnumber <= num_buses; busnumber++) {
        pthread_testcancel();
        syslog_session(sn->session, DBG_DEBUG,
            "Send all data for bus number %d to new client.", busnumber);

        /* first some global bus data */
        /* send Descriptions for buses */
        describeBus(busnumber, reply);
        if (writen_amlb(sn->socket, reply) < 0)
            return -1;
        strcpy(description, reply);
        *reply = 0x00;
        
        if (strstr(description, "POWER")) {
            infoPower(busnumber, reply);
            if (writen_amlb(sn->socket, reply) < 0)
                return -1;
            *reply = 0x00;
        }
        
        if (strstr(description, "TIME")) {
            describeTIME(reply);
            if (writen_amlb(sn->socket, reply) < 0)
                return -1;
            *reply = 0x00;
            infoTIME(reply);
            if (writen_amlb(sn->socket, reply) < 0)
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
                    if (writen_amlb(sn->socket, reply) < 0)
                        return -1;
                    *reply = 0x00;
                    cacheInfoGL(busnumber, i, reply);
                    if (writen_amlb(sn->socket, reply) < 0)
                        return -1;
                    *reply = 0x00;
                    cacheGetLockGL(busnumber, i, &lockid);
                    if (lockid != 0) {
                        describeLOCKGL(busnumber, i, reply);
                        if (writen_amlb(sn->socket, reply) < 0)
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
                    if (writen_amlb(sn->socket, reply) < 0)
                        return -1;
                    *reply = 0x00;
                    for (port = 0; port <= 1; port++) {
                        rc = infoGA(busnumber, i, port, reply);
                        if ((rc == SRCP_INFO)) {
                            if (writen_amlb(sn->socket, reply) < 0)
                                return -1;
                            *reply = 0x00;
                        }
                    }
                    getlockGA(busnumber, i, &lockid);
                    if (lockid != 0) {
                        describeLOCKGA(busnumber, i, reply);
                        if (writen_amlb(sn->socket, reply) < 0)
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
                    if (writen_amlb(sn->socket, reply) < 0)
                        return -1;
                    *reply = 0x00;
                }
            }
        }
    }
    syslog_session(sn->session, DBG_DEBUG,
            "All messages send to new INFO client.\n");

    /* There is a kind of race condition: Newly enqueued messages may
     * be ignored until we reach this point. But there is no message
     * loss because the following while loop will detect and send them.
     */
    
    while (1) {
        pthread_testcancel();

        /*get mutex lock and wait for new messages*/
        /*TODO: socket close by client should be detected some how*/
        pthread_mutex_lock(&sn->queue_mutex);
        while (queue_is_empty(&sn->queue))
            pthread_cond_wait(&sn->queue_cond, &sn->queue_mutex);
        pthread_mutex_unlock(&sn->queue_mutex);

        /* send all new messages to SRCP client */
        if (-1 == process_queue_messages(sn))
            return (-1);
    }
    return 0;
}
