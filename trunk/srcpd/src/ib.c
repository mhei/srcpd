/***************************************************************************
                      ib.c  -  description
                       -------------------
begin                : Don Apr 19 17:35:13 MEST 2001
copyright            : (C) 2001 by Dipl.-Ing. Frank Schmischke
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

/***************************************************************************
 *                                                                         *
 *  Changes:                                                               *
 *                                                                         *
 *  17.01.2002 Frank Schmischke                                            *
 *   - using of kernelmodul/-device "ibox"                                 *
 *                                                                         *
 ***************************************************************************/

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#ifdef linux
#include <linux/serial.h>
#include <sys/io.h>
#endif

#include "config-srcpd.h"
#include "ib.h"
#include "io.h"
#include "srcp-ga.h"
#include "srcp-gl.h"
#include "srcp-sm.h"
#include "srcp-time.h"
#include "srcp-fb.h"
#include "srcp-power.h"
#include "srcp-info.h"
#include "srcp-error.h"
#include "srcp-session.h"
#include "syslogmessage.h"
#include "ttycygwin.h"

#define __ib ((IB_DATA*)buses[busnumber].driverdata)
#define __ibt ((IB_DATA*)buses[btd->bus].driverdata)

static int init_lineIB( bus_t busnumber );

/* IB helper functions */
static int sendBreak( const int fd,  bus_t busnumber );
static int switchOffP50Command( const  bus_t busnumber );
static int readAnswer_IB( const  bus_t busnumber,
                          const int generatePrintf );
static int readByte_IB(  bus_t bus, int wait, unsigned char *the_byte );
static speed_t checkBaudrate( const int fd, const  bus_t busnumber );
static int resetBaudrate( const speed_t speed, const bus_t busnumber );

int readConfig_IB( xmlDocPtr doc, xmlNodePtr node, bus_t busnumber )
{
  syslog_bus( busnumber, DBG_INFO, "Reading configuration for bus '%s'",
          node->name);

  buses[ busnumber ].driverdata = malloc( sizeof( struct _IB_DATA ) );

    if (buses[busnumber].driverdata == NULL) {
        syslog_bus(busnumber, DBG_ERROR,
                "Memory allocation error in module '%s'.", node->name);
        return 0;
    }

  buses[ busnumber ].type = SERVER_IB;
  buses[ busnumber ].init_func = &init_bus_IB;
  buses[ busnumber ].thr_func = &thr_sendrec_IB;
  buses[ busnumber ].init_gl_func = &init_gl_IB;
  buses[ busnumber ].init_ga_func = &init_ga_IB;
  buses[ busnumber ].flags |= FB_16_PORTS;
  buses[ busnumber ].device.file.baudrate = B38400;

  strcpy( buses[ busnumber ].description,
          "GA GL FB SM POWER LOCK DESCRIPTION" );
  __ib->number_fb = 0; /* max. 31 for S88; Loconet is missing this time */
  __ib->number_ga = 256;
  __ib->number_gl = 80;
  __ib->pause_between_cmd = 250;

  xmlNodePtr child = node->children;
  xmlChar *txt = NULL;

  while ( child != NULL )
  {
    if (xmlStrncmp(child->name, BAD_CAST "text", 4) == 0) {
          /* just do nothing, it is only a comment */
    }

    else if ( xmlStrcmp( child->name, BAD_CAST "number_fb" ) == 0 )
    {
      txt = xmlNodeListGetString( doc, child->xmlChildrenNode, 1 );
      if ( txt != NULL )
      {
        __ib->number_fb = atoi( ( char * ) txt );
        xmlFree( txt );
      }
    }

    else if ( xmlStrcmp( child->name, BAD_CAST "number_gl" ) == 0 )
    {
      txt = xmlNodeListGetString( doc, child->xmlChildrenNode, 1 );
      if ( txt != NULL )
      {
        __ib->number_gl = atoi( ( char * ) txt );
        xmlFree( txt );
      }
    }

    else if ( xmlStrcmp( child->name, BAD_CAST "number_ga" ) == 0 )
    {
      txt = xmlNodeListGetString( doc, child->xmlChildrenNode, 1 );
      if ( txt != NULL )
      {
        __ib->number_ga = atoi( ( char * ) txt );
        xmlFree( txt );
      }
    }

    else if ( xmlStrcmp( child->name, BAD_CAST "fb_delay_time_0" ) == 0 )
    {
      txt = xmlNodeListGetString( doc, child->xmlChildrenNode, 1 );
      if ( txt != NULL )
      {
        set_min_time( busnumber, atoi( ( char * ) txt ) );
        xmlFree( txt );
      }
    }

    else if (xmlStrcmp(child->name, BAD_CAST "pause_between_commands") == 0)
    {
      txt = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
      if (txt != NULL)
      {
        __ib->pause_between_cmd = atoi((char *) txt);
        xmlFree(txt);
      }
    }

    else
      syslog_bus( busnumber, DBG_WARN,
           "WARNING, unknown tag found: \"%s\"!\n",
           child->name );
    ;

    child = child->next;
  }

  return ( 1 );
}

int cmpTime( struct timeval *t1, struct timeval *t2 )
{
  int result;

  result = 0;
  if ( t2->tv_sec > t1->tv_sec )
  {
    result = 1;
  }
  else
  {
    if ( t2->tv_sec == t1->tv_sec )
    {
      if ( t2->tv_usec > t1->tv_usec )
      {
        result = 1;
      }
    }
  }
  return result;
}

/**
 * cacheInitGL: modifies the gl data used to initialize the device
 * this is called whenever a new loco comes in town...
 */
int init_gl_IB( gl_state_t *gl )
{
  gl->n_fs = 126;
  gl->n_func = 5;
  gl->protocol = 'P';
  return SRCP_OK;
}

/**
 * initGA: modifies the ga data used to initialize the device
 */
int init_ga_IB( ga_state_t *ga )
{
  ga->protocol = 'M';
  return SRCP_OK;
}

