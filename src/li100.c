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

#ifdef LI100_USB
static int readAnswer_LI100_USB( long int busnumber, unsigned char *str );
static int initLine_LI100_USB( long int busnumber );
#else
static int readAnswer_LI100_SERIAL( long int busnumber, unsigned char *str );
static int initLine_LI100_SERIAL( long int busnumber );
#endif

#ifdef LI100_USB
int readConfig_LI100_USB( xmlDocPtr doc, xmlNodePtr node, long int busnumber )
#else
int readConfig_LI100_SERIAL( xmlDocPtr doc, xmlNodePtr node, long int busnumber )
#endif
{
#ifdef LI100_USB
  DBG( busnumber, DBG_INFO, "reading configuration for LI100 (usb) at bus #%ld", busnumber );
#else

  DBG( busnumber, DBG_INFO, "reading configuration for LI100 (serial) at bus #%ld", busnumber );
#endif

#ifdef LI100_USB

  busses[ busnumber ].type = SERVER_LI100_USB;
  busses[ busnumber ].init_func = &init_bus_LI100_USB;
  busses[ busnumber ].term_func = &term_bus_LI100_USB;
  busses[ busnumber ].thr_func = &thr_sendrec_LI100_USB;
  busses[ busnumber ].baudrate = B57600;
#else

  busses[ busnumber ].type = SERVER_LI100_SERIAL;
  busses[ busnumber ].init_func = &init_bus_LI100_SERIAL;
  busses[ busnumber ].term_func = &term_bus_LI100_SERIAL;
  busses[ busnumber ].thr_func = &thr_sendrec_LI100_SERIAL;
  busses[ busnumber ].baudrate = B9600;
#endif

  busses[ busnumber ].init_gl_func = &init_gl_LI100;
  busses[ busnumber ].init_ga_func = &init_ga_LI100;
  busses[ busnumber ].driverdata = malloc( sizeof( struct _LI100_DATA ) );
  busses[ busnumber ].flags |= FB_4_PORTS;

  strcpy( busses[ busnumber ].description,
          "GA GL FB SM POWER LOCK DESCRIPTION" );
#ifdef LI100_USB

  __li100->number_fb = 1024;
  __li100->number_ga = 1024;
  __li100->number_gl = 9999;
#else

  __li100->number_fb = 256;
  __li100->number_ga = 256;
  __li100->number_gl = 99;
#endif

  xmlNodePtr child = node->children;
  xmlChar *txt = NULL;

  while ( child != NULL )
  {
    if ( xmlStrcmp( child->name, BAD_CAST "number_fb" ) == 0 )
    {
      txt = xmlNodeListGetString( doc, child->xmlChildrenNode, 1 );
      if ( txt != NULL )
      {
        __li100->number_fb = atoi( ( char * ) txt );
        xmlFree( txt );
      }
    }

    else if ( xmlStrcmp( child->name, BAD_CAST "number_gl" ) == 0 )
    {
      txt = xmlNodeListGetString( doc, child->xmlChildrenNode, 1 );
      if ( txt != NULL )
      {
        __li100->number_gl = atoi( ( char * ) txt );
        xmlFree( txt );
      }
    }

    else if ( xmlStrcmp( child->name, BAD_CAST "number_ga" ) == 0 )
    {
      txt = xmlNodeListGetString( doc, child->xmlChildrenNode, 1 );
      if ( txt != NULL )
      {
        __li100->number_ga = atoi( ( char * ) txt );
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

    else
      DBG( busnumber, DBG_WARN,
           "WARNING, unknown tag found: \"%s\"!\n",
           child->name );
    ;

    child = child->next;
  }
  return ( 1 );
}

#ifdef LI100_USB
long int init_bus_LI100_USB( long int busnumber )
#else
long int init_bus_LI100_SERIAL( long int busnumber )
#endif
{
  int status;
  int i;

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
  DBG( busnumber, DBG_WARN, "debug array for locomotivs" );
  if ( init_FB( busnumber, __li100->number_fb * 16 ) )
  {
    __li100->number_fb = 0;
    DBG( busnumber, DBG_ERROR, "Can't create array for feedback" );
  }

  status = 0;
  printf( "Bus #%ld with debuglevel %d\n", busnumber,
          busses[ busnumber ].debuglevel );
#ifdef LI100_USB

  if ( busses[ busnumber ].type != SERVER_LI100_USB )
#else

  if ( busses[ busnumber ].type != SERVER_LI100_SERIAL )
#endif

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
    __li100->working_LI100 = 0;
  }

  if ( busses[ busnumber ].debuglevel < 7 )
  {
    if ( status == 0 )
#ifdef LI100_USB

      status = initLine_LI100_USB( busnumber );
#else

      status = initLine_LI100_SERIAL( busnumber );
#endif

  }
  else
    busses[ busnumber ].fd = 9999;
  if ( status == 0 )
  {
    __li100->working_LI100 = 1;
    printf( "Version LENZ-Interface : %d.%d\n", __li100->version_interface / 256, __li100->version_interface % 256 );
    printf( "Code LENZ-Interface    : %d%d\n", __li100->code_interface / 16, __li100->code_interface % 16 );
    printf( "Version LENZ-Centrale  : %d.%d\n", __li100->version_zentrale / 256, __li100->version_zentrale % 256 );
    //     printf("Code LENZ-Centrale     : %d",__li100->code_zentrale);

    __li100->get_addr = 0;
    // if version of centrale is greather than 3.0, cleanup stack
    if ( __li100->version_zentrale >= 0x0300 )
    {
      unsigned char byte2send[ 20 ];
      for ( ;; )
      {
        byte2send[ 0 ] = 0xe3;
        byte2send[ 1 ] = 0x05;
        byte2send[ 2 ] = 0;
        byte2send[ 3 ] = 0;

#ifdef LI100_USB

        send_command_LI100_USB( busnumber, byte2send );
#else

        send_command_LI100_SERIAL( busnumber, byte2send );
#endif

        if ( __li100->get_addr == 0 )
          break;
        printf( "remove engine with address %d from stack\n", __li100->get_addr & 0x3fff );
        byte2send[ 0 ] = 0xe3;
        byte2send[ 1 ] = 0x44;
        byte2send[ 2 ] = ( unsigned char ) ( __li100->get_addr >> 8 );
        byte2send[ 3 ] = ( unsigned char ) ( __li100->get_addr & 0x00FF );
#ifdef LI100_USB

        send_command_LI100_USB( busnumber, byte2send );
#else

        send_command_LI100_SERIAL( busnumber, byte2send );
#endif

      }
    }
  }

#ifdef LI100_USB

  printf( "INIT_BUS_LI100 (usb) exited with code: %d\n", status );
#else

  printf( "INIT_BUS_LI100 (serial) exited with code: %d\n", status );
#endif

  __li100->last_type = -1;
  __li100->last_value = -1;
  __li100->emergency_on_LI100 = 0;
  __li100->extern_engine_ctr = 0;
  for ( i = 0; i < 100; i++ )
    __li100->extern_engine[ i ] = -1;
  __li100->pgm_mode = 0;

  return status;
}

#ifdef LI100_USB
long int term_bus_LI100_USB( long int busnumber )
#else
long int term_bus_LI100_SERIAL( long int busnumber )
#endif
{
#ifdef LI100_USB
  if ( busses[ busnumber ].type != SERVER_LI100_USB )
#else

  if ( busses[ busnumber ].type != SERVER_LI100_SERIAL )
#endif

    return 1;

  if ( busses[ busnumber ].pid == 0 )
    return 0;

  __li100->working_LI100 = 0;

  pthread_cancel( busses[ busnumber ].pid );
  busses[ busnumber ].pid = 0;
  close_comport( busnumber );
  return 0;
}

#ifdef LI100_USB
void *thr_sendrec_LI100_USB( void *v )
#else
void *thr_sendrec_LI100_SERIAL( void *v )
#endif
{
  long int busnumber;
  unsigned char byte2send[ 20 ];
  int status;

  int zaehler1, fb_zaehler1, fb_zaehler2;

  busnumber = ( long int ) v;
#ifdef LI100_USB

  DBG( busnumber, DBG_INFO, "thr_sendrec_LI100 (usb) is startet as bus #%ld", busnumber );
#else

  DBG( busnumber, DBG_INFO, "thr_sendrec_LI100 (serial) is startet as bus #%ld", busnumber );
#endif

  // initialize tga-structure
  for ( zaehler1 = 0; zaehler1 < 50; zaehler1++ )
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
#ifdef LI100_USB

      status = send_command_LI100_USB( busnumber, byte2send );
#else

      status = send_command_LI100_SERIAL( busnumber, byte2send );
#endif

      if ( status == 0 )                                    // war alles OK ?
        busses[ busnumber ].power_changed = 0;
    }

#ifdef LI100_USB
    send_command_gl_LI100_USB( busnumber );
    send_command_ga_LI100_USB( busnumber );
    check_status_LI100_USB( busnumber );
    check_extern_engines_USB( busnumber );
    send_command_sm_LI100_USB( busnumber );
#else

    send_command_gl_LI100_SERIAL( busnumber );
    send_command_ga_LI100_SERIAL( busnumber );
    check_status_LI100_SERIAL( busnumber );
    check_extern_engines_SERIAL( busnumber );
    send_command_sm_LI100_SERIAL( busnumber );
#endif

    check_reset_fb( busnumber );
    busses[ busnumber ].watchdog = 1;
    usleep( 50000 );
  }                           // Ende WHILE(1)
}

