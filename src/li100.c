/***************************************************************************
         li100.c  -  description
           -------------------
begin                : Tue Jan 22 11:35:13 MEST 2002
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

/***************************************************************************
 *                                                                         *
 *  Changes:                                                               *
 *                                                                         *
 *                                                                         *
 ***************************************************************************/
#include "stdincludes.h"

#ifdef linux
#include <linux/serial.h>
#include <sys/io.h>
#endif

#include "config-srcpd.h"
#include "li100.h"
#include "io.h"
#include "srcp-ga.h"
#include "srcp-gl.h"
#include "srcp-sm.h"
#include "srcp-time.h"
#include "srcp-fb.h"
#include "srcp-power.h"
#include "srcp-info.h"
#include "srcp-error.h"

#define __li100 ((LI100_DATA*)busses[busnumber].driverdata)

int cmpTime( struct timeval *t1, struct timeval *t2 );

static int readAnswer_LI100( int busnumber, unsigned char *str );
static int initLine_LI100( int busnumber );

void readConfig_LI100( xmlDocPtr doc, xmlNodePtr node, int busnumber )
{
  xmlNodePtr child = node->children;
  DBG( busnumber, DBG_INFO, "reading configuration for LI100 at bus %d", busnumber );

  busses[ busnumber ].type = SERVER_LI100;
  busses[ busnumber ].init_func = &init_bus_LI100;
  busses[ busnumber ].term_func = &term_bus_LI100;
  busses[ busnumber ].thr_func = &thr_sendrec_LI100;
  busses[ busnumber ].init_gl_func = &init_gl_LI100;
  busses[ busnumber ].init_ga_func = &init_ga_LI100;
  busses[ busnumber ].driverdata = malloc( sizeof( struct _LI100_DATA ) );
  busses[ busnumber ].flags |= FB_16_PORTS;
  busses[ busnumber ].baudrate = B9600;
  busses[ busnumber ].numberOfSM = 0;

  strcpy( busses[ busnumber ].description, "GA GL FB SM POWER LOCK DESCRIPTION" );
  __li100->number_fb = 256;
  __li100->number_ga = 256;
  __li100->number_gl = 99;

  while ( child )
  {
    if ( strcmp( child->name, "number_fb" ) == 0 )
    {
      char * txt = xmlNodeListGetString( doc, child->xmlChildrenNode, 1 );
      __li100->number_fb = atoi( txt );
      free( txt );
    }

    if ( strcmp( child->name, "number_gl" ) == 0 )
    {
      char * txt = xmlNodeListGetString( doc, child->xmlChildrenNode, 1 );
      __li100->number_gl = atoi( txt );
      free( txt );
    }
    if ( strcmp( child->name, "number_ga" ) == 0 )
    {
      char * txt = xmlNodeListGetString( doc, child->xmlChildrenNode, 1 );
      __li100->number_ga = atoi( txt );
      free( txt );
    }
    if ( strcmp( child->name, "number_sm" ) == 0 )
    {
      char * txt = xmlNodeListGetString( doc, child->xmlChildrenNode, 1 );
      busses[ busnumber ].numberOfSM = atoi( txt );
      free( txt );
    }
    if ( strcmp( child->name, "p_time" ) == 0 )
    {
      char * txt = xmlNodeListGetString( doc, child->xmlChildrenNode, 1 );
      set_min_time( busnumber, atoi( txt ) );
      free( txt );
    }
    child = child->next;
  }
}
/**
 * initGL: modifies the gl data used to initialize the device
 * this is called whenever a new loco comes in town...
 */
int init_gl_LI100( struct _GLSTATE *gl )
{
  gl -> n_fs = 14;
  gl -> n_func = 1;
  gl -> protocol = 'N';
  return SRCP_OK;
}
/**
 * initGA: modifies the ga data used to initialize the device

 */
int init_ga_LI100( struct _GASTATE *ga )
{
  ga -> protocol = 'N';
  return SRCP_OK;
}