int init_bus_IB( bus_t busnumber )
{
  int status;

  if ( init_GA( busnumber, __ib->number_ga ) )
  {
    __ib->number_ga = 0;
    syslog_bus( busnumber, DBG_ERROR, "Can't create array for assesoirs" );
  }

  if ( init_GL( busnumber, __ib->number_gl ) )
  {
    __ib->number_gl = 0;
    syslog_bus( busnumber, DBG_ERROR, "Can't create array for locomotives" );
  }
  if ( init_FB( busnumber, __ib->number_fb * 16 ) )
  {
    __ib->number_fb = 0;
    syslog_bus( busnumber, DBG_ERROR, "Can't create array for feedback" );
  }

  status = 0;
  /* printf("Bus %d with debuglevel %d\n", busnumber, buses[ busnumber ].debuglevel); */
  syslog_bus( busnumber, DBG_INFO, "Bus %d with debuglevel %d\n", busnumber,
       buses[ busnumber ].debuglevel );
  if ( buses[ busnumber ].type != SERVER_IB )
  {
    status = -2;
  }
  else
  {
    if ( buses[ busnumber ].device.file.fd > 0 )
      status = -3;        /* bus is already in use */
  }

  if ( status == 0 )
  {
    __ib->working_IB = 0;
  }

  if ( buses[ busnumber ].debuglevel < 7 )
  {
    if ( status == 0 )
      status = init_lineIB( busnumber );
  }
  else
    buses[ busnumber ].device.file.fd = -1;
  if ( status == 0 )
    __ib->working_IB = 1;

  syslog_bus( busnumber, DBG_INFO, "INIT_BUS_IB exited with code: %d\n", status );
  /* printf( "INIT_BUS_IB exited with code: %d\n", status ); */

  __ib->last_type = -1;
  __ib->emergency_on_ib = 0;

  return status;
}

/*thread cleanup routine for this bus*/
static void end_bus_thread(bus_thread_t *btd)
{
    int result;

    syslog_bus(btd->bus, DBG_INFO, "Intellibox bus terminated.");
    ((IB_DATA*)buses[btd->bus].driverdata)->working_IB = 0;
    close_comport(btd->bus);

    result = pthread_mutex_destroy(&buses[btd->bus].transmit_mutex);
    if (result != 0) {
        syslog_bus(btd->bus, DBG_WARN,
                "pthread_mutex_destroy() failed: %s (errno = %d).",
                strerror(result), result);
    }

    result = pthread_cond_destroy(&buses[btd->bus].transmit_cond);
    if (result != 0) {
        syslog_bus(btd->bus, DBG_WARN,
                "pthread_mutex_init() failed: %s (errno = %d).",
                strerror(result), result);
    }

    free(buses[btd->bus].driverdata);
    free(btd);
}

void *thr_sendrec_IB( void *v )
{
    unsigned char byte2send;
    int status;
    unsigned char rr;
    int zaehler1, fb_zaehler1, fb_zaehler2;
    int last_cancel_state, last_cancel_type;

    bus_thread_t* btd = (bus_thread_t*) malloc(sizeof(bus_thread_t));
    if (btd == NULL)
        pthread_exit((void*) 1);
    btd->bus =  (bus_t) v;
    btd->fd = -1;

    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &last_cancel_state);
    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, &last_cancel_type);

    /*register cleanup routine*/
    pthread_cleanup_push((void *) end_bus_thread, (void *) btd);

    syslog_bus(btd->bus, DBG_INFO, "Intellibox bus started (device = %s).",
        buses[btd->bus].device.file.path);

    /* initialize tga-structure */
    for ( zaehler1 = 0; zaehler1 < 50; zaehler1++ )
        __ibt->tga[ zaehler1 ].id = 0;

    fb_zaehler1 = 0;
    fb_zaehler2 = 1;
    byte2send = XSensOff;
    writeByte( btd->bus, byte2send, 0 );
    status = readByte( btd->bus, 1, &rr );

    while (1) {
        pthread_testcancel();
        if (buses[btd->bus].power_changed == 1)
        {
            if ( __ibt->emergency_on_ib == 1 )
            {
                syslog_bus( btd->bus, DBG_INFO,
                        "send power off to IB off while emergency-stop" );
                __ibt->emergency_on_ib = 2;
                byte2send = 0xA6;
                writeByte( btd->bus, byte2send, __ibt->pause_between_cmd );
                status = readByte( btd->bus, 1, &rr );
                while ( status == -1 )
                {
                    usleep( 100000 );
                    status = readByte( btd->bus, 1, &rr );
                }
                /* sleep(2); */
            }
            else
            {
                if ( ( __ibt->emergency_on_ib == 2 )
                        && ( buses[ btd->bus ].power_state == 0 ) )
                {
                    check_status_IB( btd->bus );
                    usleep( 50000 );
                    continue;
                }
                char msg[ 110 ];
                byte2send = buses[ btd->bus ].power_state ? 0xA7 : 0xA6;
                writeByte(btd->bus, byte2send, __ibt->pause_between_cmd );
                status = readByte_IB( btd->bus, 1, &rr );
                while ( status == -1 )
                {
                    usleep( 100000 );
                    status = readByte_IB( btd->bus, 1, &rr );
                }
                if ( rr == 0x00 )    /* war alles OK? */
                {
                    buses[ btd->bus ].power_changed = 0;
                }
                if ( rr == 0x06 )    /* power on not possible - overheating */
                {
                    buses[ btd->bus ].power_changed = 0;
                    buses[ btd->bus ].power_state = 0;
                }
                if ( buses[ btd->bus ].power_state == 1 )
                    __ibt->emergency_on_ib = 0;
                infoPower( btd->bus, msg );
                queueInfoMessage( msg );
            }
        }

        if ( buses[ btd->bus ].power_state == 0 )
        {
            check_status_IB( btd->bus );
            usleep( 50000 );
            continue;
        }

        send_command_gl_IB( btd->bus );
        send_command_ga_IB( btd->bus );
        check_status_IB( btd->bus );
        send_command_sm_IB( btd->bus );
        check_reset_fb( btd->bus );
        buses[ btd->bus ].watchdog = 1;
        usleep( 50000 );
    }                           /* End WHILE(1) */

    /*run the cleanup routine*/
    pthread_cleanup_pop(1);
    return NULL;
}

