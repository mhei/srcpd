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

#include "srcp-gl.h"
#include "srcp-ga.h"
#include "srcp-fb.h"
#include "srcp-info.h"
#include "srcp-error.h"
#include "config-srcpd.h"
#include "netserver.h"

#define QUEUELENGTH_INFO 1000

/* Kommandoqueues pro Bus */
static struct _INFO info_queue[QUEUELENGTH_INFO];            // info queue.
static pthread_mutex_t queue_mutex_info;
static pthread_mutex_t queue_mutex_client;
static pthread_t ttid_info;
static int out, in;

static int number_of_clients;               // number of registerated clients
static int max_clients;                     // number of clients, that can hold in field
static int *socketOfClients;
static int *sessionOfClients;

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

static int compareTime(struct timeval t1, struct timeval t2)
{
  int result;

  result = 0;
  if (t1.tv_sec < t2.tv_sec)
    result = 1;
  else
  {
    if (t1.tv_sec == t2.tv_sec)
    {
      if (t1.tv_usec < t2.tv_usec)
        result = 1;
    }
  }
  return result;
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
  max_clients = 0;

  return SRCP_OK;
}

// add a new client to list of info-clients
// if it is first client, start thread
static int addNewClient(int Socket, int sessionid)
{
  pthread_mutex_lock(&queue_mutex_client);
  if(number_of_clients == max_clients)
  {
    int max_new;
    int i;
    int *temp_sck;
    int *temp_sid;
    if(max_clients > 0)
    {
      max_new = max_clients + 5;
      temp_sck = malloc(max_new * sizeof(int));
      temp_sid = malloc(max_new * sizeof(int));
      for(i=0;i<max_clients;i++)
      {
        temp_sck[i] = socketOfClients[i];
        temp_sid[i] = sessionOfClients[i];
      }
      free(socketOfClients);
      free(sessionOfClients);
      socketOfClients = temp_sck;
      sessionOfClients = temp_sid;
      max_clients = max_new;
    }
    else
    {
      max_clients = 5;
      socketOfClients = malloc(max_clients * sizeof(int));
      sessionOfClients = malloc(max_clients * sizeof(int));
      if(pthread_create(&ttid_info, NULL, thr_doInfoClient, NULL))
        syslog(LOG_INFO, "cannot start Info Thread!");
      pthread_detach(ttid_info);
    }
  }
  socketOfClients[number_of_clients] = Socket;
  sessionOfClients[number_of_clients] = sessionid;
  number_of_clients++;
  pthread_mutex_unlock(&queue_mutex_client);

  return SRCP_OK;
}

// if a connection to a client is closed, remove this
// client from the list
// if it was last client, cancel thread
static int removeClient(int client)
{
  pthread_mutex_lock(&queue_mutex_client);
  if (number_of_clients == 1)
  {
    // remove thread
    number_of_clients = 0;
    max_clients = 0;
    socketOfClients = NULL;
    sessionOfClients = NULL;
    pthread_cancel(ttid_info);
  }
  else
  {
    // remove client
    int i;
    for(i=client;i<number_of_clients-1;i++)
    {
      socketOfClients[i] = socketOfClients[i+1];
      sessionOfClients[i] = sessionOfClients[i+1];
    }
    number_of_clients--;
  }
  pthread_mutex_unlock(&queue_mutex_client);
  return SRCP_OK;
}

static int generate_info_msg(struct _INFO *info_t, char *reply)
{
  int error_code = SRCP_INFO;
  switch (info_t->infoType)
  {
    case INFO_GL:
      sprintf(reply, "%ld.%ld 100 INFO %d GL %d", info_t->akt_time.tv_sec, info_t->akt_time.tv_usec/1000, info_t->bus, info_t->id);
      break;
    case INFO_GA:
      sprintf(reply, "%ld.%ld 100 INFO %d GA %d %d %d", info_t->akt_time.tv_sec, info_t->akt_time.tv_usec/1000, info_t->bus, info_t->id, info_t->port, info_t->action);
      break;
    case INFO_FB:
      sprintf(reply, "%ld.%ld 100 INFO %d FB %d %d", info_t->akt_time.tv_sec, info_t->akt_time.tv_usec/1000, info_t->bus, info_t->id, info_t->action);
      break;
    case INFO_SM:
      sprintf(reply, "%ld.%ld 100 INFO %d SM %d", info_t->akt_time.tv_sec, info_t->akt_time.tv_usec/1000, info_t->bus, info_t->id);
      break;
    default:
      error_code = SRCP_NOTSUPPORTED;
      break;
  }
  return error_code;
}