#ifdef LI100_USB
void send_command_ga_LI100_USB( long int busnumber )
#else
void send_command_ga_LI100_SERIAL( long int busnumber )
#endif
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
  for ( i = 0; i < 50; i++ )
  {
    if ( __li100->tga[ i ].id )
    {
      syslog( LOG_INFO, "time %i,%i", ( int ) akt_time.tv_sec,
              ( int ) akt_time.tv_usec );
      cmp_time = __li100->tga[ i ].t;
      // Ausschaltzeitpunkt erreicht?
      if ( cmpTime( &cmp_time, &akt_time ) )
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
#ifdef LI100_USB
        send_command_LI100_USB( busnumber, byte2send );
#else

        send_command_LI100_SERIAL( busnumber, byte2send );
#endif

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
#ifdef LI100_USB
    send_command_LI100_USB( busnumber, byte2send );
#else

    send_command_LI100_SERIAL( busnumber, byte2send );
#endif

    status = 1;
    if ( gatmp.action && ( gatmp.activetime > 0 ) )
    {
      for ( i1 = 0; i1 < 50; i1++ )
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
          __li100->tga[ i1 ] = gatmp;
          status = 0;
          DBG( busnumber, DBG_DEBUG,
               "GA %i for switch off at %i,%i on %i",
               __li100->tga[ i1 ].id,
               ( int ) __li100->tga[ i1 ].t.tv_sec,
               ( int ) __li100->tga[ i1 ].t.tv_usec, i1 );
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

#ifdef LI100_USB
void send_command_gl_LI100_USB( long int busnumber )
#else
void send_command_gl_LI100_SERIAL( long int busnumber )
#endif
{
  int temp;
  int addr;
  unsigned char byte2send[ 20 ];
  int status = -1;
  struct _GLSTATE gltmp, glakt;

  /* Lokdecoder */
  //fprintf(stderr, "LOK's... ");
  /* nur senden, wenn wirklich etwas vorliegt */
  if ( !queue_GL_isempty( busnumber ) )
  {
    unqueueNextGL( busnumber, &gltmp );
    addr = gltmp.id;
    getGL( busnumber, addr, &glakt );

    // speed, direction or function changed?
    if ( ( gltmp.direction != glakt.direction ) ||
         ( gltmp.speed != glakt.speed ) || ( gltmp.funcs != glakt.funcs ) )
    {
      // Lokkommando soll gesendet werden
      // emergency stop for one locomotive
      if ( gltmp.direction == 2 )
      {
        if ( __li100->version_zentrale >= 0x0300 )
        {
          byte2send[ 0 ] = 0x92;
          byte2send[ 1 ] = addr >> 8;
          if ( addr > 99 )
            byte2send[ 1 ] |= 0xc0;
          byte2send[ 2 ] = addr & 0xff;
        }
        else
        {
          byte2send[ 0 ] = 0x91;
          byte2send[ 1 ] = addr;
        }
#ifdef LI100_USB
        status = send_command_LI100_USB( busnumber, byte2send );
#else

        status = send_command_LI100_SERIAL( busnumber, byte2send );
#endif

      }
      else
      {
#ifndef LI100_USB
        if ( __li100->version_zentrale <= 0x0150 )
        {
          byte2send[ 0 ] = 0xb3;
          // address
          byte2send[ 1 ] = gltmp.id;
          // setting direction and speed
          byte2send[ 2 ] = gltmp.speed;
          if ( gltmp.speed > 0 )
            byte2send[ 2 ] ++;
          if ( gltmp.direction )
          {
            byte2send[ 2 ] |= 0x40;
          }
          if ( gltmp.funcs & 1 )
          {
            byte2send[ 2 ] |= 0x20;
          }
          // functions f1..f4
          byte2send[ 3 ] = ( gltmp.funcs >> 1 ) & 0x000F;
          status = send_command_LI100_SERIAL( busnumber, byte2send );
        }
        if ( ( __li100->version_zentrale > 0x0150 ) &&
             ( __li100->version_zentrale < 0x0300 ) )
        {
          byte2send[ 0 ] = 0xb4;
          // address
          byte2send[ 1 ] = gltmp.id;
          // mode
          switch ( gltmp.n_fs )
          {
            case 14:
              byte2send[ 4 ] = 0x00;
              // setting direction and speed
              byte2send[ 2 ] = gltmp.speed;
              if ( gltmp.speed > 0 )
                byte2send[ 2 ] ++;
              if ( gltmp.direction )
              {
                byte2send[ 2 ] |= 0x40;
              }
              if ( gltmp.funcs & 1 )
              {
                byte2send[ 2 ] |= 0x20;
              }
              break;
            case 27:
              byte2send[ 4 ] = 0x01;
              // setting direction and speed
              byte2send[ 2 ] = gltmp.speed;
              if ( gltmp.speed > 0 )
                byte2send[ 2 ] += 3;
              if ( byte2send[ 2 ] & 0x01 )
                byte2send[ 2 ] |= 0x20;
              byte2send[ 2 ] >>= 1;
              if ( gltmp.direction )
              {
                byte2send[ 2 ] |= 0x40;
              }
              if ( gltmp.funcs & 1 )
              {
                byte2send[ 2 ] |= 0x20;
              }
              break;
            case 28:
              byte2send[ 4 ] = 0x02;
              // setting direction and speed
              byte2send[ 2 ] = gltmp.speed;
              if ( gltmp.speed > 0 )
                byte2send[ 2 ] += 3;
              if ( byte2send[ 2 ] & 0x01 )
                byte2send[ 2 ] |= 0x20;
              byte2send[ 2 ] >>= 1;
              if ( gltmp.direction )
              {
                byte2send[ 2 ] |= 0x40;
              }
              if ( gltmp.funcs & 1 )
              {
                byte2send[ 2 ] |= 0x20;
              }
              break;
            default:
              byte2send[ 4 ] = 0x00;
              // setting direction and speed
              byte2send[ 2 ] = gltmp.speed;
              if ( gltmp.speed > 0 )
                byte2send[ 2 ] ++;
              if ( gltmp.direction )
              {
                byte2send[ 2 ] |= 0x40;
              }
              if ( gltmp.funcs & 1 )
              {
                byte2send[ 2 ] |= 0x20;
              }
          }
          // functions f1..f4
          byte2send[ 3 ] = ( gltmp.funcs >> 1 ) & 0x000F;
          status = send_command_LI100_SERIAL( busnumber, byte2send );
        }
        if ( __li100->version_zentrale >= 0x0300 )
        {
#endif
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
          if ( ( gltmp.n_fs == 27 ) || ( gltmp.n_fs = 28 ) )
          {
            if ( gltmp.speed > 0 )
              byte2send[ 4 ] += 3;
            if ( byte2send[ 4 ] & 0x01 )
              byte2send[ 4 ] |= 0x20;
            byte2send[ 4 ] >>= 1;
          }
          else
          {
            if ( gltmp.speed > 0 )
              byte2send[ 4 ] ++;
          }
          if ( gltmp.direction )
          {
            byte2send[ 4 ] |= 0x80;
          }
#ifdef LI100_USB
          status = send_command_LI100_USB( busnumber, byte2send );
#else

          status = send_command_LI100_SERIAL( busnumber, byte2send );
#endif

          // send functions f0..f4
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
#ifdef LI100_USB
          status = send_command_LI100_USB( busnumber, byte2send );
#else

          status = send_command_LI100_SERIAL( busnumber, byte2send );
        }
#endif

        }
        if ( status == 0 )
        {
          setGL( busnumber, addr, gltmp );
        }
      }
    }
  }

#ifdef LI100_USB
  void check_extern_engines_USB( long int busnumber )
#else
  void check_extern_engines_SERIAL( long int busnumber )
#endif
  {
    int i;
    int tmp_addr;
    unsigned char byte2send[ 20 ];
    struct _GLSTATE gltmp;

    if ( __li100->extern_engine_ctr > 0 )
    {
      for ( i = 0; i < 100; i++ )
      {
        tmp_addr = __li100->extern_engine[ i ];
        if ( tmp_addr != -1 )
        {
          if ( __li100->version_zentrale <= 0x0150 )
          {
            __li100->last_value = tmp_addr;
            byte2send[ 0 ] = 0xa1;
            // address
            byte2send[ 1 ] = tmp_addr;
#ifdef LI100_USB

            send_command_LI100_USB( busnumber, byte2send );
#else

            send_command_LI100_SERIAL( busnumber, byte2send );
#endif

          }
          if ( ( __li100->version_zentrale > 0x0150 ) &&
               ( __li100->version_zentrale < 0x0300 ) )
          {
            __li100->last_value = tmp_addr;
            getGL( busnumber, tmp_addr, &gltmp );
            byte2send[ 0 ] = 0xa2;
            // address
            byte2send[ 1 ] = tmp_addr;
            // mode
            switch ( gltmp.n_fs )
            {
              case 14:
                byte2send[ 2 ] = 0x00;
                break;
              case 27:
                byte2send[ 2 ] = 0x01;
                break;
              case 28:
                byte2send[ 2 ] = 0x02;
                break;
              default:
                byte2send[ 2 ] = 0x02;
            }
#ifdef LI100_USB
            send_command_LI100_USB( busnumber, byte2send );
#else

            send_command_LI100_SERIAL( busnumber, byte2send );
#endif

          }
          if ( __li100->version_zentrale >= 0x0300 )
          {
            __li100->last_value = tmp_addr;
            byte2send[ 0 ] = 0xe3;
            byte2send[ 1 ] = 0x00;
            byte2send[ 2 ] = ( tmp_addr & 0xff00 ) >> 8;
            byte2send[ 3 ] = tmp_addr & 0x00ff;
#ifdef LI100_USB

            send_command_LI100_USB( busnumber, byte2send );
            check_status_LI100_USB( busnumber );
#else

            send_command_LI100_SERIAL( busnumber, byte2send );
            check_status_LI100_SERIAL( busnumber );
#endif

          }
        }
      }
    }
    __li100->last_value = -1;
  }

#ifdef LI100_USB
  int read_register_LI100_USB( long int busnumber, int reg )
#else
  int read_register_LI100_SERIAL( long int busnumber, int reg )
#endif
  {
    unsigned char byte2send[ 20 ];
    unsigned char status;

    byte2send[ 0 ] = 0x22;
    byte2send[ 1 ] = 0x11;
    byte2send[ 2 ] = reg;

#ifdef LI100_USB

    status = send_command_LI100_USB( busnumber, byte2send );
    get_status_sm_LI100_USB( busnumber );
#else

    status = send_command_LI100_SERIAL( busnumber, byte2send );
    get_status_sm_LI100_SERIAL( busnumber );
#endif

    return status;
  }

#ifdef LI100_USB
  int write_register_LI100_USB( long int busnumber, int reg, int value )
#else
  int write_register_LI100_SERIAL( long int busnumber, int reg, int value )
#endif
  {
    unsigned char byte2send[ 20 ];
    unsigned char status;

    byte2send[ 0 ] = 0x23;
    byte2send[ 1 ] = 0x12;
    byte2send[ 2 ] = reg;
    byte2send[ 3 ] = value;

#ifdef LI100_USB

    status = send_command_LI100_USB( busnumber, byte2send );
#else

    status = send_command_LI100_SERIAL( busnumber, byte2send );
#endif

    return status;
  }

#ifdef LI100_USB
  int read_page_LI100_USB( long int busnumber, int cv )
#else
  int read_page_LI100_SERIAL( long int busnumber, int cv )
#endif
  {
    unsigned char byte2send[ 20 ];
    unsigned char status;

    byte2send[ 0 ] = 0x22;
    byte2send[ 1 ] = 0x14;
    byte2send[ 2 ] = cv;

#ifdef LI100_USB

    status = send_command_LI100_USB( busnumber, byte2send );
    get_status_sm_LI100_USB( busnumber );
#else

    status = send_command_LI100_SERIAL( busnumber, byte2send );
    get_status_sm_LI100_SERIAL( busnumber );
#endif

    return status;
  }

#ifdef LI100_USB
  int write_page_LI100_USB( long int busnumber, int cv, int value )
#else
  int write_page_LI100_SERIAL( long int busnumber, int cv, int value )
#endif
  {
    unsigned char byte2send[ 20 ];
    unsigned char status;

    byte2send[ 0 ] = 0x23;
    byte2send[ 1 ] = 0x17;
    byte2send[ 2 ] = cv;
    byte2send[ 3 ] = value;

#ifdef LI100_USB

    status = send_command_LI100_USB( busnumber, byte2send );
#else

    status = send_command_LI100_SERIAL( busnumber, byte2send );
#endif

    return status;
  }

#ifdef LI100_USB
  int read_cv_LI100_USB( long int busnumber, int cv )
#else
  int read_cv_LI100_SERIAL( long int busnumber, int cv )
#endif
  {
    unsigned char byte2send[ 20 ];
    unsigned char status;

    byte2send[ 0 ] = 0x22;
    byte2send[ 1 ] = 0x15;
    byte2send[ 2 ] = cv;

#ifdef LI100_USB

    status = send_command_LI100_USB( busnumber, byte2send );
    get_status_sm_LI100_USB( busnumber );
#else

    status = send_command_LI100_SERIAL( busnumber, byte2send );
    get_status_sm_LI100_SERIAL( busnumber );
#endif

    return status;
  }

#ifdef LI100_USB
  int write_cv_LI100_USB( long int busnumber, int cv, int value )
#else
  int write_cv_LI100_SERIAL( long int busnumber, int cv, int value )
#endif
  {
    unsigned char byte2send[ 20 ];
    unsigned char status;

    byte2send[ 0 ] = 0x23;
    byte2send[ 1 ] = 0x16;
    byte2send[ 2 ] = cv;
    byte2send[ 3 ] = value;

#ifdef LI100_USB

    status = send_command_LI100_USB( busnumber, byte2send );
#else

    status = send_command_LI100_SERIAL( busnumber, byte2send );
#endif

    return status;
  }

  // program decoder on the main
#ifdef LI100_USB
  int send_pom_cv_LI100_USB( long int busnumber, int addr, int cv, int value )
#else
  int send_pom_cv_LI100_SERIAL( long int busnumber, int addr, int cv, int value )
#endif
  {
    unsigned char byte2send[ 20 ];
    unsigned char status;
    int ret_val;
    int tmp;

    cv--;

    // send pom-command
    byte2send[ 0 ] = 0xE6;
    byte2send[ 1 ] = 0x30;
    // high-byte of decoder-adress
    tmp = addr >> 8;
    byte2send[ 2 ] = tmp;
    // low-byte of decoder-adress
    tmp = addr & 0xFF;
    byte2send[ 3 ] = tmp;
    tmp = 0x7C | ( ( cv >> 8 ) & 0x03 );
    byte2send[ 4 ] = tmp;
    byte2send[ 5 ] = cv & 0xff;
    byte2send[ 6 ] = value;

#ifdef LI100_USB

    status = send_command_LI100_USB( busnumber, byte2send );
#else

    status = send_command_LI100_SERIAL( busnumber, byte2send );
#endif

    ret_val = 0;
    if ( status != 0 )
      ret_val = -1;
    return ret_val;
  }

  // program decoder on the main
#ifdef LI100_USB
  int send_pom_cvbit_LI100_USB( long int busnumber, int addr, int cv, int cvbit, int value )
#else
  int send_pom_cvbit_LI100_SERIAL( long int busnumber, int addr, int cv, int cvbit, int value )
#endif
  {
    unsigned char byte2send[ 20 ];
    unsigned char status;
    int ret_val;
    int tmp;

    cv--;

    // send pom-command
    byte2send[ 0 ] = 0xE6;
    byte2send[ 1 ] = 0x30;
    // high-byte of decoder-adress
    tmp = addr >> 8;
    byte2send[ 2 ] = tmp;
    // low-byte of decoder-adress
    tmp = addr & 0xFF;
    byte2send[ 3 ] = tmp;
    tmp = 0x7C | ( ( cv >> 8 ) & 0x03 );
    byte2send[ 4 ] = tmp;
    byte2send[ 5 ] = cv & 0xff;
    byte2send[ 6 ] = cvbit;
    if ( value )
      byte2send[ 6 ] |= 0x08;

#ifdef LI100_USB

    status = send_command_LI100_USB( busnumber, byte2send );
#else

    status = send_command_LI100_SERIAL( busnumber, byte2send );
#endif

    ret_val = 0;
    if ( status != 0 )
      ret_val = -1;
    return ret_val;
  }

#ifdef LI100_USB
  int term_pgm_LI100_USB( long int busnumber )
#else
  int term_pgm_LI100_SERIAL( long int busnumber )
#endif
  {
    unsigned char byte2send[ 20 ];
    unsigned char status;

    // send command "turn all on"
    byte2send[ 0 ] = 0x21;
    byte2send[ 1 ] = 0x81;

#ifdef LI100_USB

    status = send_command_LI100_USB( busnumber, byte2send );
#else

    status = send_command_LI100_SERIAL( busnumber, byte2send );
#endif

    return status;
  }

#ifdef LI100_USB
  void send_command_sm_LI100_USB( long int busnumber )
#else
  void send_command_sm_LI100_SERIAL( long int busnumber )
#endif
  {
    //unsigned char byte2send;
    //unsigned char status;
    struct _SM smakt;

    /* Lokdecoder */
    //fprintf(stderr, "LOK's... ");
    /* nur senden, wenn wirklich etwas vorliegt */
    if ( !queue_SM_isempty( busnumber ) )
    {
      unqueueNextSM( busnumber, &smakt );

      __li100->last_type = smakt.type;
      __li100->last_typeaddr = smakt.typeaddr;
      __li100->last_bit = smakt.bit;
      __li100->last_value = smakt.value;

      DBG( busnumber, DBG_DEBUG, "in send_command_sm: last_type[%d] = %d",
           busnumber, __li100->last_type );
      switch ( smakt.command )
      {
        case SET:
          if ( smakt.addr == -1 )
          {
            switch ( smakt.type )
            {
              case REGISTER:
#ifdef LI100_USB

                write_register_LI100_USB( busnumber, smakt.typeaddr, smakt.value );
#else

                write_register_LI100_SERIAL( busnumber, smakt.typeaddr, smakt.value );
#endif

                break;
              case CV:
#ifdef LI100_USB

                write_cv_LI100_USB( busnumber, smakt.typeaddr, smakt.value );
#else

                write_cv_LI100_SERIAL( busnumber, smakt.typeaddr, smakt.value );
#endif

                break;
                /*        case CV_BIT:
                   write_cvbit( busnumber, smakt.typeaddr, smakt.bit, smakt.value );
                   break; */
              case PAGE:
#ifdef LI100_USB

                write_page_LI100_USB( busnumber, smakt.typeaddr, smakt.value );
#else

                write_page_LI100_SERIAL( busnumber, smakt.typeaddr, smakt.value );
#endif

            }
          }
          else
          {
            switch ( smakt.type )
            {
              case CV:
#ifdef LI100_USB

                send_pom_cv_LI100_USB( busnumber, smakt.addr, smakt.typeaddr, smakt.value );
#else

                send_pom_cv_LI100_SERIAL( busnumber, smakt.addr, smakt.typeaddr, smakt.value );
#endif

                break;
              case CV_BIT:
#ifdef LI100_USB

                send_pom_cvbit_LI100_USB( busnumber, smakt.addr, smakt.typeaddr, smakt.bit, smakt.value );
#else

                send_pom_cvbit_LI100_SERIAL( busnumber, smakt.addr, smakt.typeaddr, smakt.bit, smakt.value );
#endif

                break;
            }
          }
          break;
        case GET:
          switch ( smakt.type )
          {
            case REGISTER:
#ifdef LI100_USB

              read_register_LI100_USB( busnumber, smakt.typeaddr );
#else

              read_register_LI100_SERIAL( busnumber, smakt.typeaddr );
#endif

              break;
            case CV:
#ifdef LI100_USB

              read_cv_LI100_USB( busnumber, smakt.typeaddr );
#else

              read_cv_LI100_SERIAL( busnumber, smakt.typeaddr );
#endif

              break;
              /*      case CV_BIT:
                 read_cvbit( busnumber, smakt.typeaddr, smakt.bit );
                 break; */
            case PAGE:
#ifdef LI100_USB

              read_page_LI100_USB( busnumber, smakt.typeaddr );
#else

              read_page_LI100_SERIAL( busnumber, smakt.typeaddr );
#endif

          }
          break;
        case VERIFY:
          break;
        case INIT:
          break;
        case TERM:
          DBG( busnumber, DBG_DEBUG,
               "on bus %i pgm_mode is %i", busnumber, __li100->pgm_mode );

          if ( __li100->pgm_mode == 1 )
          {
#ifdef LI100_USB

            term_pgm_LI100_USB( busnumber );
#else

            term_pgm_LI100_SERIAL( busnumber );
#endif

          }
          break;
      }
    }
  }

#ifdef LI100_USB
  void get_status_sm_LI100_USB( long int busnumber )
#else
  void get_status_sm_LI100_SERIAL( long int busnumber )
#endif
  {
    unsigned char byte2send[ 20 ];

    byte2send[ 0 ] = 0x21;
    byte2send[ 1 ] = 0x10;

#ifdef LI100_USB

    send_command_LI100_USB( busnumber, byte2send );
#else

    send_command_LI100_SERIAL( busnumber, byte2send );
#endif

  }

#ifdef LI100_USB
  void check_status_LI100_USB( long int busnumber )
#else
  void check_status_LI100_SERIAL( long int busnumber )
#endif
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
          DBG( busnumber, DBG_ERROR,
               "readbyte(): IOCTL   status: %d with errno = %d: %s",
               status, errno, msg );
        }
        DBG( busnumber, DBG_DEBUG,
             "readbyte(): (fd = %d), there are %d bytes to read.",
             busses[ busnumber ].fd, i );
        // read only, if there is really an input
        if ( i > 0 )
#ifdef LI100_USB

          status = readAnswer_LI100_USB( busnumber, str );
#else

          status = readAnswer_LI100_SERIAL( busnumber, str );
#endif

      }
    }
  }

