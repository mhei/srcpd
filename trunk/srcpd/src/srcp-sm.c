/***************************************************************************
                          srcp-sm.c  -  description
                             -------------------
    begin                : Mon Aug 12 2002
    copyright            : (C) 2002 by Dipl.-Ing. Frank Schmischke
    email                : frank.schmischke@t-online.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <pthread.h>
#include <stdio.h>
#include <syslog.h>
#include <unistd.h>

#include "config-srcpd.h"
#include "srcp-error.h"
#include "srcp-info.h"
#include "srcp-sm.h"

#define QUEUELEN 2

/*  Important:
    - only NMRA is supportet at this time
    - set adress of decoder only availible at programming track
    - every GET/VERIFY only avalible at programming track
    - address = -1, means to use program-track
    - address > 0, means to use pom (only availible with CV)

    for instance:
      SET 1 SM -1 CV 29 2             - write 2 into CV 29 at program-track
      SET 1 SM 1210 CV 29 2           - write 2 into CV 29 of decoder with adress 1210 on the main-line
      SET 1 SM -1 CVBIT 29 1 1        - set the 2-nd bit of CV 29 at program-track
      SET 1 SM -1 REG 5 2             - same as first, but using register-mode

    - the answer of GET is delivert via INFO-port !!!
*/


/* Kommandoqueues pro Bus */
static struct _SM queue[MAX_BUSSES][QUEUELEN];  // Kommandoqueue
static pthread_mutex_t queue_mutex[MAX_BUSSES];
static int out[MAX_BUSSES], in[MAX_BUSSES];

/* internal functions */
static int queue_len(int busnumber);
static int queue_isfull(int busnumber);


int get_number_sm (int busnumber)
{
  return busses[busnumber].numberOfSM;
}

// queue SM after some checks
int queueSM(int busnumber, int command, int type, int addr, int typeaddr, int bit, int value)
{
  struct timeval akt_time;
  int number_sm = get_number_sm(busnumber);
  syslog(LOG_INFO, "queueSM für %i", addr);
  // addr == -1 means using separate progrm-track
  // addr != -1 means programming on the main (only availible with CV)
  if ( (addr == -1) || ((addr > 0) && (addr <= number_sm) && (type == CV)) )
  {
    while (queue_isfull(busnumber))
    {
      usleep(1000);
    }

    pthread_mutex_lock(&queue_mutex[busnumber]);

    queue[busnumber][in[busnumber]].bit        = bit;
    queue[busnumber][in[busnumber]].type       = type;
    queue[busnumber][in[busnumber]].value      = value;
    queue[busnumber][in[busnumber]].typeaddr   = typeaddr;
    queue[busnumber][in[busnumber]].command    = command;
    gettimeofday(&akt_time, NULL);
    queue[busnumber][in[busnumber]].tv         = akt_time;
    queue[busnumber][in[busnumber]].addr       = addr;
    in[busnumber]++;
    if (in[busnumber] == QUEUELEN)
      in[busnumber] = 0;

    pthread_mutex_unlock(&queue_mutex[busnumber]);
  }
  else
  {
    return SRCP_WRONGVALUE;
  }
  return SRCP_OK;
}

int queue_SM_isempty(int busnumber)
{
  return (in[busnumber] == out[busnumber]);
}

static int queue_len(int busnumber)
{
  if (in[busnumber] >= out[busnumber])
    return in[busnumber] - out[busnumber];
  else
    return QUEUELEN + in[busnumber] - out[busnumber];
}

/* maybe, 1 element in the queue cannot be used.. */
static int queue_isfull(int busnumber)
{
  return queue_len(busnumber) >= QUEUELEN - 1;
}

/** liefert nächsten Eintrag und >=0, oder -1 */
int getNextSM(int busnumber, struct _SM *l)
{
  if (in[busnumber] == out[busnumber])
    return -1;
  *l = queue[busnumber][out[busnumber]];
  return out[busnumber];
}

/** liefert nächsten Eintrag oder -1, setzt fifo pointer neu! */
int unqueueNextSM(int busnumber, struct _SM *l)
{
  if (in[busnumber] == out[busnumber])
    return -1;

  *l = queue[busnumber][out[busnumber]];
  out[busnumber]++;
  if (out[busnumber] == QUEUELEN)
    out[busnumber] = 0;
  return out[busnumber];
}

int setSM(int busnumber, int type, int addr, int typeaddr, int bit, int value, int return_code)
{
  int number_sm = get_number_sm(busnumber);
  struct timeval tv;
  if(number_sm == 0)
    return SRCP_UNSUPPORTEDDEVICEGROUP;

  if ( (addr == -1) || ((addr > 0) && (addr <= number_sm) && (type == CV)) )
  {
    gettimeofday(&tv, NULL);
    queueInfoSM(busnumber, addr, type, typeaddr, bit, value, return_code, &tv);
    return SRCP_OK;
  }
  else
  {
    return SRCP_NODATA;
  }
}

int infoSM(int busnumber, int command, int type, int addr, int typeaddr, int bit, int value, char* info)
{
  int status;
  if (busses[busnumber].debuglevel > 4)
    syslog(LOG_INFO, "CV: %d         BIT: %d         VALUE: 0x%02x", typeaddr, bit ,value);
  status = queueSM(busnumber, command, type, addr, typeaddr, bit, value);

  sprintf(info, "       >> currently under construction <<");

  return status;
}
