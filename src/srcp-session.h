/***************************************************************************
                          srcp-session.h  -  description
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

#ifndef _SRCP_SESSION_H
#define _SRCP_SESSION_H

#include <pthread.h>

#include "config-srcpd.h"
#include "queue.h"

/*session modes, should be enum type*/
#define smUndefined 0
#define smCommand   1
#define smInfo      2


/*session list node to store session data*/
typedef struct sn {
    sessionid_t session;
    pthread_t thread;
    int socket;
    int mode;
    queue_t queue;
    pthread_mutex_t queue_mutex;
    pthread_cond_t queue_cond;
    struct sn *next;
} session_node_t;


int startup_SESSION(void);

session_node_t* create_anonymous_session(int);
void register_session(session_node_t*);
void destroy_anonymous_session(session_node_t*);
void destroy_session(sessionid_t);
void terminate_all_sessions();
int is_valid_info_session(sessionid_t);
void session_enqueue_info_message(sessionid_t, const char*);

int start_session(session_node_t*);
int stop_session(sessionid_t);
int describeSESSION(bus_t, sessionid_t, char*);
int termSESSION(bus_t, sessionid_t, sessionid_t, char*);

int session_preparewait(bus_t);
int session_wait(bus_t, unsigned int timeout, int *result);
int session_endwait(bus_t, int returnvalue);
int session_cleanupwait(bus_t);
int session_processwait(bus_t);

#endif