void send_command_ga_IB( bus_t busnumber )
{
  int i, i1;
  int temp;
  int addr;
  unsigned char byte2send;
  unsigned char status;
  unsigned char rr;
  ga_state_t gatmp;
  struct timeval akt_time, cmp_time;

  gettimeofday( &akt_time, NULL );

  /* first switch of decoders */
  for ( i = 0; i < 50; i++ )
  {
    if ( __ib->tga[ i ].id )
    {
      syslog_bus( busnumber, DBG_DEBUG, "Time %i,%i", ( int ) akt_time.tv_sec,
           ( int ) akt_time.tv_usec );
      cmp_time = __ib->tga[ i ].t;

      /* switch off time reached? */
      if ( cmpTime( &cmp_time, &akt_time ) )
      {
        gatmp = __ib->tga[ i ];
        addr = gatmp.id;
        byte2send = 0x90;
        writeByte( busnumber, byte2send, 0 );
        temp = gatmp.id;
        temp &= 0x00FF;
        byte2send = temp;
        writeByte( busnumber, byte2send, 0 );
        temp = gatmp.id;
        temp >>= 8;
        byte2send = temp;
        if ( gatmp.port )
        {
          byte2send |= 0x80;
        }
        writeByte( busnumber, byte2send, 0 );
        readByte_IB( busnumber, 1, &rr );
        gatmp.action = 0;
        setGA( busnumber, addr, gatmp );
        __ib->tga[ i ].id = 0;
      }
    }
  }

  /* switch on decoder */
  if ( !queue_GA_isempty( busnumber ) )
  {
    unqueueNextGA( busnumber, &gatmp );
    addr = gatmp.id;
    byte2send = 0x90;
    writeByte( busnumber, byte2send, 0 );
    temp = gatmp.id;
    temp &= 0x00FF;
    byte2send = temp;
    writeByte( busnumber, byte2send, 0 );
    temp = gatmp.id;
    temp >>= 8;
    byte2send = temp;
    if ( gatmp.action )
    {
      byte2send |= 0x40;
    }
    if ( gatmp.port )
    {
      byte2send |= 0x80;
    }
    writeByte( busnumber, byte2send, 0 );
    status = 0;

    /* reschedule event: turn off --to be done-- */
    if ( gatmp.action && ( gatmp.activetime > 0 ) )
    {
      status = 1;
      for ( i1 = 0; i1 < 50; i1++ )
      {
        if ( __ib->tga[ i1 ].id == 0 )
        {
          gatmp.t = akt_time;
          gatmp.t.tv_sec += gatmp.activetime / 1000;
          gatmp.t.tv_usec += ( gatmp.activetime % 1000 ) * 1000;
          if ( gatmp.t.tv_usec > 1000000 )
          {
            gatmp.t.tv_sec++;
            gatmp.t.tv_usec -= 1000000;
          }
          __ib->tga[ i1 ] = gatmp;
          syslog_bus( busnumber, DBG_DEBUG,
               "GA %i for switch off at %i,%i on %i",
               __ib->tga[ i1 ].id, ( int ) __ib->tga[ i1 ].t.tv_sec,
               ( int ) __ib->tga[ i1 ].t.tv_usec, i1 );
          break;
        }
      }
    }
    readByte_IB( busnumber, 1, &rr );
    if ( status )
    {
      setGA( busnumber, addr, gatmp );
    }
  }
}

void send_command_gl_IB( bus_t busnumber )
{
  int temp;
  int addr = 0;
  unsigned char byte2send;
  unsigned char status;
  gl_state_t gltmp, glakt;

  /* locomotive decoder */
  /* fprintf(stderr, "LOK's... "); */
  /* send only if new data is available */
  if ( !queue_GL_isempty( busnumber ) )
  {
    unqueueNextGL( busnumber, &gltmp );
    addr = gltmp.id;
    cacheGetGL( busnumber, addr, &glakt );

    /* speed, direction or function changed? */
    if ( ( gltmp.direction != glakt.direction ) ||
         ( gltmp.speed != glakt.speed ) || ( gltmp.funcs != glakt.funcs ) )
    {
      /* send loco command */
      byte2send = 0x80;
      writeByte( busnumber, byte2send, 0 );
      /* send low byte of address */
      temp = gltmp.id;
      temp &= 0x00FF;
      byte2send = temp;
      writeByte( busnumber, byte2send, 0 );
      /* send high byte of address */
      temp = gltmp.id;
      temp >>= 8;
      byte2send = temp;
      writeByte( busnumber, byte2send, 0 );

      /* if emergency stop is activated set emergency stop */
      if ( gltmp.direction == 2 )
      {
        byte2send = 1;
      }
      else
      {
        /* IB scales speeds INTERNALLY down! */
        /* but gltmp.speed can already contain down-scaled speed */
        /* IB has general range of 0..127, independent of decoder type! */
        byte2send =
          ( unsigned char ) ( ( gltmp.speed * 126 ) / glakt.n_fs );

        /* printf("send_cmd_gl(): speed = %d, n_fs = %d\n", gltmp.speed, glakt.n_fs); */
        /* printf("send_cmd_gl(): sending speed %d to IB\n", byte2send); */

        if ( byte2send > 0 )
        {
          byte2send++;
        }
      }
      writeByte( busnumber, byte2send, 0 );

      /* set direction, light and function */
      byte2send = ( gltmp.funcs >> 1 ) + ( gltmp.funcs & 0x01 ? 0x10 : 0 );
      byte2send |= 0xc0;
      if ( gltmp.direction )
      {
        byte2send |= 0x20;
      }
      writeByte( busnumber, byte2send, 0 );
      readByte_IB( busnumber, 1, &status );
      if ( ( status == 0 ) || ( status == 0x41 ) || ( status == 0x42 ) )
      {
        cacheSetGL( busnumber, addr, gltmp );
      }
    }
  }
}

int read_register_IB( bus_t busnumber, int reg )
{
  unsigned char byte2send;
  unsigned char status;

  byte2send = 0xEC;
  writeByte( busnumber, byte2send, 0 );
  byte2send = reg;
  writeByte( busnumber, byte2send, 0 );
  byte2send = 0;
  writeByte( busnumber, byte2send, 2 );

  readByte( busnumber, 1, &status );

  return status;
}

