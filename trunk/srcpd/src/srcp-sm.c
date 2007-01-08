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

#include "stdincludes.h"

#include "config-srcpd.h"
#include "srcp-error.h"
#include "srcp-info.h"
#include "srcp-session.h"
#include "srcp-sm.h"

#define QUEUELEN 2

/*  Important:
    - only NMRA is supportet at this time
    - set adress of decoder only availible at programming track
    - every GET/VERIFY only avalible at programming track
    - address = -1, means to use program-track
    - address > 0, means to use pom (only availible with CV)

    for instance:
      SET 1 SM -1 CV 29 2         - write 2 into CV 29 at program-track
      SET 1 SM 1210 CV 29 2       - write 2 into CV 29 of decoder with
                                    address 1210 on the main-line
      SET 1 SM -1 CVBIT 29 1 1    - set the 2-nd bit of CV 29 at program-track
      SET 1 SM -1 REG 5 2         - same as first, but using register-mode

    - the answer of GET is delivert via INFO-port!
*/


/* Kommandoqueues pro Bus */
static struct _SM queue[ MAX_BUSSES ][ QUEUELEN ];
static pthread_mutex_t queue_mutex[ MAX_BUSSES ];
static volatile int out[ MAX_BUSSES ], in[ MAX_BUSSES ];

/* internal functions */
static int queue_len( long int busnumber );
static int queue_isfull( long int busnumber );

int queueInfoSM( long int busnumber, int addr, int type, int typeaddr,
                 int bit, int value, int return_code,
                 struct timeval *akt_time )
{
  char buffer[ 1000 ], msg[ 1000 ];
  char tmp[ 100 ];

  if ( return_code == 0 )
  {
    sprintf( buffer, "%ld.%ld 100 INFO %ld SM %d",
             akt_time->tv_sec, akt_time->tv_usec / 1000,
             busnumber, addr );
    switch ( type )
    {
      case REGISTER:
        sprintf( tmp, "REG %d %d", typeaddr, value );
        break;
      case CV:
        sprintf( tmp, "CV %d %d", typeaddr, value );
        break;
      case CV_BIT:
        sprintf( tmp, "CVBIT %d %d %d", typeaddr, bit, value );
        break;
      case PAGE:
        sprintf( tmp, "PAGE %d %d", typeaddr, value );
        break;
    }
  }
  else
  {
    sprintf( buffer, "%ld.%ld 600 ERROR %ld SM %d %d",
             akt_time->tv_sec, akt_time->tv_usec / 1000,
             busnumber, addr, return_code );
    switch ( return_code )
    {
      case 0xF2:
        sprintf( tmp, "Cannot terminate task " );
        break;
      case 0xF3:
        sprintf( tmp, "No task to terminate" );
        break;
      case 0xF4:
        sprintf( tmp, "Task terminated" );
        break;
      case 0xF6:
        sprintf( tmp, "XPT_DCCQD: Not Ok (direkt bit read mode "
                 "is (probably) not supported)" );
        break;
      case 0xF7:
        sprintf( tmp, "XPT_DCCQD: Ok (direkt bit read mode is "
                 "(probably) supported)" );
        break;
      case 0xF8:
        sprintf( tmp, "Error during Selectrix read" );
        break;
      case 0xF9:
        sprintf( tmp, "No acknowledge to paged operation "
                 "(paged r/w not supported?)" );
        break;
      case 0xFA:
        sprintf( tmp, "Error during DCC direct bit mode operation" );
        break;
      case 0xFB:
        sprintf( tmp, "Generic Error" );
        break;
      case 0xFC:
        sprintf( tmp, "No decoder detected" );
        break;
      case 0xFD:
        sprintf( tmp, "Short! (on the PT)" );
        break;
      case 0xFE:
        sprintf( tmp, "No acknowledge from decoder (but a write "
                 "maybe was successful)" );
        break;
      case 0xFF:
        sprintf( tmp, "Timeout" );
        break;
    }
  }
  sprintf( msg, "%s %s\n", buffer, tmp );
  queueInfoMessage( msg );
  return SRCP_OK;
}

int get_number_sm( long int busnumber )
{
  return busses[ busnumber ].numberOfSM;
}