int init_bus_LI100( int busnumber )
{
  int status;

  if ( init_GA( busnumber, __li100->number_ga ) )
  {
    __li100->number_ga = 0;
    DBG( busnumber, DBG_ERROR, "Can't create array for assesoirs" );
  }

  if ( init_GL( busnumber, __li100->number_gl ) )
  {
    __li100->number_gl = 0;
    DBG( busnumber, DBG_ERROR, "Can't create array for locomotivs" );
  }
  if ( init_FB( busnumber, __li100->number_fb * 16 ) )
  {
    __li100->number_fb = 0;
    DBG( busnumber, DBG_ERROR, "Can't create array for feedback" );
  }

  status = 0;
  printf( "Bus %d with debuglevel %d\n", busnumber, busses[ busnumber ].debuglevel );
  if ( busses[ busnumber ].type != SERVER_LI100 )
  {
    status = -2;
  }
  else
  {
    if ( busses[ busnumber ].fd > 0 )
      status = -3;              // bus is already in use
  }

  if ( status == 0 )
  {
    __li100->working_LI100 = 0;
  }

  if ( busses[ busnumber ].debuglevel < 7 )
  {
    if ( status == 0 )
      status = initLine_LI100( busnumber );
  }
  else
    busses[ busnumber ].fd = 9999;
  if ( status == 0 )
    __li100->working_LI100 = 1;

  printf( "INIT_BUS_LI100 exited with code: %d\n", status );

  __li100->last_type = -1;
  __li100->emergency_on_LI100 = 0;

  return status;
}

int term_bus_LI100( int busnumber )
{
  if ( busses[ busnumber ].type != SERVER_LI100 )
    return 1;

  if ( busses[ busnumber ].pid == 0 )
    return 0;

  __li100->working_LI100 = 0;

  pthread_cancel( busses[ busnumber ].pid );
  busses[ busnumber ].pid = 0;
  close_comport( busnumber );
  return 0;
}

void* thr_sendrec_LI100( void *v )
{
  int busnumber;
  unsigned char byte2send[ 20 ];
  int status;

  int zaehler1, fb_zaehler1, fb_zaehler2;

  busnumber = ( int ) v;
  DBG( busnumber, DBG_INFO, "thr_sendrec_LI100 is startet as bus %i", busnumber );

  // initialize tga-structure
  for ( zaehler1 = 0;zaehler1 < 50;zaehler1++ )
    __li100->tga[ zaehler1 ].id = 0;

  fb_zaehler1 = 0;
  fb_zaehler2 = 1;

  while ( 1 )
  {
    //  syslog(LOG_INFO, "thr_sendrec_LI100 Start in Schleife");
    /* Start/Stop */
    //fprintf(stderr, "START/STOP... ");
    if ( busses[ busnumber ].power_changed )
    {
      byte2send[ 0 ] = 0x21;
      byte2send[ 1 ] = busses[ busnumber ].power_state ? 0x81 : 0x80;
      status = send_command_LI100( busnumber, byte2send );
      if ( status == 0 )                                // war alles OK ?
        busses[ busnumber ].power_changed = 0;
    }

    send_command_gl_LI100( busnumber );
    send_command_ga_LI100( busnumber );
    check_status_LI100( busnumber );
  }      // Ende WHILE(1)
}

