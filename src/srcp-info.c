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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <signal.h>
#include <syslog.h>
#include <sys/time.h>

#include "srcp-info.h"
#include "srcp-error.h"
#include "netserver.h"

int doInfoClient(int Socket, int sessionidid)
{
  char reply[1000];
  while (1)
  {
    strcpy(reply, "I'm alive");
    socket_writereply(Socket, SRCP_INFO, reply);
    sleep(1);
  }
}

