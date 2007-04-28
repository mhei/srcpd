/***************************************************************************
                         hsi-88.c  -  description
                            -------------------
   begin                : Mon Oct 29 2001
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

#include "stdincludes.h"

#include "config-srcpd.h"
#include "hsi-88.h"
#include "io.h"
#include "srcp-fb.h"

#define __hsi ((HSI_88_DATA*)busses[busnumber].driverdata)

static int working_HSI88;

int readConfig_HSI_88( xmlDocPtr doc, xmlNodePtr node, bus_t busnumber )
{
  int number;

  busses[ busnumber ].driverdata = malloc( sizeof( struct _HSI_88_DATA ) );

    if (busses[busnumber].driverdata == NULL) {
        DBG(busnumber, DBG_ERROR,
                "Memory allocation error in module '%s'.", node->name);
        return 0;
    }

  busses[ busnumber ].type = SERVER_HSI_88;
  busses[ busnumber ].init_func = &init_bus_HSI_88;
  busses[ busnumber ].term_func = &term_bus_HSI_88;
  busses[ busnumber ].thr_func = &thr_sendrec_HSI_88;
  busses[ busnumber ].flags |= FB_ORDER_0;
  busses[ busnumber ].flags |= FB_16_PORTS;
  strcpy( busses[ busnumber ].description, "FB POWER" );
  __hsi->refresh = 10000;
  __hsi->number_fb[ 0 ] = 0;
  __hsi->number_fb[ 1 ] = 0;
  __hsi->number_fb[ 2 ] = 0;

  xmlNodePtr child = node->children;
  xmlChar *txt = NULL;

  while ( child != NULL )
  {
    if ( xmlStrncmp( child->name, BAD_CAST "text", 4 ) == 0 )
    {
      /* just do nothing, it is only a comment */
    }

    else if ( xmlStrcmp( child->name, BAD_CAST "refresh" ) == 0 )
    {
      txt = xmlNodeListGetString( doc, child->xmlChildrenNode, 1 );
      if ( txt != NULL )
      {
        __hsi->refresh = atoi( ( char * ) txt );
        xmlFree( txt );
      }
    }

    else if ( xmlStrcmp( child->name, BAD_CAST "p_time" ) == 0 )
    {
      txt = xmlNodeListGetString( doc, child->xmlChildrenNode, 1 );
      if ( txt != NULL )
      {
        set_min_time( busnumber, atoi( ( char * ) txt ) );
        xmlFree( txt );
      }
    }

    else if ( xmlStrcmp( child->name, BAD_CAST "number_fb_left" ) == 0 )
    {
      txt = xmlNodeListGetString( doc, child->xmlChildrenNode, 1 );
      if ( txt != NULL )
      {
        __hsi->number_fb[ 0 ] = atoi( ( char * ) txt );
        xmlFree( txt );
      }
    }

    else if ( xmlStrcmp( child->name, BAD_CAST "number_fb_center" ) == 0 )
    {
      txt = xmlNodeListGetString( doc, child->xmlChildrenNode, 1 );
      if ( txt != NULL )
      {
        __hsi->number_fb[ 1 ] = atoi( ( char * ) txt );
        xmlFree( txt );
      }
    }

    else if ( xmlStrcmp( child->name, BAD_CAST "number_fb_right" ) == 0 )
    {
      txt = xmlNodeListGetString( doc, child->xmlChildrenNode, 1 );
      if ( txt != NULL )
      {
        __hsi->number_fb[ 2 ] = atoi( ( char * ) txt );
        xmlFree( txt );
      }
    }

    else
      DBG( busnumber, DBG_WARN,
           "WARNING, unknown tag found: \"%s\"!\n",
           child->name );
    ;

    child = child->next;
  }

  // create an array for feedbacks;
  number = __hsi->number_fb[ 0 ];
  number += __hsi->number_fb[ 1 ];
  number += __hsi->number_fb[ 2 ];

  if ( init_FB( busnumber, number * 16 ) )
  {
    __hsi->number_fb[ 0 ] = 0;
    __hsi->number_fb[ 1 ] = 0;
    __hsi->number_fb[ 2 ] = 0;
    DBG( busnumber, DBG_ERROR, "Can't create array for feedback" );
  }

  return ( 1 );
}

