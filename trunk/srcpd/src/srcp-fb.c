
/*
 * Vorliegende Software unterliegt der General Public License,
 * Version 2, 1991. (c) Matthias Trute, 2000-2001.
 *
 */

#include "stdincludes.h"

#include "config-srcpd.h"
#include "srcp-error.h"
#include "srcp-info.h"

#include "srcp-fb.h"

/* one array for all busses             */
/* not visible outside of this module   */
static struct _FB fb[ MAX_BUSSES ];
static int min_time[ MAX_BUSSES ];

#define QUEUELENGTH_FB 1000

static struct _RESET_FB reset_queue[ MAX_BUSSES ][ QUEUELENGTH_FB ];   // reset fb queue.
static pthread_mutex_t queue_mutex_fb, queue_mutex_reset[ MAX_BUSSES ];
static int out[ MAX_BUSSES ], in[ MAX_BUSSES ];


/* internal functions */

static int queueInfoFB( int busnumber, int port )
{
  char msg[ 1000 ];
  infoFB( busnumber, port, msg );
  queueInfoMessage( msg );
  return SRCP_OK;
}

static int queueLengthFB( int busnumber )
{
  if ( in[ busnumber ] >= out[ busnumber ] )
    return in[ busnumber ] - out[ busnumber ];
  else
    return QUEUELENGTH_FB + in[ busnumber ] - out[ busnumber ];
}

static int queueIsFullFB( int busnumber )
{
  return queueLengthFB( busnumber ) >= QUEUELENGTH_FB - 1;
}

int queueIsEmptyFB( busnumber )
{
  return ( in[ busnumber ] == out[ busnumber ] );
}

/** liefert n�hsten Eintrag und >=0, oder -1 */
static int getNextFB( int busnumber, struct _RESET_FB *info )
{
  if ( in[ busnumber ] == out[ busnumber ] )
    return -1;
  pthread_mutex_lock( &queue_mutex_reset[ busnumber ] );
  info->port = reset_queue[ busnumber ][ out[ busnumber ] ].port;
  info->timestamp = reset_queue[ busnumber ][ out[ busnumber ] ].timestamp;
  pthread_mutex_unlock( &queue_mutex_reset[ busnumber ] );
  return out[ busnumber ];
}

/** liefert n�hsten Eintrag oder -1, setzt fifo pointer neu! */
static void unqueueNextFB( int busnumber )
{
  pthread_mutex_lock( &queue_mutex_reset[ busnumber ] );
  out[ busnumber ] ++;
  if ( out[ busnumber ] == QUEUELENGTH_FB )
    out[ busnumber ] = 0;
  pthread_mutex_unlock( &queue_mutex_reset[ busnumber ] );
}

static void queue_reset_fb( int busnumber, int port, struct timeval *ctime )
{
  while ( queueIsFullFB( busnumber ) )
  {
    usleep( 1000 );
  }

  pthread_mutex_lock( &queue_mutex_reset[ busnumber ] );

  reset_queue[ busnumber ][ in[ busnumber ] ].port = port;
  reset_queue[ busnumber ][ in[ busnumber ] ].timestamp = *ctime;
  in[ busnumber ] ++;
  if ( in[ busnumber ] == QUEUELENGTH_FB )
    in[ busnumber ] = 0;

  pthread_mutex_unlock( &queue_mutex_reset[ busnumber ] );
}

int getFB( int bus, int port, struct timeval *time, int *value )
{
  port--;
  if ( get_number_fb( bus ) <= 0 || ( port < 0 ) || ( port >= get_number_fb( bus ) ) )
    return SRCP_NODATA;

  *value = fb[ bus ].fbstate[ port ].state;
  if ( time != NULL )
    *time = fb[ bus ].fbstate[ port ].timestamp;
  return SRCP_OK;
}

void updateFB( int bus, int port, int value )
{
  struct timezone dummy;
  struct timeval akt_time;

  int port_t;

  // check range to disallow segmentation fault
  if ( ( port > get_number_fb( bus ) ) || ( port < 1 ) )
    return ;

  port_t = port - 1;
  // we read 8 or 16 ports at once, but we will only change those ports,
  // which are really changed
  //
  // if changed contact ist resetet, we will wait 'min_time[bus]' seconds to
  // minimalize problems from contacts bitween track and train
  if ( fb[ bus ].fbstate[ port_t ].state != value )
  {
    DBG( bus, DBG_DEBUG, "FB %02i/%03i  was gone to \"%i\"", bus, port, value );

    gettimeofday( &akt_time, &dummy );
    if ( value == 0 )
    {
      DBG( bus, DBG_DEBUG, "FB %02i/%03i  was gone to 0", bus, port );
      if ( min_time[ bus ] == 0 )
      {
        fb[ bus ].fbstate[ port_t ].state = value;
        fb[ bus ].fbstate[ port_t ].timestamp = akt_time;
        fb[ bus ].fbstate[ port_t ].change = 0;
        queueInfoFB( bus, port );
      }
      else
      {
        pthread_mutex_lock( &queue_mutex_fb );
        fb[ bus ].fbstate[ port_t ].change = -1;
        pthread_mutex_unlock( &queue_mutex_fb );
        queue_reset_fb( bus, port_t, &akt_time );
      }
    }
    else
    {
      if ( fb[ bus ].fbstate[ port_t ].change < 0 )
      {
        fb[ bus ].fbstate[ port_t ].change = 0;
      }
      else
      {
        pthread_mutex_lock( &queue_mutex_fb );
        fb[ bus ].fbstate[ port_t ].state = value;
        fb[ bus ].fbstate[ port_t ].timestamp = akt_time;
        fb[ bus ].fbstate[ port_t ].change = 0;
        pthread_mutex_unlock( &queue_mutex_fb );
        // queue changes for writing info-message
        queueInfoFB( bus, port );
      }
    }
  }
}