int write_register_IB( bus_t busnumber, int reg, int value )
{
  unsigned char byte2send;
  unsigned char status;

  byte2send = 0xED;
  writeByte( busnumber, byte2send, 0 );
  byte2send = reg;
  writeByte( busnumber, byte2send, 0 );
  byte2send = 0;
  writeByte( busnumber, byte2send, 0 );
  byte2send = value;
  writeByte( busnumber, byte2send, 2 );

  readByte_IB( busnumber, 1, &status );

  return status;
}

int read_page_IB( bus_t busnumber, int cv )
{
  unsigned char byte2send;
  unsigned char status;
  int tmp;

  byte2send = 0xEE;
  writeByte( busnumber, byte2send, 0 );
  /* low-byte of cv */
  tmp = cv & 0xFF;
  byte2send = tmp;
  writeByte( busnumber, byte2send, 0 );
  /* high-byte of cv */
  tmp = cv >> 8;
  byte2send = tmp;
  writeByte( busnumber, byte2send, 0 );

  readByte_IB( busnumber, 1, &status );

  return status;
}

int write_page_IB( bus_t busnumber, int cv, int value )
{
  unsigned char byte2send;
  unsigned char status;
  int tmp;

  byte2send = 0xEF;
  writeByte( busnumber, byte2send, 0 );
  /* low-byte of cv */
  tmp = cv & 0xFF;
  byte2send = tmp;
  writeByte( busnumber, byte2send, 0 );
  /* high-byte of cv */
  tmp = cv >> 8;
  byte2send = tmp;
  writeByte( busnumber, byte2send, 0 );
  byte2send = value;
  writeByte( busnumber, byte2send, 0 );
  readByte_IB( busnumber, 1, &status );
  return status;
}

int read_cv_IB( bus_t busnumber, int cv )
{
  unsigned char byte2send;
  unsigned char status;
  int tmp;

  byte2send = 0xF0;
  writeByte( busnumber, byte2send, 0 );
  /* low-byte of cv */
  tmp = cv & 0xFF;
  byte2send = tmp;
  writeByte( busnumber, byte2send, 0 );
  /* high-byte of cv */
  tmp = cv >> 8;
  byte2send = tmp;
  writeByte( busnumber, byte2send, 2 );

  readByte_IB( busnumber, 1, &status );

  return status;
}

int write_cv_IB( bus_t busnumber, int cv, int value )
{
  unsigned char byte2send;
  unsigned char status;
  int tmp;

  byte2send = 0xF1;
  writeByte( busnumber, byte2send, 0 );
  /* low-byte of cv */
  tmp = cv & 0xFF;
  byte2send = tmp;
  writeByte( busnumber, byte2send, 0 );
  /* high-byte of cv */
  tmp = cv >> 8;
  byte2send = tmp;
  writeByte( busnumber, byte2send, 0 );
  byte2send = value;
  writeByte( busnumber, byte2send, 2 );

  readByte( busnumber, 1, &status );

  return status;
}

int read_cvbit_IB( bus_t busnumber, int cv, int bit )
{
  unsigned char byte2send;
  unsigned char status;
  int tmp;

  byte2send = 0xF2;
  writeByte( busnumber, byte2send, 0 );
  /* low-byte of cv */
  tmp = cv & 0xFF;
  byte2send = tmp;
  writeByte( busnumber, byte2send, 0 );
  /* high-byte of cv */
  tmp = cv >> 8;
  byte2send = tmp;
  writeByte( busnumber, byte2send, 2 );

  readByte_IB( busnumber, 1, &status );

  return status;
}

int write_cvbit_IB( bus_t busnumber, int cv, int bit, int value )
{
  unsigned char byte2send;
  unsigned char status;
  int tmp;

  byte2send = 0xF3;
  writeByte( busnumber, byte2send, 0 );
  /* low-byte of cv */
  tmp = cv & 0xFF;
  byte2send = tmp;
  writeByte( busnumber, byte2send, 0 );
  /* high-byte of cv */
  tmp = cv >> 8;
  byte2send = tmp;
  writeByte( busnumber, byte2send, 0 );
  byte2send = bit;
  writeByte( busnumber, byte2send, 0 );
  byte2send = value;
  writeByte( busnumber, byte2send, 0 );

  readByte_IB( busnumber, 1, &status );

  return status;
}

/* program decoder on the main */
int send_pom_IB( bus_t busnumber, int addr, int cv, int value )
{
  unsigned char byte2send;
  unsigned char status;
  int ret_val;
  int tmp;

  /* send pom-command */
  byte2send = 0xDE;
  writeByte( busnumber, byte2send, 0 );
  /* low-byte of decoder-address */
  tmp = addr & 0xFF;
  byte2send = tmp;
  writeByte( busnumber, byte2send, 0 );
  /* high-byte of decoder-address */
  tmp = addr >> 8;
  byte2send = tmp;
  writeByte( busnumber, byte2send, 0 );
  /* low-byte of cv */
  tmp = cv & 0xff;
  byte2send = tmp;
  writeByte( busnumber, byte2send, 0 );
  /* high-byte of cv */
  tmp = cv >> 8;
  byte2send = tmp;
  writeByte( busnumber, byte2send, 0 );
  byte2send = value;
  writeByte( busnumber, byte2send, 0 );

  readByte_IB( busnumber, 1, &status );

  ret_val = 0;
  if ( status != 0 )
    ret_val = -1;
  return ret_val;
}

int term_pgm_IB( bus_t busnumber )
{
  unsigned char byte2send;
  unsigned char status;
  int ret_val;

  /* send command turn off PT */
  byte2send = 0xE2;
  writeByte( busnumber, byte2send, 0 );

  readByte( busnumber, 1, &status );

  ret_val = 0;
  if ( status != 0 )
    ret_val = -1;
  return ret_val;
}

