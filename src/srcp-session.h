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

#include "config-srcpd.h"


int startup_SESSION(void);

sessionid_t session_getnextID();

int start_session(sessionid_t, int);
int stop_session(sessionid_t);
int describeSESSION(bus_t, sessionid_t, char *);
int termSESSION(bus_t, sessionid_t, sessionid_t, char *);

int session_preparewait(bus_t);
int session_wait(bus_t, unsigned int timeout, int *result);
int session_endwait(bus_t, int returnvalue);
int session_cleanupwait(bus_t);

int session_processwait(bus_t);

#endif
