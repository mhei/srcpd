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

#include <sys/time.h>

#include "srcp-gl.h"
#include "srcp-ga.h"
#include "srcp-fb.h"
#include "srcp-sm.h"
#include "srcp-info.h"
#include "srcp-error.h"
#include "config-srcpd.h"
#include "netserver.h"

#define QUEUELENGTH_INFO 1000

/* Kommandoqueues pro Bus */
static char info_queue[QUEUELENGTH_INFO][200];   // info queue.
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

int queueInfoGL(int busnumber, int addr, int dir, int speed, int maxspeed, int f,  int f1, int f2, int f3, int f4, struct timeval *akt_time)
{
  if (number_of_clients > 0)
  {
    while (queueIsFullInfo())
    {
      usleep(1000);
    }

    pthread_mutex_lock(&queue_mutex_info);

    sprintf(info_queue[in], "%ld.%ld 100 INFO %d GL %d %d %d %d %d %d %d %d %d\n",
      akt_time->tv_sec, akt_time->tv_usec/1000, busnumber, addr,
      dir, speed, maxspeed,
      f, f1, f2, f3, f4);
    DBG(busnumber, DBG_INFO, "data queued: %s", info_queue[in]);
    in++;
    if (in == QUEUELENGTH_INFO)
      in = 0;

    pthread_mutex_unlock(&queue_mutex_info);
  }
  return SRCP_OK;
}

int queueInfoGA(int busnumber, int addr, int port, int action, struct timeval *akt_time)
{
  if (number_of_clients > 0)
  {
    while (queueIsFullInfo())
    {
      usleep(1000);
    }

    pthread_mutex_lock(&queue_mutex_info);

    sprintf(info_queue[in], "%ld.%ld 100 INFO %d GA %d %d %d\n",
      akt_time->tv_sec, akt_time->tv_usec/1000,
      busnumber, addr, port, action);
    DBG(busnumber, DBG_INFO, "data queued: %s", info_queue[in]);
    in++;
    if (in == QUEUELENGTH_INFO)
      in = 0;

    pthread_mutex_unlock(&queue_mutex_info);
  }
  return SRCP_OK;
}

int queueInfoFB(int busnumber, int port, int action, struct timeval *akt_time)
{
//  syslog(LOG_INFO, "enter queueInfoFB");
  if (number_of_clients > 0)
  {
    while (queueIsFullInfo())
    {
      usleep(1000);
    }

    pthread_mutex_lock(&queue_mutex_info);

    sprintf(info_queue[in], "%ld.%ld 100 INFO %d FB %d %d\n",
      akt_time->tv_sec, akt_time->tv_usec/1000,
      busnumber, port, action);
      DBG(busnumber, DBG_INFO, "data queued: %s", info_queue[in]);
    in++;
    if (in == QUEUELENGTH_INFO)
      in = 0;

    pthread_mutex_unlock(&queue_mutex_info);
  }
  return SRCP_OK;
}