void send_command_ga_LI100( int busnumber )
{
  int i, i1;
  int temp;
  int addr;
  unsigned char byte2send[ 20 ];
  unsigned char status;
  struct _GASTATE gatmp;
  struct timeval akt_time, cmp_time;

  gettimeofday( &akt_time, NULL );
  // zuerst eventuell Decoder abschalten
  for ( i = 0;i < 50;i++ )
  {
    if ( __li100->tga[ i ].id )
    {
      syslog( LOG_INFO, "Zeit %i,%i", ( int ) akt_time.tv_sec, ( int ) akt_time.tv_usec );
      cmp_time = __li100->tga[ i ].t;
      if ( cmpTime( &cmp_time, &akt_time ) )                        // Ausschaltzeitpunkt erreicht ?
      {
        gatmp = __li100->tga[ i ];
        addr = gatmp.id;
        temp = addr;
        temp--;
        byte2send[ 0 ] = 0x52;
        byte2send[ 1 ] = temp >> 2;
        byte2send[ 2 ] = 0x80;
        temp &= 0x03;
        temp <<= 1;
        byte2send[ 2 ] |= temp;
        if ( gatmp.port )
        {
          byte2send[ 2 ] |= 0x01;
        }
        send_command_LI100( busnumber, byte2send );
        gatmp.action = 0;
        setGA( busnumber, addr, gatmp );
        __li100->tga[ i ].id = 0;
      }
    }
  }

  // Decoder einschalten
  if ( !queue_GA_isempty( busnumber ) )
  {
    unqueueNextGA( busnumber, &gatmp );
    addr = gatmp.id;
    temp = addr;
    temp--;
    byte2send[ 0 ] = 0x52;
    byte2send[ 1 ] = temp >> 2;
    byte2send[ 2 ] = 0x80;
    temp &= 0x03;
    temp <<= 1;
    byte2send[ 2 ] |= temp;
    if ( gatmp.action )
    {
      byte2send[ 2 ] |= 0x08;
    }
    if ( gatmp.port )
    {
      byte2send[ 2 ] |= 0x01;
    }
    send_command_LI100( busnumber, byte2send );
    status = 1;
    if ( gatmp.action && ( gatmp.activetime > 0 ) )
    {
      for ( i1 = 0;i1 < 50;i1++ )
      {
        if ( __li100->tga[ i1 ].id == 0 )
        {
          gatmp.t = akt_time;
          gatmp.t.tv_sec += gatmp.activetime / 1000;
          gatmp.t.tv_usec += ( gatmp.activetime % 1000 ) * 1000;
          if ( gatmp.t.tv_usec > 1000000 )
          {
            gatmp.t.tv_sec++;
            gatmp.t.tv_usec -= 1000000;
          }
          __li100->tga[ i ] = gatmp;
          status = 0;
          DBG( busnumber, DBG_DEBUG, "GA %i für Abschaltung um %i,%i auf %i", __li100->tga[ i1 ].id,
               ( int ) __li100->tga[ i1 ].t.tv_sec, ( int ) __li100->tga[ i1 ].t.tv_usec, i1 );
          break;
        }
      }
    }
    if ( status )
    {
      setGA( busnumber, addr, gatmp );
    }
  }
}

