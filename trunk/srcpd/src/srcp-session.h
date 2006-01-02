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

int startup_SESSION(void);

int start_session(long int sessionid, int mode);
int stop_session(long int sessionid);
int describeSESSION(int bus, int sessionid, char *reply);
int termSESSION(int bus, int sessionid, int termsessionid, char *reply);

int session_preparewait(int busnumber);
int session_wait(int busnumber, struct timespec timeout, int *result);
int session_endwait(int busnumber, int returnvalue);
int session_cleanupwait(int busnumber);

int session_processwait(int busnumber);
#endif
