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

int startup_SESSION(void)
{
  return 0;
}

int start_session(long int sessionid, int mode)
{

  char msg[1000];
  struct timeval akt_time;
  gettimeofday(&akt_time, NULL);
  DBG(0, DBG_INFO, "Session started; clientid %ld, mode %d", sessionid, mode);
  sprintf(msg, "%lu.%.3lu 101 INFO 0 SESSION %lu %s\n", akt_time.tv_sec, akt_time.tv_usec/1000, sessionid,
     (mode==1?"COMMAND":"INFO"));
  queueMessage(msg);
  return SRCP_OK;

  return SRCP_OK;
}

int stop_session(long int sessionid)
{
  char msg[1000];
  struct timeval akt_time;
  gettimeofday(&akt_time, NULL);

  DBG(0, DBG_INFO, "Session terminated clientid %ld", sessionid);
  // clean all locks
  unlock_ga_bysessionid(sessionid);
  unlock_gl_bysessionid(sessionid);
  sprintf(msg, "%lu.%.3lu 102 INFO 0 SESSION %lu\n", akt_time.tv_sec, akt_time.tv_usec/1000, sessionid);
  queueMessage(msg);  
  return SRCP_OK;
}

int describeSESSION(int bus, int sessionid, char *reply)
{
  return SRCP_UNSUPPORTEDOPERATION;
}

int termSESSION(int bus, int sessionid, int termsessionid, char *reply)
{
  if(sessionid == termsessionid || termsessionid == 0)
  {
    return - SRCP_OK;
  }
  return SRCP_FORBIDDEN;
}