void send_command_sm_IB( bus_t busnumber )
{
  /* unsigned char byte2send; */
  /* unsigned char status; */
  struct _SM smakt;

  /* locomotive decoder */
  /* fprintf(stderr, "LOK's... "); */
  /* send only if data is available */
  if ( !queue_SM_isempty( busnumber ) )
  {
    unqueueNextSM( busnumber, &smakt );

    __ib->last_type = smakt.type;
    __ib->last_typeaddr = smakt.typeaddr;
    __ib->last_bit = smakt.bit;
    __ib->last_value = smakt.value;

    syslog_bus( busnumber, DBG_DEBUG, "in send_command_sm: last_type[%d] = %d",
         busnumber, __ib->last_type );
    switch ( smakt.command )
    {
      case SET:
        if ( smakt.addr == -1 )
        {
          switch ( smakt.type )
          {
            case REGISTER:
              write_register_IB( busnumber, smakt.typeaddr,
                                 smakt.value );
              break;
            case CV:
              write_cv_IB( busnumber, smakt.typeaddr, smakt.value );
              break;
            case CV_BIT:
              write_cvbit_IB( busnumber, smakt.typeaddr, smakt.bit,
                              smakt.value );
              break;
            case PAGE:
              write_page_IB( busnumber, smakt.typeaddr, smakt.value );
          }
        }
        else
        {
          send_pom_IB( busnumber, smakt.addr, smakt.typeaddr,
                       smakt.value );
        }
        break;
      case GET:
        switch ( smakt.type )
        {
          case REGISTER:
            read_register_IB( busnumber, smakt.typeaddr );
            break;
          case CV:
            read_cv_IB( busnumber, smakt.typeaddr );
            break;
          case CV_BIT:
            read_cvbit_IB( busnumber, smakt.typeaddr, smakt.bit );
            break;
          case PAGE:
            read_page_IB( busnumber, smakt.typeaddr );
        }
        break;
      case VERIFY:
        break;
      case TERM:
        term_pgm_IB( busnumber );
        break;
    }
  }
}

void check_status_IB( bus_t busnumber )
{
  int i;
  int temp;
  unsigned char byte2send;
  unsigned char rr;
  unsigned char xevnt1, xevnt2, xevnt3;
  gl_state_t gltmp, glakt;
  ga_state_t gatmp;

  /* Request for state�changes:
     1. �derungen an S88-Modulen
     2. manuelle Lokbefehle
     3. manuelle Weichenbefehle */

  /* #warning add loconet */

  byte2send = 0xC8;
  writeByte( busnumber, byte2send, 0 );
  xevnt2 = 0x00;
  xevnt3 = 0x00;
  readByte_IB( busnumber, 1, &xevnt1 );
  if ( xevnt1 & 0x80 )
  {
    readByte_IB( busnumber, 1, &xevnt2 );
    if ( xevnt2 & 0x80 )
    {
      readByte_IB( busnumber, 1, &xevnt3 );
    }
  }

  /* mindestens eine Lok wurde von Hand gesteuert */
  if ( xevnt1 & 0x01 )
  {
    byte2send = 0xC9;
    writeByte( busnumber, byte2send, 0 );
    readByte_IB( busnumber, 1, &rr );
    while ( rr != 0x80 )
    {
      if ( rr == 1 )
      {
        gltmp.speed = 0;        /* Lok befindet sich im Nothalt */
        gltmp.direction = 2;
      }
      else
      {
        gltmp.speed = rr;       /* Lok faehrt mit dieser Geschwindigkeit */
        gltmp.direction = 0;
        if ( gltmp.speed > 0 )
          gltmp.speed--;
      }
      /* 2. byte functions */
      readByte_IB( busnumber, 1, &rr );
      /* gltmp.funcs = rr & 0xf0; */
      gltmp.funcs = ( rr << 1 );
      /* 3. byte address (low-part A7..A0) */
      readByte_IB( busnumber, 1, &rr );
      gltmp.id = rr;
      /* 4. byte address (high-part A13..A8), direction, light */
      readByte_IB( busnumber, 1, &rr );
      if ( ( rr & 0x80 ) && ( gltmp.direction == 0 ) )
        gltmp.direction = 1;    /* direction is forward */
      if ( rr & 0x40 )
        gltmp.funcs |= 0x01;    /* light is on */
      rr &= 0x3F;
      gltmp.id |= rr << 8;

      /* 5. byte real speed (is ignored) */
      readByte_IB( busnumber, 1, &rr );
      /* printf("speed reported in 5th byte: speed = %d\n", rr); */

      /* printf("got GL data from IB: lok# = %d, speed = %d, dir = %d\n", */
      /* gltmp.id, gltmp.speed, gltmp.direction); */

      /* initialize the GL if not done by user, */
      /* because IB can report uninitialized GLs... */
      if ( !isInitializedGL( busnumber, gltmp.id ) )
      {
        syslog_bus( busnumber, DBG_INFO,
             "IB reported uninitialized GL. Performing default init for %d",
             gltmp.id );
        cacheInitGL( busnumber, gltmp.id, 'P', 1, 126, 5 );
      }
      /* get old data, to know which FS the user wants to have... */
      cacheGetGL( busnumber, gltmp.id, &glakt );
      /* recalculate speed */
      gltmp.speed = ( gltmp.speed * glakt.n_fs ) / 126;
      cacheSetGL( busnumber, gltmp.id, gltmp );

      /* next 1. byte */
      readByte_IB( busnumber, 1, &rr );
    }
  }

  /* some feedback state has changed */
  if ( xevnt1 & 0x04 )
  {
    byte2send = 0xCB;
    writeByte( busnumber, byte2send, 0 );
    readByte_IB( busnumber, 1, &rr );
    while ( rr != 0x00 )
    {
      int aktS88 = rr;
      readByte_IB( busnumber, 1, &rr );
      temp = rr;
      temp <<= 8;
      readByte_IB( busnumber, 1, &rr );
      setFBmodul( busnumber, aktS88, temp | rr );
      readByte_IB( busnumber, 1, &rr );
    }
  }

  /* some turnout was switched by hand */
  if ( xevnt1 & 0x20 )
  {
    byte2send = 0xCA;
    writeByte( busnumber, byte2send, 0 );
    readByte_IB( busnumber, 1, &rr );
    temp = rr;
    for ( i = 0; i < temp; i++ )
    {
      readByte_IB( busnumber, 1, &rr );
      gatmp.id = rr;
      readByte_IB( busnumber, 1, &rr );
      gatmp.id |= ( rr & 0x07 ) << 8;
      gatmp.port = ( rr & 0x80 ) ? 1 : 0;
      gatmp.action = ( rr & 0x40 ) ? 1 : 0;
      setGA( busnumber, gatmp.id, gatmp );
    }
  }

  /* overheat, short-circuit on track etc. */
  if ( xevnt2 & 0x3f )
  {
    syslog_bus( busnumber, DBG_DEBUG,
         "On bus %i short detected; old-state is %i", busnumber,
         getPower( busnumber ) );
    if ( ( __ib->emergency_on_ib == 0 ) && ( getPower( busnumber ) ) )
    {
      if ( xevnt2 & 0x20 )
        setPower( busnumber, 0, "Overheating condition detected" );
      if ( xevnt2 & 0x10 )
        setPower( busnumber, 0,
                  "Non-allowed electrical connection between "
                  "programming track and rest of layout" );
      if ( xevnt2 & 0x08 )
        setPower( busnumber, 0,
                  "Overload on DCC-Booster or Loconet" );
      if ( xevnt2 & 0x04 )
        setPower( busnumber, 0, "Short-circuit on internal booster" );
      if ( xevnt2 & 0x02 )
        setPower( busnumber, 0, "Overload on Lokmaus-bus" );
      if ( xevnt2 & 0x01 )
        setPower( busnumber, 0, "Short-circuit on external booster" );

      __ib->emergency_on_ib = 1;
    }
  }

  /* power off? */
  /* we should send an XStatus-command */
  if ( ( xevnt1 & 0x08 ) || ( xevnt2 & 0x40 ) )
  {
    byte2send = 0xA2;
    writeByte( busnumber, byte2send, 0 );
    readByte_IB( busnumber, 1, &rr );
    if ( !( rr & 0x08 ) )
    {
      syslog_bus( busnumber, DBG_DEBUG,
           "On bus %i no power detected; old-state is %i", busnumber,
           getPower( busnumber ) );
      if ( ( __ib->emergency_on_ib == 0 ) && ( getPower( busnumber ) ) )
      {
        setPower( busnumber, 0, "Emergency Stop" );
        __ib->emergency_on_ib = 1;
      }
    }
    else
    {
      syslog_bus( busnumber, DBG_DEBUG,
           "On bus %i power detected; old-state is %i", busnumber,
           getPower( busnumber ) );
      if ( ( __ib->emergency_on_ib == 1 ) || ( !getPower( busnumber ) ) )
      {
        setPower( busnumber, 1, "No Emergency Stop" );
        __ib->emergency_on_ib = 0;
      }
    }
    if ( rr & 0x80 )
      readByte_IB( busnumber, 1, &rr );
  }


  /* we should send an XPT_event-command */
  if ( xevnt3 & 0x01 )
    check_status_pt_IB( busnumber );
}