int queueInfoSM(int busnumber, int addr, int type, int typeaddr, int bit, int value, int return_code, struct timeval *akt_time)
{
  if (number_of_clients > 0)
  {
    char buffer[1000];
    char tmp[100];

    while (queueIsFullInfo())
    {
      usleep(1000);
    }

    
    pthread_mutex_lock(&queue_mutex_info);

    if (return_code == 0)
    {
      sprintf(buffer, "%ld.%ld 100 INFO %d SM %d",
        akt_time->tv_sec, akt_time->tv_usec/1000,
        busnumber, addr);
      switch (type)
      {
        case REGISTER:
          sprintf(tmp, "REG %d %d", typeaddr, value);
          break;
        case CV:
          sprintf(tmp, "CV %d %d", typeaddr, value);
          break;
        case CV_BIT:
          sprintf(tmp, "CVBIT %d %d %d", typeaddr, bit, value);
          break;
      }
    }
    else
    {
      sprintf(buffer, "%ld.%ld 600 ERROR %d SM %d",
        akt_time->tv_sec, akt_time->tv_usec/1000,
        busnumber, addr);
      switch (return_code)
      {
        case 0xF2:
          sprintf(tmp, "Cannot terminate task ");
          break;
        case 0xF3:
          sprintf(tmp, "No task to terminate");
          break;
        case 0xF4:
          sprintf(tmp, "Task terminated");
          break;
        case 0xF6:
          sprintf(tmp, "XPT_DCCQD: Not Ok (direkt bit read mode is (probably) not supported)");
          break;
        case 0xF7:
          sprintf(tmp, "XPT_DCCQD: Ok (direkt bit read mode is (probably) supported)");
          break;
        case 0xF8:
          sprintf(tmp, "Error during Selectrix read");
          break;
        case 0xF9:
          sprintf(tmp, "No acknowledge to paged operation (paged r/w not supported?)");
          break;
        case 0xFA:
          sprintf(tmp, "Error during DCC direct bit mode operation");
          break;
        case 0xFB:
          sprintf(tmp, "Generic Error");
          break;
        case 0xFC:
          sprintf(tmp, "No decoder detected");
          break;
        case 0xFD:
          sprintf(tmp, "Short! (on the PT)");
          break;
        case 0xFE:
          sprintf(tmp, "No acknowledge from decoder (but a write maybe was successful)");
          break;
        case 0xFF:
          sprintf(tmp, "Timeout");
          break;
      }
    }
    sprintf(info_queue[in], "%s %s\n", buffer, tmp);
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
int getNextInfo(char *info)
{
  if (in == out)
    return -1;
  pthread_mutex_lock(&queue_mutex_info);
  strcpy(info, info_queue[out]);
  pthread_mutex_unlock(&queue_mutex_info);
  return out;
}

/** liefert nächsten Eintrag oder -1, setzt fifo pointer neu! */
int unqueueNextInfo(char *info)
{
  if (in == out)
    return -1;

  pthread_mutex_lock(&queue_mutex_info);
  strcpy(info, info_queue[out]);
  out++;
  if (out == QUEUELENGTH_INFO)
    out = 0;
  pthread_mutex_unlock(&queue_mutex_info);
  return out;
}

int startup_INFO(void)
{
  pthread_mutex_init(&queue_mutex_info, NULL);
  pthread_mutex_init(&queue_mutex_client, NULL);

  number_of_clients = 0;
  max_clients = 0;
  out = 0;
  in = 0;

  return SRCP_OK;
}

// add a new client to list of info-clients
// if it is first client, start thread
static int addNewClient(int Socket, int sessionid)
{
  DBG(0, DBG_INFO, "addNewClient %ld", sessionid);
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
        DBG(0, DBG_ERROR, "cannot start Info Thread!");
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
  DBG(0, DBG_DEBUG, "removeClient %ld: %ld %ld", client, socketOfClients[client], sessionOfClients[client]);
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

void sendInfos(int current)
{
  int ctr;
  int i;
  int status;
  char reply[1000];
  char temp[200];

  // if -1 then send the message to all registrated clients
  if(current == -1)
  {
    ctr = 50;
    reply[0] = '\0';
    while (unqueueNextInfo(temp) != -1)
    {
      strcat(reply, temp);
      DBG(0, DBG_DEBUG, "reply-length = %d", strlen(reply));
      if(strlen(reply) > 900)
      {
        for (i = 0; i < number_of_clients; i++)
        {
	    DBG(0, DBG_DEBUG, "send infos to client %d", i);
          status = write(socketOfClients[i], reply, strlen(reply));
          if (status < 0)
          {
            removeClient(i);
            i--;                  // disable "i++" for this time
                                  // so we will not skip next client
          }
        }
        reply[0] = '\0';
      }
      ctr--;
      if (ctr <= 0)
        break;        // check for new clients
    }
    DBG(0, DBG_DEBUG, "reply-length after loop = %d", strlen(reply));
    if(strlen(reply) > 0)
    {
      for (i = 0; i < number_of_clients; i++)
      {
        DBG(0, DBG_DEBUG, "write infos to client %d at socket %d", i, socketOfClients[i]);
        status = write(socketOfClients[i], reply, strlen(reply));
        if (status < 0)
        {
          removeClient(i);
          i--;                  // disable "i++" for this time
                                // so we will not skip next client
        }
      }
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

    reply[0] = '\0';

    for (busnumber = 0; busnumber < MAX_BUSSES; busnumber++)
    {

      DBG(busnumber, DBG_DEBUG, "send all data for busnumber %d to new client", busnumber);
      // send all needed generic locomotivs
      gettimeofday(&start_time, NULL);
      number = get_number_gl(busnumber);
      DBG(busnumber, DBG_DEBUG, "send all (max. %d) locomotivs from busnumber %d to new client", number, busnumber);
      for (i = 1; i <= number; i++)
      {
        if(isInitializedGL(busnumber, i))
        {
            infoGL(busnumber, i, reply);
            if (strlen(reply) > 0)
            {
              write(socketOfClients[current], reply, strlen(reply));
              reply[0] = '\0';
            }
        }
      }

      // send all needed generic assesoires
      gettimeofday(&start_time, NULL);
      number = get_number_ga(busnumber);
      DBG(busnumber, DBG_DEBUG,  "send all (max. %d) assesoirs from busnumber %d to new client", number, busnumber);
      for (i = 1; i <= number; i++)
      {
        if(isInitializedGA(busnumber, i))
        {
          DBG(busnumber, DBG_DEBUG, "GA initialized: %d", i);
          infoGA(busnumber, i, 0, reply);
          if (strlen(reply) > 0)
            write(socketOfClients[current], reply, strlen(reply));
          infoGA(busnumber, i, 1, reply);
          if (strlen(reply) > 0)
            write(socketOfClients[current], reply, strlen(reply));

          reply[0] = '\0';
        }
      }

      // send all needed feedbacks
      gettimeofday(&start_time, NULL);
      number = get_number_fb(busnumber);
      DBG(busnumber, DBG_DEBUG,  "send all feedbacks from busnumber %d to new client", busnumber);
      for (i = 1; i <= number; i++)
      {
        int rc = getFB(busnumber, i, &cmp_time, &value);
        if (rc != SRCP_OK)
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
              {
                write(socketOfClients[current], reply, strlen(reply));
                reply[0] = '\0';
	      }
            }
          }
        }
      }
    }
    if (strlen(reply) > 0)
    {
      write(socketOfClients[current], reply, strlen(reply));
    }
    DBG(0, DBG_DEBUG,  "all datas to new Info-Client sended");
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

  DBG(0, DBG_DEBUG, "new Info-client detected %ld", sessionid);
  if(addNewClient(Socket, sessionid) == SRCP_OK)
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
        usleep(100000);
    }
  }
}