static void sendInfoS(int socket, char *reply)
{
  socket_writereply(socketOfClients[socket], SRCP_INFO, reply, NULL);
  reply[0] = '\0';
}

void sendInfos(int current)
{
  int error_code;
  int ctr;
  int i;
  int status;
  char reply[1000];
  char temp[80];
  struct _INFO info_t;

  // if -1 then send the message to all registrated clients
  if(current == -1)
  {
    ctr = 2000;
    while (unqueueNextInfo(&info_t) != -1)
    {
      error_code = generate_info_msg(&info_t, reply);
      for (i = 0; i < number_of_clients; i++)
      {
        status = socket_writereply(socketOfClients[i], error_code, reply, NULL);
        if (status < 0)
        {
          removeClient(i);
          i--;                  // disable "i++" for this time
                                // so we will not skip next client
        }
      }
      ctr--;
      if (ctr <= 0)
        break;        // check for new clients
    }
  }
  else
  {
    // send startup-infos to a new client
    struct timeval start_time;        // for comparsation
    struct timeval cmp_time;
    int busnumber;
    int value;
    int number;
    struct _GASTATE gatmp;
    struct _GLSTATE gltmp;

    reply[0] = '\0';

    for (busnumber = 0; busnumber < MAX_BUSSES; busnumber++)
    {

      // send all needed generic locomotivs
      gettimeofday(&start_time, NULL);
      number = get_number_gl(busnumber);
      for (i = 1; i <= number; i++)
      {
        if(!isInitializedGL(busnumber, i))
        {
          #warning complete me
          getGL(busnumber, i, &gltmp);
          if (compareTime(gltmp.tv, start_time))
          {
            sprintf(temp, "%ld.%ld 100 INFO %d GL %d %d %d %d %d %d %d %d\n",
              gltmp.tv.tv_sec, gltmp.tv.tv_usec/1000,
              busnumber, i,
              gltmp.direction, gltmp.speed, gltmp.maxspeed,
              0, 0, 0, 0);
            strcat(reply, temp);
            if (strlen(reply) > 900)
              sendInfoS(current, reply);
          }
        }
      }

      // send all needed generic assesoires
      gettimeofday(&start_time, NULL);
      number = get_number_ga(busnumber);
      for (i = 1; i <= number; i++)
      {
        if(!isInitializedGA(busnumber, i))
        {
          getGA(busnumber, i, &gatmp);
          value = gatmp.port;
          if (compareTime(gatmp.tv[value], start_time))
          {
            sprintf(temp, "%ld.%ld 100 INFO %d GA %d %d %d\n",
              gatmp.tv[value].tv_sec, gatmp.tv[value].tv_usec/1000,
              busnumber, i, value, gatmp.action);
            strcat(reply, temp);
            if (strlen(reply) > 900)
              sendInfoS(current, reply);
          }
        }
      }

      // send all needed feedbacks
      gettimeofday(&start_time, NULL);
      number = get_number_fb(busnumber);
      for (i = 1; i <= number; i++)
      {
        value = getFB(busnumber, i, &cmp_time);
        if (value != -1)
        {
          // time is modified
          if (compareTime(cmp_time, start_time))
          {
            // last change is before we startet
            if (value)
            {
              // last change isn't a reset
              sprintf(temp, "%ld.%ld 100 INFO %d FB %d %d\n", cmp_time.tv_sec, cmp_time.tv_usec/1000, busnumber, i, value);
              strcat(reply, temp);
              if (strlen(reply) > 900)
                sendInfoS(current, reply);
            }
          }
        }
      }
    }
    if (strlen(reply) > 0)
    {
      sendInfoS(current, reply);
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

  if(!addNewClient(Socket, sessionid))
  {
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