void check_status_pt_IB( bus_t busnumber )
{
  int i;
  /* int temp; */
  /* int status; */
  unsigned char byte2send;
  unsigned char rr[ 7 ];

  syslog_bus( busnumber, DBG_DEBUG,
       "We've got an answer from programming decoder" );
  /* first clear input-buffer */
  i = 0;
  while ( i == 0 )
  {
    i = readByte( busnumber, 0, &rr[ 0 ] );
  }

  byte2send = 0xCE;
  i = -1;
  while ( i == -1 )
  {
    writeByte( busnumber, byte2send, 0 );
    i = readByte_IB( busnumber, 1, &rr[ 0 ] );
    if ( i == 0 )
    {
      /* wait for an answer of our programming */
      if ( rr[ 0 ] == 0xF5 )
      {
        /* sleep for one second, if answer is not available yet */
        i = -1;
        sleep( 1 );
      }
    }
  }
  for ( i = 0; i < ( int ) rr[ 0 ]; i++ )
    readByte_IB( busnumber, 1, &rr[ i + 1 ] );

  if ( ( int ) rr[ 0 ] < 2 )

    rr[ 2 ] = __ib->last_value;



  if ( __ib->last_type != -1 )
  {
    session_endwait( busnumber, ( int ) rr[ 2 ] );
    setSM( busnumber, __ib->last_type, -1, __ib->last_typeaddr,
           __ib->last_bit, ( int ) rr[ 2 ], ( int ) rr[ 1 ] );
    __ib->last_type = -1;
  }
}

static int open_comport( bus_t busnumber, speed_t baud )
{
  int fd;
  char *name = buses[ busnumber ].device.file.path;
#ifdef linux

  unsigned char rr;
  int status;
#endif

  struct termios interface;

  syslog_bus( busnumber, DBG_INFO, "Try to open serial line %s for %i baud",
       name, ( 2400 * ( 1 << ( baud - 11 ) ) ) );
  fd = open( name, O_RDWR );
  buses[ busnumber ].device.file.fd = fd;
  if (fd == -1)
  {
      syslog_bus(busnumber, DBG_ERROR, "Open serial line failed: %s (errno = %d).\n",
              strerror(errno), errno);
  }
  else
  {
#ifdef linux
    tcgetattr( fd, &interface );
    interface.c_oflag = ONOCR;
    interface.c_cflag =
      CS8 | CRTSCTS | CSTOPB | CLOCAL | CREAD | HUPCL;
    interface.c_iflag = IGNBRK;
    interface.c_lflag = IEXTEN;
    cfsetispeed( &interface, baud );
    cfsetospeed( &interface, baud );
    interface.c_cc[ VMIN ] = 0;
    interface.c_cc[ VTIME ] = 1;
    tcsetattr( fd, TCSANOW, &interface );
    status = 0;
    sleep( 1 );
    while ( status != -1 )
      status = readByte_IB( busnumber, 1, &rr );
#else

    interface.c_ispeed = interface.c_ospeed = baud;
    interface.c_cflag = CREAD | HUPCL | CS8 | CSTOPB | CRTSCTS;
    cfmakeraw( &interface );
    tcsetattr( fd, TCSAFLUSH | TCSANOW, &interface );
#endif

  }
  return fd;
}