void send_command_gl_LI100( int busnumber )
{
  int temp;
  int addr;
  unsigned char byte2send[ 20 ];
  unsigned char status;
  struct _GLSTATE gltmp, glakt;

  /* Lokdecoder */
  //fprintf(stderr, "LOK's... ");
  /* nur senden, wenn wirklich etwas vorliegt */
  if ( !queue_GL_isempty( busnumber ) )
  {
    unqueueNextGL( busnumber, &gltmp );
    addr = gltmp.id;
    getGL( busnumber, addr, &glakt );

    // speed, direction or function changed ?
    if ( ( gltmp.direction != glakt.direction ) ||
         ( gltmp.speed != glakt.speed ) ||
         ( gltmp.funcs != glakt.funcs ) )
    {
      // Lokkommando soll gesendet werden
      if ( gltmp.direction == 2 )                         // emergency stop for one locomotiv
      {
        if ( __li100->version_zentrale >= 0x0300 )
        {
          byte2send[ 0 ] = 0x92;
          byte2send[ 1 ] = addr >> 8;
          if ( addr > 99 )
            byte2send[ 1 ] |= 0xc0;
          byte2send[ 2 ] = addr & 0xff;
          send_command_LI100( busnumber, byte2send );
        }
        else
        {
          byte2send[ 0 ] = 0x91;
          byte2send[ 1 ] = addr;
          send_command_LI100( busnumber, byte2send );
        }
      }
      else
      {
        if ( __li100->version_zentrale >= 0x0300 )
        {
          byte2send[ 0 ] = 0xe4;
          // mode
          switch ( gltmp.n_fs )
          {
            case 14:
              byte2send[ 1 ] = 0x10;
              break;
            case 27:
              byte2send[ 1 ] = 0x11;
              break;
            case 28:
              byte2send[ 1 ] = 0x12;
              break;
            case 126:
              byte2send[ 1 ] = 0x13;
              break;
            default:
              byte2send[ 1 ] = 0x12;
          }
          // highbyte of adress
          temp = gltmp.id;
          temp >>= 8;
          byte2send[ 2 ] = temp;
          if ( addr > 99 )
            byte2send[ 2 ] |= 0xc0;
          // lowbyte of adress
          temp = gltmp.id;
          temp &= 0x00FF;
          byte2send[ 3 ] = temp;
          // setting direction and speed
          byte2send[ 4 ] = gltmp.speed;
          if ( gltmp.speed > 0 )
            byte2send[ 4 ] ++;
          if ( gltmp.direction )
          {
            byte2send[ 4 ] |= 0x80;
          }
          send_command_LI100( busnumber, byte2send );

          byte2send[ 0 ] = 0xe4;
          // mode
          byte2send[ 1 ] = 0x20;
          // highbyte of adress
          temp = gltmp.id;
          temp >>= 8;
          byte2send[ 2 ] = temp;
          if ( addr > 99 )
            byte2send[ 2 ] |= 0xc0;
          // lowbyte of adress
          temp = gltmp.id;
          temp &= 0x00FF;
          byte2send[ 3 ] = temp;
          // setting F0-F4
          byte2send[ 4 ] = ( gltmp.funcs >> 1 ) & 0x00FF;
          if ( gltmp.funcs & 1 )
          {
            byte2send[ 4 ] |= 0x10;
          }
          send_command_LI100( busnumber, byte2send );


          /*          byte2send[ 0 ] = 0xe4;
                   // mode
                   switch ( gltmp.n_fs )
                   {
                     case 14:
                       byte2send[ 1 ] = 0x10;
                       break;
                     case 27:
                       byte2send[ 1 ] = 0x11;
                       break;
                     case 28:
                       byte2send[ 1 ] = 0x12;
                       break;
                     case 126:
                       byte2send[ 1 ] = 0x13;
                       break;
                     default:
                       byte2send[ 1 ] = 0x12;
                   }
                   // highbyte of adress
                   temp = gltmp.id;
                   temp >>= 8;
                   byte2send[ 2 ] = temp;
                   if ( addr > 99 )
                     byte2send[ 2 ] |= 0xc0;
                   // lowbyte of adress
                   temp = gltmp.id;
                   temp &= 0x00FF;
                   byte2send[ 3 ] = temp;
                   // setting direction and speed
                   byte2send[ 4 ] = gltmp.speed;
                   if ( gltmp.speed > 0 )
                     byte2send[ 4 ] ++;
                   if ( gltmp.direction )
                   {
                     byte2send[ 4 ] |= 0x80;
                   }
                   send_command_LI100( busnumber, byte2send );

                   byte2send[ 0 ] = 0xe4;
                   // mode
                   switch ( gltmp.n_fs )
                   {
                     case 14:
                       byte2send[ 1 ] = 0x10;
                       break;
                     case 27:
                       byte2send[ 1 ] = 0x11;
                       break;
                     case 28:
                       byte2send[ 1 ] = 0x12;
                       break;
                     case 126:
                       byte2send[ 1 ] = 0x13;
                       break;
                     default:
                       byte2send[ 1 ] = 0x12;
                   }
                   // highbyte of adress
                   temp = gltmp.id;
                   temp >>= 8;
                   byte2send[ 2 ] = temp;
                   if ( addr > 99 )
                     byte2send[ 2 ] |= 0xc0;
                   // lowbyte of adress
                   temp = gltmp.id;
                   temp &= 0x00FF;
                   byte2send[ 3 ] = temp;
                   // setting direction and speed
                   byte2send[ 4 ] = gltmp.speed;
                   if ( gltmp.speed > 0 )
                     byte2send[ 4 ] ++;
                   if ( gltmp.direction )
                   {
                     byte2send[ 4 ] |= 0x80;
                   }
                   send_command_LI100( busnumber, byte2send );*/
        }
        else
        {
          /*          byte2send = 0x80;
                    writeByte(busnumber, byte2send, 0);
                    // send lowbyte of adress
                    temp = gltmp.id;
                    temp &= 0x00FF;
                    byte2send = temp;
                    writeByte(busnumber, byte2send, 0);
                    // send highbyte of adress
                    temp = gltmp.id;
                    temp >>= 8;
                    byte2send = temp;
                    writeByte(busnumber, byte2send, 0);
                    if(gltmp.direction == 2)       // Nothalt ausgel�t ?
                    {
                      byte2send = 1;              // Nothalt setzen
                    }
                    else
                    {
                      writeByte(busnumber, byte2send, 0);
                      // setting direction, light and function
                      byte2send = (gltmp.funcs >> 1) + (gltmp.funcs & 0x01?0x10:0);
                      byte2send |= 0xc0;
                      if(gltmp.direction)
                      {
                        byte2send |= 0x20;
                      }
                    writeByte(busnumber, byte2send, 0);
                    }*/
        }
      }
      if ( status == 0 )
      {
        setGL( busnumber, addr, gltmp );
      }
    }
  }
}