static int open_lineHSI88( char *name )
{
  int fd;
  struct termios interface;

  printf( "try opening serial line %s for 9600 baud\n", name );
  fd = open( name, O_RDWR );
  if ( fd == -1 )
  {
    printf( "Sorry, couldn't open device.\n" );
  }
  else
  {
    tcgetattr( fd, &interface );
    interface.c_oflag = ONOCR;
    interface.c_cflag =
      CS8 | CRTSCTS | CSTOPB | CLOCAL | CREAD | HUPCL;
    interface.c_iflag = IGNBRK;
    interface.c_lflag = IEXTEN;
    cfsetispeed( &interface, B9600 );
    cfsetospeed( &interface, B9600 );
    interface.c_cc[ VMIN ] = 0;
    interface.c_cc[ VTIME ] = 1;
    tcsetattr( fd, TCSANOW, &interface );
  }
  return fd;
}

static int init_lineHSI88( bus_t busnumber, int modules_left,
                           int modules_center, int modules_right )
{
  int status;
  int i;
  int ctr;
  unsigned char byte2send;
  unsigned char rr;

  sleep( 1 );
  byte2send = 0x0d;
  for ( i = 0; i < 10; i++ )
    writeByte( busnumber, byte2send, 500 );

  while ( readByte( busnumber, 0, &rr ) == 0 )
  {}

  // HSI-88 initialize
  // switch off terminal-mode
  i = 1;
  while ( i )
  {
    byte2send = 't';
    writeByte( busnumber, byte2send, 0 );
    byte2send = 0x0d;
    writeByte( busnumber, byte2send, 200 );
    rr = 0;
    ctr = 0;
    status = readByte( busnumber, 0, &rr );
    while ( rr != 't' )
    {
      usleep( 100000 );
      status = readByte( busnumber, 0, &rr );
      if ( status == -1 )
        ctr++;
      if ( ctr > 20 )
        return -1;
    }
    readByte( busnumber, 0, &rr );
    if ( rr == '0' )
      i = 0;
    readByte( busnumber, 0, &rr );
  }
  // looking for version of HSI
  byte2send = 'v';
  writeByte( busnumber, byte2send, 0 );
  byte2send = 0x0d;
  writeByte( busnumber, byte2send, 500 );

  for ( i = 0; i < 49; i++ )
  {
    status = readByte( busnumber, 1, &rr );
    if ( status == -1 )
      break;
    __hsi->v_text[ i ] = ( char ) rr;
  }
  __hsi->v_text[ i ] = 0x00;
  printf( "%s\n", __hsi->v_text );

  status = 1;
  while ( status )
  {
    // initialise module-numbers
    // up to "GO", non feedback-module
    byte2send = 's';
    writeByte( busnumber, byte2send, 0 );
    byte2send = 0;
    writeByte( busnumber, byte2send, 0 );
    writeByte( busnumber, byte2send, 0 );
    writeByte( busnumber, byte2send, 0 );
    byte2send = 0x0d;
    writeByte( busnumber, byte2send, 0 );
    byte2send = 0x0d;
    writeByte( busnumber, byte2send, 0 );
    byte2send = 0x0d;
    writeByte( busnumber, byte2send, 0 );
    byte2send = 0x0d;
    writeByte( busnumber, byte2send, 5 );

    rr = 0;
    readByte( busnumber, 1, &rr );    // read answer (three bytes)
    while ( rr != 's' )
    {
      usleep( 100000 );
      readByte( busnumber, 0, &rr );
    }
    readByte( busnumber, 0, &rr );    // number of given modules
    status = 0;
  }
  return 0;
}

int init_bus_HSI_88(bus_t busnumber )
{
  int fd;
  int status;
  int anzahl;

  status = 0;
  printf( "Bus %ld with debuglevel %d\n", busnumber,
          busses[ busnumber ].debuglevel );
  if ( busses[ busnumber ].type != SERVER_HSI_88 )
  {
    status = -2;
  }
  else
  {
    if ( busses[ busnumber ].fd > 0 )
      status = -3;        // bus is already in use
  }

  if ( status == 0 )
  {
    working_HSI88 = 0;
    anzahl = __hsi->number_fb[ 0 ];
    anzahl += __hsi->number_fb[ 1 ];
    anzahl += __hsi->number_fb[ 2 ];

    if ( anzahl > 31 )
    {
      printf( "Number of feedback-modules greater than 31!" );
      status = -4;
    }
  }

  if ( busses[ busnumber ].debuglevel < 7 )
  {
    if ( status == 0 )
    {
      fd = open_lineHSI88( busses[ busnumber ].filename.path );
      if ( fd > 0 )
      {
        busses[ busnumber ].fd = fd;
        status = init_lineHSI88( busnumber, __hsi->number_fb[ 0 ],
                                 __hsi->number_fb[ 1 ],
                                 __hsi->number_fb[ 2 ] );
      }
      else
        status = -5;
    }
  }
  else
    busses[ busnumber ].fd = 9999;
  if ( status == 0 )
    working_HSI88 = 1;

  printf( "INIT_BUS_HSI with code: %d\n", status );
  return status;
}