static int init_lineIB( bus_t busnumber )
{
  int fd;
  int status;
  speed_t baud;
  unsigned char rr;
  unsigned char byte2send;
  struct termios interface;

  char *name = buses[busnumber].device.file.path;
  syslog_bus( busnumber, DBG_INFO,
          "Beginning to detect IB on serial line: %s\n", name );

  syslog_bus( busnumber, DBG_INFO,
          "Opening serial line %s for 2400 baud\n", name );
  fd = open( name, O_RDWR );
  syslog_bus( busnumber, DBG_DEBUG, "fd = %d", fd );
  if ( fd == -1 )
  {
    syslog_bus(busnumber, DBG_ERROR,
            "Open serial line failed: %s (errno = %d)\n",
            strerror(errno), errno);
    return 1;
  }
  buses[ busnumber ].device.file.fd = fd;
  tcgetattr( fd, &interface );
  interface.c_oflag = ONOCR;
  interface.c_cflag = CS8 | CRTSCTS | CSTOPB | CLOCAL | CREAD | HUPCL;
  interface.c_iflag = IGNBRK;
  interface.c_lflag = IEXTEN;
  cfsetispeed( &interface, B2400 );
  cfsetospeed( &interface, B2400 );
  interface.c_cc[ VMIN ] = 0;
  interface.c_cc[ VTIME ] = 1;
  tcsetattr( fd, TCSANOW, &interface );

  status = 0;
  sleep( 1 );
  syslog_bus( busnumber, DBG_INFO, "Clearing input-buffer\n" );

  while ( status != -1 )
    status = readByte_IB( busnumber, 1, &rr );

  status = 0;

  syslog_bus( busnumber, DBG_INFO, "Sending BREAK... " );

  status = sendBreak( fd, busnumber );
  close( fd );

  if ( status == 0 )
  {
    syslog_bus( busnumber, DBG_INFO, "successful.\n" );
  }
  else
  {
    syslog_bus( busnumber, DBG_INFO, "not successful.\n" );
  }

  /* open the comport in 2400, to get baud rate from IB and turn off
     P50 commands*/
  baud = B2400;
  fd = open_comport( busnumber, baud );
  buses[ busnumber ].device.file.fd = fd;
  if ( fd < 0 )
  {
      syslog_bus(busnumber, DBG_ERROR,
              "Open serial line failed: %s (errno = %d)\n",
              strerror(errno), errno);
    return ( -1 );
  }

  baud = checkBaudrate( fd, busnumber );
  if ( ( baud == B0 ) || ( baud > B38400 ) )
  {
    syslog_bus( busnumber, DBG_ERROR, "checkBaurate() failed\n" );
    return -1;
  }

  buses[ busnumber ].device.file.baudrate = baud;

  status = switchOffP50Command( busnumber );
  status = resetBaudrate( buses[ busnumber ].device.file.baudrate, busnumber );
  close_comport(busnumber);

  sleep( 1 );

  /* now open the comport for the communication */

  fd = open_comport( busnumber, buses[ busnumber ].device.file.baudrate );
  buses[ busnumber ].device.file.fd = fd;
  syslog_bus( busnumber, DBG_DEBUG, "fd after open_comport = %d", fd );
  if ( fd < 0 )
  {
    printf( "open_comport() failed\n" );
    return ( -1 );
  }
  byte2send = 0xC4;
  writeByte( busnumber, byte2send, 0 );
  status = readByte_IB( busnumber, 1, &rr );
  if ( status == -1 )
    return ( 1 );
  if ( rr == 'D' )
  {
    syslog_bus( busnumber, DBG_FATAL,
         "Intellibox in download mode.\nDO NOT PROCEED!\n" );
    return ( 2 );
  }

  return 0;
}

static int sendBreak( const int fd, bus_t busnumber )
{
  if ( tcflush( fd, TCIOFLUSH ) != 0 )
  {
    syslog_bus(busnumber, DBG_ERROR,
            "sendBreak(): tcflush before break failed: %s (errno = %d)\n",
            strerror(errno), errno);
    return -1;
  }

  tcflow( fd, TCOOFF );
  usleep( 300000 );             /* 300 ms */

  if ( tcsendbreak( fd, 100 ) != 0 )
  {
    syslog_bus(busnumber, DBG_ERROR,
            "sendBreak(): tcsendbreak failed: %s (errno = %d)\n",
            strerror(errno), errno);
    return -1;
  }

  sleep( 3 );
  usleep( 600000 );             /* 600 ms */
  tcflow( fd, TCOON );
  return 0;
}


/**
 * checks the baud rate of the intellibox;
 * see interface description of intellibox
 *
 * @param file descriptor of the port
 * @param  busnumber inside srcp
 * @return the correct baudrate or -1 if nor recognized
 **/
speed_t checkBaudrate( const int fd, const bus_t busnumber )
{
  int found = 0;
  short error = 0;
  int baudrate = 2400;
  struct termios interface;
  unsigned char input[ 10 ];
  unsigned char out;
  short len = 0;
  int i;
  speed_t internalBaudrate = B0;

  syslog_bus( busnumber, DBG_INFO,
       "Checking baud rate inside IB, see special option S1 in IB-Handbook\n" );
  for ( i = 0; i < 10; i++ )
    input[ i ] = 0;
  while ( ( found == 0 ) && ( baudrate <= 38400 ) )
  {
    syslog_bus( busnumber, DBG_INFO, "baudrate = %i\n", baudrate );
    error = tcgetattr( fd, &interface );
    if ( error < 0 )
    {
        syslog_bus(busnumber, DBG_ERROR,
                "checkBaudrate(): tcgetattr failed: %s (errno = %d)\n",
                strerror(errno), errno);
        return B0;
    }
    switch ( baudrate )
    {
      case 2400:
        internalBaudrate = B2400;
        break;
      case 4800:
        internalBaudrate = B4800;
        break;
      case 9600:
        internalBaudrate = B9600;
        break;
      case 19200:
        internalBaudrate = B19200;
        break;
      case 38400:
        internalBaudrate = B38400;
        break;
      default:
        internalBaudrate = B19200;
        break;
    }

    if ( cfsetispeed( &interface, internalBaudrate ) != 0 )
    {
        syslog_bus(busnumber, DBG_ERROR,
                "checkBaudrate(): cfsetispeed failed: %s (errno = %d)\n",
                strerror(errno), errno);
        /*CHECK: What to do now?*/
    }
    tcflush( fd, TCOFLUSH );
    error = tcsetattr( fd, TCSANOW, &interface );
    if ( error != 0 )
    {
        syslog_bus(busnumber, DBG_ERROR,
                "checkBaudrate(): tcsetattr failed: %s (errno = %d)\n",
                strerror(errno), errno);
        return B0;
    }

    out = 0xC4;
    writeByte( busnumber, out, 0 );
    usleep( 200000 );         /* 200 ms */

    for ( i = 0; i < 2; i++ )
    {
      int erg = readByte_IB( busnumber, 1, &input[ i ] );
      /* printf("baudrate = %d, readyByte_ib() returned %d\n", baudrate, erg); */
      if ( erg == 0 )
        len++;
    }

    /* printf("Answer from IB: %s\n", input); */

    switch ( len )
    {
      case 1:
        /* IBox has P50 commands disabled */
        found = 1;
        if ( input[ 0 ] == 'D' )
        {
          syslog_bus( busnumber, DBG_FATAL,
               "Intellibox in download mode.\nDO NOT PROCEED!\n" );
          return ( 2 );
        }
        syslog_bus( busnumber, DBG_INFO,
             "IBox found; P50-commands are disabled.\n" );
        break;
      case 2:
        /* IBox has P50 commands enabled */
        found = 1;
        /* don't know if this also works, when P50 is enabled... */
        /* check disabled for now... */
        syslog_bus( busnumber, DBG_INFO,
             "IBox found; P50-commands are enabled.\n" );
        break;
      default:
        found = 0;
        break;
    }
    if ( found == 0 )
    {
      baudrate <<= 1;
      internalBaudrate = B0;
    }
    usleep( 200000 );         /* 200 ms */
  }
  syslog_bus( busnumber, DBG_INFO, "Baudrate checked: %d\n", baudrate );
  return internalBaudrate;
}