void check_status_LI100( int busnumber )
{
  int i;
  int status;
  unsigned char str[ 20 ];


  // with debuglevel beyond DBG_DEBUG, we will not really work on hardware
  if ( busses[ busnumber ].debuglevel <= DBG_DEBUG )
  {
    i = 1;
    while ( i > 0 )
    {
      status = ioctl( busses[ busnumber ].fd, FIONREAD, &i );
      if ( status < 0 )
      {
        char msg[ 200 ];
        strcpy( msg, strerror( errno ) );
        DBG( busnumber, DBG_ERROR, "readbyte(): IOCTL   status: %d with errno = %d: %s", status, errno, msg );
      }
      DBG( busnumber, DBG_DEBUG, "readbyte(): (fd = %d), there are %d bytes to read.", busses[ busnumber ].fd, i );
      // read only, if there is really an input
      if ( i > 0 )
        status = readAnswer_LI100( busnumber, str );
    }
  }
}

int send_command_LI100( int busnumber, unsigned char *str )
{
  int ctr, i;
  int status;

  str[ 19 ] = 0x00;                   // control-byte for xor
  ctr = str[ 0 ] & 0x0f;              // generate length of command
  ctr++;
  for ( i = 0;i < ctr;i++ )                                  // send command
  {
    str[ 19 ] ^= str[ i ];
    writeByte( busnumber, str[ i ], 0 );
  }
  writeByte( busnumber, str[ 19 ], 250 ); // send X-Or-Byte

  status = readAnswer_LI100( busnumber, str );
  return status;
}

