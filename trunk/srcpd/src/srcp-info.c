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
#include "stdincludes.h"

#include "srcp-gl.h"
#include "srcp-ga.h"
#include "srcp-fb.h"
#include "srcp-sm.h"
#include "srcp-power.h"
#include "srcp-info.h"
#include "srcp-error.h"
#include "srcp-descr.h"
#include "config-srcpd.h"
#include "netserver.h"

#define QUEUELENGTH_INFO 1000

/* Kommandoqueues pro Bus */
static char info_queue[QUEUELENGTH_INFO][200];   // info queue.
static pthread_mutex_t queue_mutex_info;
static pthread_mutex_t queue_mutex_client;
static int  in=0;

static int number_of_clients = 0;               // number of registerated clients
static int max_clients = 0;                     // number of clients, that can hold in field


// take changes from server
// we need changes only, if there is a receiver for our info-messages

/* queue a pre-formatted message */
int queueMessage(char *msg) {
    pthread_mutex_lock(&queue_mutex_info);
    sprintf(info_queue[in], "%s", msg);
    in++;
    if (in == QUEUELENGTH_INFO)
      in = 0;
    pthread_mutex_unlock(&queue_mutex_info);
  return SRCP_OK;
}

int queueInfoGL(int busnumber, int addr, int dir, int speed, int maxspeed, int f,  int f1, int f2, int f3, int f4, struct timeval *akt_time)
{
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
  return SRCP_OK;
}

int queueInfoGA(int busnumber, int addr, int port, int action, struct timeval *akt_time)
{
    pthread_mutex_lock(&queue_mutex_info);

    sprintf(info_queue[in], "%ld.%ld 100 INFO %d GA %d %d %d\n",
      akt_time->tv_sec, akt_time->tv_usec/1000,
      busnumber, addr, port, action);
    DBG(busnumber, DBG_INFO, "data queued: %s", info_queue[in]);
    in++;
    if (in == QUEUELENGTH_INFO)
      in = 0;

    pthread_mutex_unlock(&queue_mutex_info);
  return SRCP_OK;
}

int queueInfoFB(int busnumber, int port, int action, struct timeval *akt_time)
{
    pthread_mutex_lock(&queue_mutex_info);

    sprintf(info_queue[in], "%ld.%ld 100 INFO %d FB %d %d\n",
      akt_time->tv_sec, akt_time->tv_usec/1000,
      busnumber, port, action);
      DBG(busnumber, DBG_INFO, "data queued: %s", info_queue[in]);
    in++;
    if (in == QUEUELENGTH_INFO)
      in = 0;

    pthread_mutex_unlock(&queue_mutex_info);
  return SRCP_OK;
}

int queueInfoSM(int busnumber, int addr, int type, int typeaddr, int bit, int value, int return_code, struct timeval *akt_time)
{
    char buffer[1000];
    char tmp[100];

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
  return SRCP_OK;
}

int queueIsEmptyInfo(int current)
{
  return (in == current);
}

/** liefert nächsten Eintrag oder -1, setzt fifo pointer neu! */
int unqueueNextInfo(int current, char *info)
{
  if (in == current)
    return -1;

  pthread_mutex_lock(&queue_mutex_info);
  strcpy(info, info_queue[current++]);
  if (current == QUEUELENGTH_INFO)
    current = 0;
  pthread_mutex_unlock(&queue_mutex_info);
  return current;
}

int startup_INFO(void)
{
  pthread_mutex_init(&queue_mutex_info, NULL);
  pthread_mutex_init(&queue_mutex_client, NULL);

  number_of_clients = 0;
  max_clients = 0;
  in = 0;

  return SRCP_OK;
}


/**
  * Endless loop for new infomode client
  * terminates on write failure
  */

int doInfoClient(int Socket, int sessionid)
{
  int status, i, current;
  char reply[1000];
  
    // send startup-infos to a new client
    struct timeval start_time;        // for comparsation
    struct timeval cmp_time;
    int busnumber;
    int value;
    int number;

    reply[0] = '\0';

    DBG(0, DBG_DEBUG, "new Info-client requested %ld", sessionid);
    for (busnumber = 0; busnumber <= num_busses; busnumber++)
    {
      DBG(busnumber, DBG_DEBUG, "send all data for busnumber %d to new client", busnumber);
      // first some global bus data
      infoPower(busnumber, reply);
      if (strlen(reply) > 0) {
              write(Socket, reply, strlen(reply));
      }
      reply[0] = '\0';
      // send Descriptions for busses
      describeBus(busnumber, reply);
      if (strlen(reply) > 0) {
              write(Socket, reply, strlen(reply));
      }
      reply[0] = '\0';

      // send all needed generic locomotivs
      gettimeofday(&start_time, NULL);
      number = get_number_gl(busnumber);
      DBG(busnumber, DBG_DEBUG, "send all (max. %d) locomotivs from busnumber %d to new client", number, busnumber);
      for (i = 1; i <= number; i++)
      {
        if(isInitializedGL(busnumber, i))
        {
            describeGL(busnumber, i, reply);
            if (strlen(reply) > 0) {
              write(Socket, reply, strlen(reply));
            }
            reply[0] = '\0';

            infoGL(busnumber, i, reply);
            if (strlen(reply) > 0) {
              write(Socket, reply, strlen(reply));
            }
            reply[0] = '\0';
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
            describeGA(busnumber, i, reply);
            if (strlen(reply) > 0) {
              write(Socket, reply, strlen(reply));
            }
            reply[0] = '\0';

          infoGA(busnumber, i, 0, reply);
          if (strlen(reply) > 0)
            write(Socket, reply, strlen(reply));
          infoGA(busnumber, i, 1, reply);
          if (strlen(reply) > 0) {
            write(Socket, reply, strlen(reply));
          }
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
        if (rc == SRCP_OK && value!=0) {
              infoFB(busnumber, i, reply);
              if(strlen(reply)>0) {
                write(Socket, reply, strlen(reply));
              }
              reply[0] = '\0';
        }
      }
   }
   DBG(0, DBG_DEBUG,  "all data to new Info-Client (%ld) sent", sessionid);
   current = in;
   while (1==1)   {
      while(queueIsEmptyInfo(current)) usleep(1000);
      current = unqueueNextInfo(current, reply);
      DBG(0, DBG_DEBUG, "reply-length = %d", strlen(reply));
      status = write(Socket, reply, strlen(reply));
      if (status < 0) {
         break;
      }
   }
   return 0;
}
