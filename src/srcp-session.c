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

#include <syslog.h>

#include "srcp-session.h"
#include "srcp-ga.h"
#include "srcp-gl.h"
#include "srcp-error.h"

int startup_SESSION(void)
{
  return 0;
}

int start_session(long int sessionid, int mode)
{
  syslog(LOG_INFO, "Session started; clientid %ld, mode %d", sessionid, mode);
  return SRCP_OK;
}

int stop_session(long int sessionid)
{
  syslog(LOG_INFO, "Session terminated clientid %ld", sessionid);
  // clean all locks
  unlock_ga_bysessionid(sessionid);
  unlock_gl_bysessionid(sessionid);
  return SRCP_OK;
}

int describeSESSION(int bus, int sessionid, char *reply)
{
  return SRCP_NOTSUPPORTED;
}

int termSESSION(int bus, int sessionid, int termsessionid, char *reply)
{
  if(sessionid == termsessionid || termsessionid == 0)
  {
    return - SRCP_OK;
  }
  return SRCP_NOTSUPPORTED;
}