/* all moduls with 8 or 16 ports */
int setFBmodul( int bus, int modul, int values )
{
  int i;
  int c;
  int fb_contact;
  int ports;
  int dir;
  int mask;

  ports = ( ( busses[ bus ].flags & FB_16_PORTS ) == FB_16_PORTS ) ? 16 : 8;
  if ( busses[ bus ].flags & FB_4_PORTS )
    ports = 4;
  if ( ( busses[ bus ].flags & FB_ORDER_0 ) == FB_ORDER_0 )
  {
    dir = 0;
    mask = 1;
  }
  else
  {
    dir = 1;
    mask = 1 << ( ports - 1 );
  }
  // compute startcontact ( (modul - 1) * ports + 1 )
  fb_contact = modul - 1;
  fb_contact *= ports;
  fb_contact++;

  for ( i = 0; i < ports; i++ )
  {
    c = ( values & mask ) ? 1 : 0;
    updateFB( bus, fb_contact++, c );
    if ( dir )
      mask >>= 1;
    else
      mask <<= 1;
  }
  return SRCP_OK;
}

int infoFB( int bus, int port, char *msg )
{
  struct timeval time;
  int state;
  int rc = getFB( bus, port, &time, &state );
  msg[ 0 ] = 0x00;
  if ( rc >= SRCP_OK )
  {
    sprintf( msg, "%lu.%.3lu 100 INFO %d FB %d %d \n",
             time.tv_sec, time.tv_usec / 1000, bus, port, state );
    return SRCP_INFO;
  }
  else
  {
    return SRCP_NODATA;
  }
}

int describeFB( int bus, int addr, char *reply )
{
  return SRCP_UNSUPPORTEDOPERATION;
}

int startup_FB()
{
  int i;
  for ( i = 0;i < MAX_BUSSES;i++ )
  {
    fb[ i ].numberOfFb = 0;
    fb[ i ].fbstate = NULL;
    out[ i ] = 0;
    in[ i ] = 0;
    min_time[ i ] = 0;
  }
  return 0;
}

void set_min_time( int busnumber, int mt )
{
  if ( ( busnumber >= 0 ) && ( busnumber < MAX_BUSSES ) )
    min_time[ busnumber ] = mt * 1000;
}

int init_FB( int bus, int number )
{
  struct timeval akt_time;
  int i;

  if ( bus >= MAX_BUSSES )
    return 1;

  if ( number > 0 )
  {
    gettimeofday( &akt_time, NULL );

    fb[ bus ].fbstate = malloc( number * sizeof( struct _FBSTATE ) );
    if ( fb[ bus ].fbstate == NULL )
      return 1;
    fb[ bus ].numberOfFb = number;
    for ( i = 0;i < number;i++ )
    {
      fb[ bus ].fbstate[ i ].state = 0;
      fb[ bus ].fbstate[ i ].change = 0;
      fb[ bus ].fbstate[ i ].timestamp = akt_time;
    }
  }
  return 0;
}

int get_number_fb( int bus )
{
  return fb[ bus ].numberOfFb;
}

void check_reset_fb( int busnumber )
{
  struct _RESET_FB reset_fb;
  struct timeval cmp_time, diff_time;

  while ( getNextFB( busnumber, &reset_fb ) != -1 )
  {
    if ( fb[ busnumber ].fbstate[ reset_fb.port ].change == 0 )
    {
      // drop this reset of feedback, because we've got an new impulse
      unqueueNextFB( busnumber );
    }
    else
    {
      gettimeofday( &cmp_time, NULL );
      DBG( busnumber, DBG_DEBUG, "FB %02i/%03i  time: %ld.%ld      compare: %ld.%ld",
           busnumber, reset_fb.port, cmp_time.tv_sec, cmp_time.tv_usec,
           reset_fb.timestamp.tv_sec, reset_fb.timestamp.tv_usec );
      diff_time.tv_sec = cmp_time.tv_sec - reset_fb.timestamp.tv_sec;
      diff_time.tv_usec = cmp_time.tv_usec - reset_fb.timestamp.tv_usec;
      diff_time.tv_usec += ( diff_time.tv_sec * 1000000 );
      DBG( busnumber, DBG_DEBUG, "FB %02i/%03i  time-diff = %ld us (need %d us)",
           busnumber, reset_fb.port, diff_time.tv_usec, min_time[ busnumber ] );
      if ( diff_time.tv_usec < min_time[ busnumber ] )
      {
        break;
      }
      else
      {
        DBG( busnumber, DBG_DEBUG, "set %d feedback to 0", reset_fb.port );
        unqueueNextFB( busnumber );
        pthread_mutex_lock( &queue_mutex_fb );
        fb[ busnumber ].fbstate[ reset_fb.port ].state = 0;
        fb[ busnumber ].fbstate[ reset_fb.port ].timestamp = reset_fb.timestamp;
        fb[ busnumber ].fbstate[ reset_fb.port ].change = 0;
        pthread_mutex_unlock( &queue_mutex_fb );
        queueInfoFB( busnumber, reset_fb.port + 1 );
      }
    }
  }
}