/* queue SM after some checks */
int queueSM( long int busnumber, int command, int type, int addr,
             int typeaddr, int bit, int value )
{
  struct timeval akt_time;
  DBG( busnumber, DBG_INFO, "queueSM for %i (in=%d out=%d)", addr,
       in[ busnumber ], out[ busnumber ] );
  // addr == -1 means using separate progrm-track
  // addr != -1 means programming on the main (only availible with CV)
  //if ( (addr == -1) || ((addr > 0) && (addr <= number_sm) && (type == CV)) )
  if ( queue_isfull( busnumber ) )
  {
    DBG( busnumber, DBG_DEBUG, "SM Queue is full" );
    return SRCP_TEMPORARILYPROHIBITED;
  }

  pthread_mutex_lock( &queue_mutex[ busnumber ] );

  queue[ busnumber ][ in[ busnumber ] ].bit = bit;
  queue[ busnumber ][ in[ busnumber ] ].type = type;
  queue[ busnumber ][ in[ busnumber ] ].value = value;
  queue[ busnumber ][ in[ busnumber ] ].typeaddr = typeaddr;
  queue[ busnumber ][ in[ busnumber ] ].command = command;
  gettimeofday( &akt_time, NULL );
  queue[ busnumber ][ in[ busnumber ] ].tv = akt_time;
  queue[ busnumber ][ in[ busnumber ] ].addr = addr;
  in[ busnumber ] ++;
  if ( in[ busnumber ] == QUEUELEN )
    in[ busnumber ] = 0;

  pthread_mutex_unlock( &queue_mutex[ busnumber ] );
  DBG( busnumber, DBG_DEBUG, "SM queued" );
  return SRCP_OK;
}

int queue_SM_isempty( long int busnumber )
{
  return ( in[ busnumber ] == out[ busnumber ] );
}

static int queue_len( long int busnumber )
{
  if ( in[ busnumber ] >= out[ busnumber ] )
    return in[ busnumber ] - out[ busnumber ];
  else
    return QUEUELEN + in[ busnumber ] - out[ busnumber ];
}

/* maybe, 1 element in the queue cannot be used.. */
static int queue_isfull( long int busnumber )
{
  return queue_len( busnumber ) >= QUEUELEN - 1;
}

/** return next entry with rc >=0, or return -1, if no more entries */
int getNextSM( long int busnumber, struct _SM *l )
{
  if ( in[ busnumber ] == out[ busnumber ] )
    return -1;
  *l = queue[ busnumber ][ out[ busnumber ] ];
  return out[ busnumber ];
}

/** return next entry or -1, set fifo pointer to new position! */
int unqueueNextSM( long int busnumber, struct _SM *l )
{
  if ( in[ busnumber ] == out[ busnumber ] )
    return -1;

  *l = queue[ busnumber ][ out[ busnumber ] ];
  out[ busnumber ] ++;
  if ( out[ busnumber ] == QUEUELEN )
    out[ busnumber ] = 0;
  return out[ busnumber ];
}

int setSM( long int busnumber, int type, int addr, int typeaddr, int bit,
           int value, int return_code )
{
  int number_sm = get_number_sm( busnumber );
  DBG( busnumber, DBG_DEBUG, "in setSM with number_sm=%i", number_sm );
  struct timeval tv;

  DBG( busnumber, DBG_DEBUG,
       "CV: %d         BIT: %d         VALUE: 0x%02x", typeaddr, bit,
       value );
  if ( ( addr == -1 )
       || ( ( addr > 0 ) && ( addr <= number_sm ) && ( type == CV ) ) )
  {
    gettimeofday( &tv, NULL );
    if ( type == CV_BIT )
      value = ( value & ( 1 << bit ) ) ? 1 : 0;
    queueInfoSM( busnumber, addr, type, typeaddr, bit, value,
                 return_code, &tv );
    return SRCP_OK;
  }
  else
  {
    return SRCP_NODATA;
  }
}

int infoSM( long int busnumber, int command, int type, int addr,
            int typeaddr, int bit, int value, char *info )
{
  int status, result;
  struct timeval now;

  DBG( busnumber, DBG_INFO, "TYPE: %d, CV: %d, BIT: %d, VALUE: 0x%02x",
       type, typeaddr, bit, value );
  session_preparewait( busnumber );
  status = queueSM( busnumber, command, type, addr, typeaddr, bit, value );

  if ( session_wait( busnumber, 90, &result ) == ETIMEDOUT )
  {
    gettimeofday( &now, NULL );
    sprintf( info, "%ld.%ld 417 ERROR timeout\n", now.tv_sec,
             now.tv_usec / 1000 );
  }
  else
  {
    gettimeofday( &now, NULL );
    sprintf( info, "%ld.%ld 100 INFO %ld SM %d CV %d %d\n", now.tv_sec,
             now.tv_usec / 1000, busnumber, addr, typeaddr, result );
  }
  session_cleanupwait( busnumber );
  return status;
}