#ifdef LI100_USB
  int send_command_LI100_USB( long int busnumber, unsigned char *str )
#else
  int send_command_LI100_SERIAL( long int busnumber, unsigned char *str )
#endif
  {
    int ctr, i;
    int status;

#ifdef LI100_USB
    // header for LI100_USB
    writeByte( busnumber, 0xff, 0 );
    writeByte( busnumber, 0xfe, 0 );
#endif

    str[ 19 ] = 0x00;             // control-byte for xor
    ctr = str[ 0 ] & 0x0f;        // generate length of command
    ctr++;
    for ( i = 0; i < ctr; i++ )                                  // send command
    {
      str[ 19 ] ^= str[ i ];
      writeByte( busnumber, str[ i ], 0 );
    }
    writeByte( busnumber, str[ 19 ], 0 ); // send X-Or-Byte

#ifdef LI100_USB

    status = readAnswer_LI100_USB( busnumber, str );
#else

    status = readAnswer_LI100_SERIAL( busnumber, str );
#endif

    return status;
  }

#ifdef LI100_USB
  int readAnswer_LI100_USB( long int busnumber, unsigned char *str )
#else
  int readAnswer_LI100_SERIAL( long int busnumber, unsigned char *str )
#endif
  {
    int status;
    int i, ctr;
    int tmp_addr;
    int message_processed;
    unsigned char cXor;
    struct _GLSTATE gltmp, glakt;
    struct _GASTATE gatmp /*, gaakt*/;

    message_processed = 0;
    status = -1;                // wait for answer
    while ( status == -1 )
    {
      usleep( 2000 );
      status = readByte( busnumber, 1, &str[ 0 ] );
    }
#ifdef LI100_USB
    // skip LI100_USB-header
    readByte( busnumber, 1, &str[ 0 ] );
    readByte( busnumber, 1, &str[ 0 ] );
#endif

    ctr = str[ 0 ] & 0x0f;         // generate length of answer
    ctr += 2;
    for ( i = 1; i < ctr; i++ )                                   // read answer
    {
      readByte( busnumber, 1, &str[ i ] );
    }

    cXor = 0;
    for ( i = 0; i < ctr; i++ )
    {
      cXor ^= str[ i ];
    }
    if ( cXor != 0x00 )                                           // must be 0x00
      status = -1;            // error

    if ( str[ 0 ] == 0x02 )                                       // version-number of interface
    {
      __li100->version_interface =
        ( ( str[ 1 ] & 0xf0 ) << 4 ) + ( str[ 1 ] & 0x0f );
      __li100->code_interface = ( int ) str[ 2 ];
      message_processed = 1;
    }

    // version-number of zentrale
    if ( ( str[ 0 ] == 0x62 ) || ( str[ 0 ] == 0x63 ) )
    {
      if ( str[ 1 ] == 0x21 )
      {
        __li100->version_zentrale =
          ( ( str[ 2 ] & 0xf0 ) << 4 ) + ( str[ 2 ] & 0x0f );
        if ( str[ 0 ] == 0x63 )
          __li100->code_interface = ( int ) str[ 3 ];
        else
          __li100->code_interface = -1;
#ifndef LI100_USB
        // check for address-range 1..99; 1..256 for version < V3.00
        if ( __li100->version_zentrale < 0x0300 )
        {
          if ( __li100->number_gl > 99 )
            __li100->number_gl = 99;
          if ( __li100->number_ga > 256 )
            __li100->number_ga = 256;
        }
#endif
        message_processed = 1;

      }
    }

    // power on/off
    if ( str[ 0 ] == 0x61 )
    {
      // power on
      if ( str[ 1 ] == 0x01 )
      {
        DBG( busnumber, DBG_DEBUG,
             "on bus %i power detected; old-state is %i", busnumber,
             getPower( busnumber ) );
        if ( ( __li100->emergency_on_LI100 == 1 )
             || ( !getPower( busnumber ) ) )
        {
          setPower( busnumber, 1, "No Emergency Stop" );
          __li100->emergency_on_LI100 = 0;
        }
        else if ( __li100->pgm_mode == 1 )
        {
          session_endwait( busnumber, -1 );
          setPower( busnumber, 1, "Program mode end" );
          __li100->pgm_mode = 0;
        }
        message_processed = 1;
      }
      // power off
      if ( str[ 1 ] == 0x00 )
      {
        DBG( busnumber, DBG_DEBUG,
             "on bus %i no power detected; old-state is %i",
             busnumber, getPower( busnumber ) );
        if ( ( __li100->emergency_on_LI100 == 0 )
             && ( getPower( busnumber ) ) )
        {
          setPower( busnumber, 0, "Emergency Stop" );
          __li100->emergency_on_LI100 = 1;
        }
        message_processed = 1;
      }
      // program mode on
      if ( str[ 1 ] == 0x02 )
      {
        if ( __li100->pgm_mode == 0 )
        {
          __li100->pgm_mode = 1;
          DBG( busnumber, DBG_DEBUG,
               "on bus %i program mode was activated",
               busnumber );
          setPower( busnumber, -1, "Program mode start" );
        }
        message_processed = 1;
      }
    }


    if ( ( str[ 0 ] == 0x83 ) || ( str[ 0 ] == 0xa3 ) )
    {
      if ( str[ 0 ] & 0x20 )
        add_extern_engine( busnumber, str[ 1 ] );
      else
        remove_extern_engine( busnumber, str[ 1 ] );
      gltmp.id = str[ 1 ];
      gltmp.direction = ( str[ 2 ] & 0x40 ) ? 1 : 0;
      gltmp.speed = str[ 2 ] & 0x0f;
      if ( gltmp.speed == 1 )
      {
        gltmp.speed = 0;
        gltmp.direction = 2;
      }
      else
      {
        if ( gltmp.speed > 0 )
          gltmp.speed--;
      }
      gltmp.funcs = ( ( str[ 3 ] & 0x0f ) << 1 );
      if ( str[ 2 ] & 0x20 )
        gltmp.funcs |= 0x01;        // light is on
      // get old data, to send only if something changed
      getGL( busnumber, gltmp.id, &glakt );
      if ( ( glakt.speed != gltmp.speed ) ||
           ( glakt.direction != gltmp.direction ) ||
           ( glakt.funcs != gltmp.funcs ) )
        setGL( busnumber, gltmp.id, gltmp );
      message_processed = 1;
    }

    if ( str[ 0 ] == 0xe3 )
    {
      if ( ( str[ 1 ] & 0x30 ) == 0x30 )
      {
        __li100->get_addr = 256 * ( int ) str[ 2 ];
        __li100->get_addr += ( int ) str[ 3 ];
        message_processed = 1;
      }
      if ( str[ 1 ] == 0x40 )
      {
        tmp_addr = str[ 3 ];
        tmp_addr |= str[ 2 ] << 8;
        add_extern_engine( busnumber, tmp_addr );
        message_processed = 1;
      }
    }

    // information about an engine (single traction)
    if ( str[ 0 ] == 0xe4 )
    {
      gltmp.id = __li100->last_value & 0x3fff;
      //is engine alway allocated by an external device?
      if ( !( str[ 1 ] & 0x08 ) )
      {
        remove_extern_engine( busnumber, __li100->last_value );
      }
      gltmp.direction = ( str[ 2 ] & 0x80 ) ? 1 : 0;
      switch ( str[ 1 ] & 7 )
      {
        case 0:
        case 4:
          gltmp.speed = str[ 2 ] & 0x7f;
          if ( gltmp.speed == 1 )
          {
            gltmp.speed = 0;
            //             gltmp.direction = 2;
          }
          else
          {
            if ( gltmp.speed > 0 )
              gltmp.speed--;
          }
          break;
        case 1:
        case 2:
          gltmp.speed = str[ 2 ] & 0x7f;
          gltmp.speed <<= 1;
          if ( gltmp.speed & 0x20 )
            gltmp.speed |= 0x01;
          gltmp.speed &= 0xdf;
          if ( gltmp.speed == 2 )
          {
            gltmp.speed = 0;
            //             gltmp.direction = 2;
          }
          else
          {
            if ( gltmp.speed > 0 )
              gltmp.speed -= 3;
          }
          break;
      }
      if ( str[ 3 ] & 0x10 )
        gltmp.funcs |= 0x01;        // light is on
      // get old data, to send only if something changed
      getGL( busnumber, gltmp.id, &glakt );
      if ( ( glakt.speed != gltmp.speed ) ||
           ( glakt.direction != gltmp.direction ) ||
           ( glakt.funcs != gltmp.funcs ) )
        setGL( busnumber, gltmp.id, gltmp );
      message_processed = 1;
    }

    // information about feedback
    if ( ( str[ 0 ] & 0xf0 ) == 0x40 )
    {
      ctr = str[ 0 ] & 0xf;
      ctr += 2;
      for ( i = 1; i < ctr; i += 2 )
      {
        switch ( str[ i + 1 ] & 0x60 )
        {
          case 0x00:                                         // switch-decoder without feedback
          case 0x20:                                         // switch-decoder with feedback
            gatmp.id = str[ i ];
            gatmp.id <<= 2;
            if ( str[ i + 1 ] & 0x20 )
              gatmp.id += 2;
            tmp_addr = str[ i + 1 ] & 0x03;
            if ( tmp_addr == 0x01 )
            {
              gatmp.port = 0;
              gatmp.action = 1;
              setGA( busnumber, gatmp.id, gatmp );
            }
            if ( tmp_addr == 0x02 )
            {
              gatmp.port = 1;
              gatmp.action = 1;
              setGA( busnumber, gatmp.id, gatmp );
            }
            gatmp.id++;
            tmp_addr = str[ i + 1 ] & 0x0C;
            if ( tmp_addr == 0x04 )
            {
              gatmp.port = 0;
              gatmp.action = 1;
              setGA( busnumber, gatmp.id, gatmp );
            }
            if ( tmp_addr == 0x08 )
            {
              gatmp.port = 1;
              gatmp.action = 1;
              setGA( busnumber, gatmp.id, gatmp );
            }
            break;
          case 0x40:                                         // feedback-decoder
            setFBmodul( busnumber,
                        ( str[ i ] * 2 ) + ( ( str[ i + 1 ] & 0x20 ) ? 1 : 0 ),
                        str[ i + 1 ] & 0x0f );
            break;
        }
      }
      message_processed = 1;
    }

    // answer of programming
    if ( ( str[ 0 ] == 0x63 ) && ( ( str[ 1 ] & 0xf0 ) == 0x10 ) )
    {
      if ( __li100->last_type != -1 )
      {
        session_endwait( busnumber, ( int ) str[ 3 ] );
        setSM( busnumber, __li100->last_type, -1,
               __li100->last_typeaddr, __li100->last_bit, ( int ) str[ 3 ],
               0 );
        __li100->last_type = -1;
      }
      message_processed = 1;
    }

    if ( ( str[ 0 ] == 0x61 ) && ( str[ 1 ] == 0x13 ) )
    {
      if ( __li100->last_type != -1 )
      {
        session_endwait( busnumber, -1 );
        setSM( busnumber, __li100->last_type, -1,
               __li100->last_typeaddr, __li100->last_bit,
               __li100->last_value, -1 );
        __li100->last_type = -1;
      }
      message_processed = 1;
    }

    /* at last catch all unknown command keys and show a warning message */
    if ( message_processed == 0 )
    {
      DBG( busnumber, DBG_WARN,
           "Unknown command key received: 0x%02x 0x%02x", str[ 0 ], str[ 1 ] );
    }

    return status;
  }

