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

#define QUEUELENGTH_INFO 1000

/* Kommandoqueues pro Bus */
static struct _INFO info_queue[QUEUELENGTH_INFO];            // info queue.
static pthread_mutex_t queue_mutex_info;
static int out, in;

/* internal functions */
static int queueLengthInfo(void)
  {
  if (in >= out)
    return in - out;
  else
    return QUEUELENGTH_INFO + in - out;
}

static int queueIsFullInfo(void)
{
  return queueLengthInfo() >= QUEUELENGTH_INFO - 1;
}


/* take changes from server */
int queueInfoGL(int bus, int addr, int dir, int speed, int maxspeed, int f,  int f1, int f2, int f3, int f4)
{
  while (queueIsFullInfo())
  {
    usleep(1000);
  }

  pthread_mutex_lock(&queue_mutex_info);

  info_queue[in].infoType  = INFO_GL;
  info_queue[in].bus       = bus;
  info_queue[in].speed     = speed;
  info_queue[in].maxspeed  = maxspeed;
  info_queue[in].direction = dir;
  info_queue[in].funcs     = f1 + (f2 << 1) + (f3 << 2) + (f4 << 3) + (f << 4);
  info_queue[in].id        = addr;
  in++;
  if (in == QUEUELENGTH_INFO)
    in = 0;

  pthread_mutex_unlock(&queue_mutex_info);
  return SRCP_OK;
}

int queueInfoGA(int bus, int addr, int port, int action)
{
  while (queueIsFullInfo())
  {
    usleep(1000);
  }

  pthread_mutex_lock(&queue_mutex_info);

  info_queue[in].infoType = INFO_GA;
  info_queue[in].bus      = bus;
  info_queue[in].id       = addr;
  info_queue[in].port     = port;
  info_queue[in].action   = action;
  in++;
  if (in == QUEUELENGTH_INFO)
    in = 0;

  pthread_mutex_unlock(&queue_mutex_info);
  return SRCP_OK;
}

int queueInfoFB(int bus, int port, int action)
{
  while (queueIsFullInfo())
  {
    usleep(1000);
  }

  pthread_mutex_lock(&queue_mutex_info);

  info_queue[in].infoType = INFO_FB;
  info_queue[in].bus      = bus;
  info_queue[in].id       = port;
  info_queue[in].action   = action;
  in++;
  if (in == QUEUELENGTH_INFO)
    in = 0;

  pthread_mutex_unlock(&queue_mutex_info);
  return SRCP_OK;
}

int queueInfoSM(int bus, int addr)
{
  while (queueIsFullInfo())
  {
    usleep(1000);
  }

  pthread_mutex_lock(&queue_mutex_info);

  info_queue[in].infoType = INFO_SM;
  info_queue[in].bus      = bus;
  info_queue[in].id       = addr;
  in++;
  if (in == QUEUELENGTH_INFO)
    in = 0;

  pthread_mutex_unlock(&queue_mutex_info);
  return SRCP_OK;
}

int queueIsEmptyInfo(void)
{
  return (in == out);
}

/** liefert nächsten Eintrag und >=0, oder -1 */
int getNextInfo(struct _INFO *info)
{
  if (in == out)
    return -1;
  *info = info_queue[out];
  return out;
}

/** liefert nächsten Eintrag oder -1, setzt fifo pointer neu! */
int unqueueNextInfo(struct _INFO *info)
{
  if (in == out)
    return -1;

  *info = info_queue[out];
  out++;
  if (out == QUEUELENGTH_INFO)
    out = 0;
  return out;
}

int startup_INFO(void)
{
  pthread_mutex_init(&queue_mutex_info, NULL);

  return SRCP_OK;
}

void sendInfos(int Socket, int sessionid)
{
  int error_code;
  char reply[1000];
  struct _INFO info_t;

  while (unqueueNextInfo(&info_t) != -1)
  {
    switch (info_t.infoType)
    {
      case INFO_GL:
        error_code = SRCP_INFO;
        sprintf(reply, "INFO %d GL %d\n", info_t.bus, info_t.id);
        break;
      case INFO_GA:
        error_code = SRCP_INFO;
        sprintf(reply, "INFO %d GA %d %d %d\n", info_t.bus, info_t.id, info_t.port, info_t.action);
        break;
      case INFO_FB:
        error_code = SRCP_INFO;
        sprintf(reply, "INFO %d FB %d %d\n", info_t.bus, info_t.id, info_t.action);
        break;
      case INFO_SM:
        error_code = SRCP_INFO;
        sprintf(reply, "INFO %d SM %d\n", info_t.bus, info_t.id);
        break;
      default:
        error_code = SRCP_NOTSUPPORTED;
        break;
    }
    socket_writereply(Socket, error_code, reply);
  }
}

int doInfoClient(int Socket, int sessionidid)
{
  while (1)
  {
    if (queueIsEmptyInfo())
      sendInfos(Socket, sessionidid);
    else
      usleep(10000);
  }
}

