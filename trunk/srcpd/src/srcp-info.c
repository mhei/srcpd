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
   Manages the INFO SESSIONs. Every Hardware driver must call (directly or
   via set<devicegroup> functions the queueInfoMessage. This function will
   copy the preformated string into internal buffer (allocated) and sets
   the current writer position (variable in). To avoid confusion, a semaphore
   protects this process.

   On the other end of the pipe are numerous threads waiting for
   new data. Each and every of these threads maintains its own reader position
   (parameter current) to unqueue the recently added messages.

   When a new INFO sessions starts, it will send all status data and
   after that will enter the reader cycle.
 */
#include "stdincludes.h"

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

#include "netserver.h"
#include "io.h"

#define QUEUELENGTH_INFO 1000

static char *info_queue[QUEUELENGTH_INFO];
static pthread_mutex_t queue_mutex_info;

static int in = 0;

/* queue a pre-formatted message */
int queueInfoMessage(char *msg)
{
    pthread_mutex_lock(&queue_mutex_info);
    /* Queue macht Kopien der Werte */
    free(info_queue[in]);
    info_queue[in] = calloc(strlen(msg) + 1, 1);
    strcpy(info_queue[in], msg);
    /* now increase the in value, the readers may see new data! */
    in++;
    if (in == QUEUELENGTH_INFO)
        in = 0;
    pthread_mutex_unlock(&queue_mutex_info);
    return SRCP_OK;
}


int queueIsEmptyInfo(int current)
{
    return (in == current);
}

/** liefert n�hsten Eintrag oder -1, setzt fifo pointer neu! */
int unqueueNextInfo(int current, char *info)
{
    if (in == current)
        return -1;

    pthread_mutex_lock(&queue_mutex_info);
    strcpy(info, info_queue[current++]);
    if (current == QUEUELENGTH_INFO)
        current = 0;
    pthread_mutex_unlock(&queue_mutex_info);
    return current;
}

int startup_INFO(void)
{
    int i;
    pthread_mutex_init(&queue_mutex_info, NULL);
    in = 0;
    for (i = 0; i < QUEUELENGTH_INFO; i++) {
        info_queue[i] = NULL;
    }
    return SRCP_OK;
}

/**
 * Endless loop for new info mode client
 * terminates on write failure
 **/
int doInfoClient(int Socket, sessionid_t sessionid)
{
    int status, i, current, number, value;
    char reply[1000], description[1000];

    // send start up-information to a new client
    struct timeval cmp_time;
    bus_t busnumber;
    current = in;
    DBG(0, DBG_DEBUG, "new Info-client requested %ld", sessionid);
    for (busnumber = 0; busnumber <= num_buses; busnumber++) {
        DBG(busnumber, DBG_DEBUG,
            "send all data for bus number %d to new client", busnumber);
        // first some global bus data
        // send Descriptions for buses
        describeBus(busnumber, reply);
        socket_writereply(Socket, reply);
        strcpy(description, reply);
        *reply = 0x00;
        
        if (strstr(description, "POWER")) {
            infoPower(busnumber, reply);
            socket_writereply(Socket, reply);
            *reply = 0x00;
        }
        
        if (strstr(description, "TIME")) {
            describeTIME(reply);
            socket_writereply(Socket, reply);
            *reply = 0x00;
            infoTIME(reply);
            socket_writereply(Socket, reply);
            *reply = 0x00;
        }

        // send all needed generic locomotives
        if (strstr(description, "GL")) {
            number = getMaxAddrGL(busnumber);
            for (i = 1; i <= number; i++) {
                if (isInitializedGL(busnumber, i)) {
                    sessionid_t lockid;
                    describeGL(busnumber, i, reply);
                    socket_writereply(Socket, reply);
                    *reply = 0x00;
                    infoGL(busnumber, i, reply);
                    socket_writereply(Socket, reply);
                    *reply = 0x00;
                    getlockGL(busnumber, i, &lockid);
                    if (lockid != 0) {
                        describeLOCKGL(busnumber, i, reply);
                        socket_writereply(Socket, reply);
                        *reply = 0x00;
                    }
                }
            }
        }
        
        // send all needed generic assesoires
        if (strstr(description, "GA")) {
            number = get_number_ga(busnumber);
            for (i = 1; i <= number; i++) {
                if (isInitializedGA(busnumber, i)) {
                    sessionid_t lockid;
                    int rc, port;
                    describeGA(busnumber, i, reply);
                    socket_writereply(Socket, reply);
                    *reply = 0x00;
                    for (port = 0; port <= 1; port++) {
                        rc = infoGA(busnumber, i, port, reply);
                        if ((rc == SRCP_INFO)) {
                            socket_writereply(Socket, reply);
                            *reply = 0x00;
                        }
                    }
                    getlockGA(busnumber, i, &lockid);
                    if (lockid != 0) {
                        describeLOCKGA(busnumber, i, reply);
                        socket_writereply(Socket, reply);
                        *reply = 0x00;
                    }

                }
            }
        }

        // send all needed feedbacks
        if (strstr(description, "FB")) {
            number = get_number_fb(busnumber);
            for (i = 1; i <= number; i++) {
                int rc = getFB(busnumber, i, &cmp_time, &value);
                if (rc == SRCP_OK && value != 0) {
                    infoFB(busnumber, i, reply);
                    socket_writereply(Socket, reply);
                    *reply = 0x00;
                }
            }
        }
    }
    DBG(0, DBG_DEBUG, "all data to new Info-Client (%ld) sent", sessionid);

    /* This is a racing condition: we should stop queuing new
       messages until we reach this this point, it is possible to
       miss some data changed since we started this thread */
    if (in != current) {
        DBG(0, DBG_WARN,
            "INFO queue dropped some information (%d elements). Sorry",
            abs(in - current));
    }
    current = in;
    
    while (1) {
        // busy waiting, anyone with better code out there?
        while (queueIsEmptyInfo(current))
            usleep(2000);

        current = unqueueNextInfo(current, reply);
        DBG(0, DBG_DEBUG, "reply-length = %d", strlen(reply));
        status = socket_writereply(Socket, reply);
        if (status < 0) {
            break;
        }
    }
    return 0;
}
