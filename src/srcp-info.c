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
#include "config-srcpd.h"
#include "netserver.h"

#define QUEUELENGTH_INFO 1000
#define MAX_CLIENTS      64

/* Kommandoqueues pro Bus */
static struct _INFO info_queue[QUEUELENGTH_INFO];            // info queue.
static pthread_mutex_t queue_mutex_info;
static pthread_mutex_t queue_mutex_client;
static int out, in;

static int number_of_clients;               // number of registerated clients
static int socketOfClients[MAX_CLIENTS];
static int sessionOfClients[MAX_CLIENTS];

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


// take changes from server
// we need changes only, if there is a receiver for our info-messages

int queueInfoGL(int bus, int addr, int dir, int speed, int maxspeed, int f,  int f1, int f2, int f3, int f4, struct timeval *akt_time)
{
  if (number_of_clients > 0)
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
    info_queue[in].akt_time  = *akt_time;
    in++;
    if (in == QUEUELENGTH_INFO)
      in = 0;

    pthread_mutex_unlock(&queue_mutex_info);
  }
  return SRCP_OK;
}

int queueInfoGA(int bus, int addr, int port, int action, struct timeval *akt_time)
{
  if (number_of_clients > 0)
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
    info_queue[in].akt_time = *akt_time;
    in++;
    if (in == QUEUELENGTH_INFO)
      in = 0;

    pthread_mutex_unlock(&queue_mutex_info);
  }
  return SRCP_OK;
}

int queueInfoFB(int bus, int port, int action, struct timeval *akt_time)
{
  if (number_of_clients > 0)
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
    info_queue[in].akt_time = *akt_time;
    in++;
    if (in == QUEUELENGTH_INFO)
      in = 0;

    pthread_mutex_unlock(&queue_mutex_info);
  }
  return SRCP_OK;
}

int queueInfoSM(int bus, int addr, struct timeval *akt_time)
{
  if (number_of_clients > 0)
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
  }
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
  pthread_mutex_init(&queue_mutex_client, NULL);

  number_of_clients = 0;

  return SRCP_OK;
}

static int generate_info_msg(struct _INFO *info_t, char *reply)
{
  int error_code = SRCP_INFO;
  switch (info_t->infoType)
  {
    case INFO_GL:
      sprintf(reply, "%d GL %d\n", info_t->bus, info_t->id);
      break;
    case INFO_GA:
      sprintf(reply, "%d GA %d %d %d\n", info_t->bus, info_t->id, info_t->port, info_t->action);
      break;
    case INFO_FB:
      sprintf(reply, "%d FB %d %d\n", info_t->bus, info_t->id, info_t->action);
      break;
    case INFO_SM:
      sprintf(reply, "%d SM %d\n", info_t->bus, info_t->id);
      break;
    default:
      error_code = SRCP_NOTSUPPORTED;
      break;
  }
  return error_code;
}

void sendInfos(int current)
{
  int error_code;
  int ctr;
  int i;
  char reply[1000];
  struct _INFO info_t;

  // if -1 then send the message to all registrated clients
  if(current == -1)
  {
    ctr = 2000;
    while (unqueueNextInfo(&info_t) != -1)
    {
      error_code = generate_info_msg(&info_t, reply);
      for (i = 0; i < number_of_clients; i++)
        socket_writereply(socketOfClients[i], error_code, reply, &(info_t.akt_time));
      ctr--;
      if (ctr <= 0)
        break;        // check for new clients
    }
  }
  else
  {
    // send startup-infos to a new client
    struct timeval start_time;        // for comparsation
    int busnumber;

    gettimeofday(&start_time, NULL);

    // send all needed generic locomotivs
    for (i = 0; i < MAXGLS; i++)
    {
    }

    // send all needed generic assesoires
    for (i = 0; i < MAXGAS; i++)
    {
    }

    for (busnumber = 0; busnumber < MAX_BUSSES - 1; busnumber++)
    {
      // send all needed feedbacks
      for (i = 0; i < MAXFBS; i++)
      {
      }
    }
  }
}


// startup-code for a new client
// 1. register client for accepting infos
// 2. sending all known things to client
//    (if not initialized or time less then start-time)
// - after starting, client will get new messages additional to
//   startup-messages
int doInfoClient(int Socket, int sessionid)
{
  int ret_code;

  if (number_of_clients < MAX_CLIENTS)
  {
    pthread_mutex_lock(&queue_mutex_client);
    socketOfClients[number_of_clients] = Socket;
    sessionOfClients[number_of_clients] = sessionid;
    number_of_clients++;
    pthread_mutex_unlock(&queue_mutex_client);
    sendInfos(number_of_clients - 1);
    ret_code = SRCP_OK;
  }
  else
  {
    ret_code = SRCP_TEMPORARILYPROHIBITED;
  }
  return ret_code;
}

// this thread is started on program-start
// every client will registrated and a message will be send to all
// registrated clients at once
void *thr_doInfoClient(void *v)
{
  while(1)
  {
    syslog(LOG_INFO, "I'm living with %d clients and %d entries in queue", number_of_clients, queueLengthInfo());
    if (number_of_clients == 0)
    {
      // no client is present, also wait a second
      sleep(1);
    }
    else
    {
      if (!queueIsEmptyInfo())
      {
        pthread_mutex_lock(&queue_mutex_client);
        sendInfos(-1);
        pthread_mutex_unlock(&queue_mutex_client);
      }
      else
        usleep(10000);

    }
  }
}