/**
 * reset the baud rate inside ib depending on par 1
 * @param requested baudrate
 * @return: 0 if successfull
 **/
static int resetBaudrate( const speed_t speed, const bus_t busnumber )
{
  unsigned char byte2send;
  int status;
  unsigned char *sendString;

  switch ( speed )
  {
    case B2400:
      syslog_bus( busnumber, DBG_INFO, "Changing baud rate to 2400 bps\n" );
      sendString = (unsigned char *) "B2400";
      writeString( busnumber, sendString, 0 );
      break;
    case B4800:
      syslog_bus( busnumber, DBG_INFO, "Changing baud rate to 4800 bps\n" );
      sendString = (unsigned char *) "B4800";
      writeString( busnumber, sendString, 0 );
      break;
    case B9600:
      syslog_bus( busnumber, DBG_INFO, "Changing baud rate to 9600 bps\n" );
      sendString = (unsigned char *) "B9600";
      writeString( busnumber, sendString, 0 );
      break;
    case B19200:
      syslog_bus( busnumber, DBG_INFO, "Changing baud rate to 19200 bps\n" );
      sendString = (unsigned char *) "B19200";
      writeString( busnumber, sendString, 0 );
      break;
    case B38400:
      syslog_bus( busnumber, DBG_INFO, "Changing baud rate to 38400 bps\n" );
      sendString = (unsigned char *) "B38400";
      writeString( busnumber, sendString, 0 );
      break;
  }

  byte2send = 0x0d;
  writeByte( busnumber, byte2send, 0 );
  usleep( 200000 );             /* 200 ms */
  /* use following line to see some debugging */
  /* status = readAnswer_ib(busnumber, 1); */
  status = readAnswer_IB( busnumber, 0 );
  return (status != 0) ? 1: 0;
}

/**
 * sends the command to switch of P50 commands off, see interface
 * description of intellibox
 *
 * the answer of the intellibox is shown with printf
 *
 * @param  busnumber inside srcpd
 * @return 0 if OK
 **/
static int switchOffP50Command( const bus_t busnumber )
{
  unsigned char byte2send;
  unsigned char *sendString = (unsigned char *)"xZzA1";
  int status;

  syslog_bus( busnumber, DBG_INFO, "Switching off P50-commands\n" );

  writeString( busnumber, sendString, 0 );
  byte2send = 0x0d;
  writeByte( busnumber, byte2send, 0 );

  /* use following line, to see some debugging */
  /* status = readAnswer_ib(busnumber, 1); */
  status = readAnswer_IB( busnumber, 0 );
  return (status != 0) ? 1: 0;
}

/**
 * reads an answer of the intellibox after a command in P50 mode.
 *
 * If required, the answer of the intellibox is shown with printf.
 * Usually the method reads until '[' is received, which is defined
 * as the end of the string. This last char is not printed.
 *
 * @param  busnumber inside srcp
 * @param if > 0 the answer is printed
 * @return 0 if OK
 **/
static int readAnswer_IB( const bus_t busnumber,
                          const int generatePrintf )
{
  unsigned char input[ 80 ];
  int counter = 0;
  int found = 0;
  int i;

  for ( i = 0; i < 80; i++ )
    input[ i ] = 0;

  while ( ( found == 0 ) && ( counter < 80 ) )
  {
    if ( readByte_IB( busnumber, 1, &input[ counter ] ) == 0 )
    {
      if ( input[ counter ] == 0 )
        input[ counter ] = 0x20;
      if ( input[ counter ] == ']' )
      {
        input[ counter ] = 0;
        found = 1;
      }
    }
    counter++;
  }

  if ( found == 0 )
    return -1;

  if ( generatePrintf > 0 )
  {
    syslog_bus( busnumber, DBG_INFO, "IBox returned: %s\n", input );
  }
  return 0;
}

/**
 * Provides an extra interface for reading one byte out of the intellibox.
 *
 * Tries to read one byte 10 times. If no byte is received -1 is returned.
 * Because the IB guarantees an answer during 50 ms all write bytes can
 * be generated with a waiting time of 0, and readByte_ib can be called
 * directly after write
 *
 * @param: busnumber
 * @param: wait time during read
 * @param: address of the byte to be received.
 **/
static int readByte_IB( bus_t bus, int wait, unsigned char *the_byte )
{
  int i;
  int status;

  for ( i = 0; i < 10; i++ )
  {
    status = readByte( bus, wait, the_byte );
    if ( status == 0 )
      return 0;
    /* printf("readByte() unsuccessfully; status = %d\n", status); */
    usleep( 10000 );
  }
  return -1;
}