int term_bus_HSI_88(bus_t busnumber )
{
  if ( busses[ busnumber ].type != SERVER_HSI_88 )
    return 1;

  if ( busses[ busnumber ].pid == 0 )
    return 0;

  working_HSI88 = 0;

  pthread_cancel( busses[ busnumber ].pid );
  busses[ busnumber ].pid = 0;
  close_comport( busnumber );
  return 0;
}

void *thr_sendrec_HSI_88( void *v )
{
  bus_t busnumber;
  int refresh_time;
  int anzahl, i, temp;
  unsigned char byte2send;
  unsigned char rr;
  int status;
  int zaehler1, fb_zaehler1, fb_zaehler2;

  busnumber = ( bus_t ) v;
  refresh_time = __hsi->refresh;
  DBG( busnumber, DBG_INFO, "thr_sendrec_HSI_88 is started" );

  zaehler1 = 0;
  fb_zaehler1 = 0;
  fb_zaehler2 = 1;
  i = 0;
  temp = 1;
  if ( busses[ busnumber ].debuglevel <= DBG_DEBUG )
  {
    status = 1;
    while ( status )
    {
      // Modulbelegung initialisieren
      byte2send = 's';
      writeByte( busnumber, byte2send, 0 );
      byte2send = __hsi->number_fb[ 0 ];
      writeByte( busnumber, byte2send, 0 );
      byte2send = __hsi->number_fb[ 1 ];
      writeByte( busnumber, byte2send, 0 );
      byte2send = __hsi->number_fb[ 2 ];
      writeByte( busnumber, byte2send, 0 );
      byte2send = 0x0d;
      writeByte( busnumber, byte2send, 0 );
      byte2send = 0x0d;
      writeByte( busnumber, byte2send, 0 );
      byte2send = 0x0d;
      writeByte( busnumber, byte2send, 0 );
      byte2send = 0x0d;
      writeByte( busnumber, byte2send, 200 );

      rr = 0;
      readByte( busnumber, 0, &rr );        // read answer (three bytes)
      while ( rr != 's' )
      {
        usleep( 100000 );
        readByte( busnumber, 0, &rr );
      }
      readByte( busnumber, 0, &rr );        // number of given modules
      anzahl = ( int ) rr;
      DBG( busnumber, DBG_INFO, "number of modules: %i", anzahl );
      anzahl -= __hsi->number_fb[ 0 ];
      anzahl -= __hsi->number_fb[ 1 ];
      anzahl -= __hsi->number_fb[ 2 ];
      if ( anzahl == 0 )     // HSI initialisation correct ?
      {
        status = 0;
      }
      else
      {
        DBG( busnumber, DBG_ERROR, "error while initialising" );
        sleep( 1 );
        while ( readByte( busnumber, 0, &rr ) == 0 )
        {}
      }
    }
  }

  while ( 1 )
  {
    if ( busses[ busnumber ].debuglevel <= DBG_DEBUG )
    {
      rr = 0;
      while ( rr != 'i' )
      {
        /* first check here for reset of feedbacks
           (do this check at the end, we will not run, until
           get new changes from HSI) */
        check_reset_fb( busnumber );
        usleep( refresh_time );
        readByte( busnumber, 0, &rr );
      }
      readByte( busnumber, 1, &rr );        // number of given modules
      anzahl = ( int ) rr;
      for ( zaehler1 = 0; zaehler1 < anzahl; zaehler1++ )
      {
        readByte( busnumber, 1, &rr );
        i = rr;
        readByte( busnumber, 1, &rr );
        temp = rr;
        temp <<= 8;
        readByte( busnumber, 1, &rr );
        setFBmodul( busnumber, i, temp | rr );
        DBG( busnumber, DBG_DEBUG, "feedback %i with 0x%04x", i,
             temp | rr );
      }
      readByte( busnumber, 1, &rr );        // <CR>
    }
    else
    {                                       // only for testing
      setFBmodul( busnumber, 1, temp );
      i++;
      temp <<= 1;
      if ( i > 16 )
      {
        i = 0;
        temp = 1;
      }
      sleep( 2 );
    }
  }
}