#ifdef LI100_USB
  int initLine_LI100_USB( long int busnumber )
#else
  int initLine_LI100_SERIAL( long int busnumber )
#endif
  {
    int status;
    int fd;
    unsigned char byte2send[ 20 ];
    struct termios interface;

    char *name = busses[ busnumber ].device;
    printf( "Begining to detect LI100 on serial line: %s\n", name );

    switch ( busses[ busnumber ].baudrate )
    {
      case B9600:
        strcpy( ( char * ) byte2send, "9600" );
        break;
      case B19200:
        strcpy( ( char * ) byte2send, "19200" );
        break;
      case B38400:
        strcpy( ( char * ) byte2send, "38400" );
        break;
      case B57600:
        strcpy( ( char * ) byte2send, "57600" );
        break;
      default:
        strcpy( ( char * ) byte2send, "9600" );
        break;
    }
    printf( "try opening serial line %s for %s baud\n", name, byte2send );
    fd = open( name, O_RDWR );
    if ( fd == -1 )
    {
      printf( "Sorry, couldn't open device.\n" );
      return 1;
    }
    busses[ busnumber ].fd = fd;
    tcgetattr( fd, &interface );
    interface.c_oflag = ONOCR;
#ifdef LI100_USB

    interface.c_cflag = CS8 | CLOCAL | CREAD | HUPCL;
#else

    interface.c_cflag = CS8 | CRTSCTS | CLOCAL | CREAD | HUPCL;
#endif

    interface.c_iflag = IGNBRK;
    interface.c_lflag = IEXTEN;
    cfsetispeed( &interface, busses[ busnumber ].baudrate );
    cfsetospeed( &interface, busses[ busnumber ].baudrate );
    interface.c_cc[ VMIN ] = 0;
    interface.c_cc[ VTIME ] = 1;
    tcsetattr( fd, TCSANOW, &interface );
    sleep( 1 );

    // get version of LI100
    byte2send[ 0 ] = 0xF0;
#ifdef LI100_USB

    status = send_command_LI100_USB( busnumber, byte2send );
#else

    status = send_command_LI100_SERIAL( busnumber, byte2send );
#endif
    // get version of zentrale
    byte2send[ 0 ] = 0x21;
    byte2send[ 1 ] = 0x21;
#ifdef LI100_USB

    status = send_command_LI100_USB( busnumber, byte2send );
#else

    status = send_command_LI100_SERIAL( busnumber, byte2send );
#endif
    // get status of zentrale
    byte2send[ 0 ] = 0x21;
    byte2send[ 1 ] = 0x24;
#ifdef LI100_USB

    status = send_command_LI100_USB( busnumber, byte2send );
#else

    status = send_command_LI100_SERIAL( busnumber, byte2send );
#endif

    return status;
  }