static int readAnswer_LI100( int busnumber, unsigned char *str )
{
  int status;
  int i, ctr;
  unsigned char cXor;

  status = -1;                      // wait for answer
  while ( status == -1 )
  {
    usleep( 2000 );
    status = readByte( busnumber, 1, &str[ 0 ] );
  }
  ctr = str[ 0 ] & 0x0f;              // generate length of answer
  ctr += 2;
  for ( i = 1;i < ctr;i++ )                  // read answer
  {
    readByte( busnumber, 1, &str[ i ] );
  }

  cXor = 0;
  for ( i = 0 ;i < ctr ;i++ )
  {
    cXor ^= str[ i ];
  }
  if ( cXor != 0x00 )                                   // must be 0x00
    status = -1;                    // error
  //9216 PRINT #2, SEND$; : IST$ = "--": GOSUB 9000           'senden
  //9218 '
  //9220 IF IST$ = "OK" THEN RETURN                           'alles klar
  //9222 IF IST$ = "TA" THEN GOSUB 9240                       'fehler timeout
  //9224 IF IST$ = "PC" THEN GOSUB 9250                       'fehler PC -> LI
  //9226 IF IST$ = "LZ" THEN GOSUB 9260                       'fehler LI -> LZ
  //9228 IF IST$ = "??" THEN GOSUB 9270                       'befehl unbekannt
  //9230 IF IST$ = "KA" THEN GOSUB 9280                       'keine antwort
  //9232 RETURN

  if ( str[ 0 ] == 0x02 )                         // version-number of interface
  {
    __li100->version_interface = ( ( str[ 1 ] & 0xf0 ) << 4 ) + ( str[ 1 ] & 0x0f );
    __li100->code_interface = ( int ) str[ 2 ];
  }

  // version-number of zentrale
  if ( ( str[ 0 ] == 0x62 ) || ( str[ 0 ] == 0x63 ) )
  {
    if ( str[ 1 ] == 0x21 )
    {
      __li100->version_zentrale = ( ( str[ 2 ] & 0xf0 ) << 4 ) + ( str[ 2 ] & 0x0f );
      if ( str[ 0 ] == 0x63 )
        __li100->code_interface = ( int ) str[ 3 ];
      else
        __li100->code_interface = -1;
      // check for address-range 1..99 for version < V3.00
      if ( __li100->version_zentrale < 0x0300 )
      {
        if ( __li100->number_gl > 99 )
          __li100->number_gl = 99;
        if ( busses[ busnumber ].numberOfSM > 99 )
          ;
        busses[ busnumber ].numberOfSM = 99;
      }
    }
  }

  // power on/off
  if ( str[ 0 ] == 0x61 )
  {
    // power on
    if ( str[ 1 ] == 0x01 )
    {
      DBG( busnumber, DBG_DEBUG, "on bus %i power detected; old-state is %i", busnumber, getPower( busnumber ) );
      if ( ( __li100->emergency_on_LI100 == 1 ) || ( !getPower( busnumber ) ) )
      {
        char msg[ 500 ];
        setPower( busnumber, 1, "No Emergency Stop" );
        infoPower( busnumber, msg );
        queueInfoMessage( msg );
        __li100->emergency_on_LI100 = 0;
      }
      // power off
      if ( str[ 1 ] == 0x00 )
      {
        DBG( busnumber, DBG_DEBUG, "on bus %i no power detected; old-state is %i", busnumber, getPower( busnumber ) );
        if ( ( __li100->emergency_on_LI100 == 0 ) && ( getPower( busnumber ) ) )
        {
          char msg[ 500 ];
          setPower( busnumber, 0, "Emergency Stop" );
          infoPower( busnumber, msg );
          queueInfoMessage( msg );
          __li100->emergency_on_LI100 = 1;
        }
      }
    }
  }

  if ( str[ 0 ] == 0xe3 )
  {
    if ( str[ 1 ] == 0x40 )
    {
      str[ 0 ] = 0xe4;
      str[ 1 ] = 0x00;
      send_command_LI100( busnumber, str );
      return 0;
    }
  }
  if ( str[ 0 ] == 0xe4 )
  {}
  return status;
}

static int initLine_LI100( int busnumber )
{
  int status;
  int fd;
  unsigned char byte2send[ 20 ];
  struct termios interface;

  char *name = busses[ busnumber ].device;
  printf( "Begining to detect LI100 on serial line: %s\n", name );

  printf( "try opening serial line %s for 9600 baud\n", name );
  fd = open( name, O_RDWR );
  if ( fd == -1 )
  {
    printf( "dammit, couldn't open device.\n" );
    return 1;
  }
  busses[ busnumber ].fd = fd;
  tcgetattr( fd, &interface );
  interface.c_oflag = ONOCR;
  interface.c_cflag = CS8 | CRTSCTS | CSTOPB | CLOCAL | CREAD | HUPCL;
  interface.c_iflag = IGNBRK;
  interface.c_lflag = IEXTEN;
  cfsetispeed( &interface, B9600 );
  cfsetospeed( &interface, B9600 );
  interface.c_cc[ VMIN ] = 0;
  interface.c_cc[ VTIME ] = 1;
  tcsetattr( fd, TCSANOW, &interface );
  sleep( 1 );

  // get version of LI100
  byte2send[ 0 ] = 0xF0;
  status = send_command_LI100( busnumber, byte2send );

  // get version of zentrale
  byte2send[ 0 ] = 0x21;
  byte2send[ 1 ] = 0x21;
  status = send_command_LI100( busnumber, byte2send );

  // get status of zentrale
  byte2send[ 0 ] = 0x21;
  byte2send[ 1 ] = 0x24;
  status = send_command_LI100( busnumber, byte2send );

  return status;
}
