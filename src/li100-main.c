/***************************************************************************
li100-main.c  -  description
-------------------
begin                : Fri Oct 26 07:49:15 MEST 2006
copyright            : (C) 2006 by Dipl.-Ing. Frank Schmischke
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
#include "srcp-session.h"

#define __li100 ((LI100_DATA*)busses[busnumber].driverdata)

int cmpTime( struct timeval *t1, struct timeval *t2 );

#define LI100_USB

#include "li100.c"

#undef LI100_USB
#include "li100.c"


/**
 * initGL: modifies the gl data used to initialize the device
 * this is called whenever a new loco comes in town...
 */
long int init_gl_LI100( struct _GLSTATE *gl )
{
  gl->protocol = 'N';
  return SRCP_OK;
}

/**
 * initGA: modifies the ga data used to initialize the device

 */
long int init_ga_LI100( struct _GASTATE *ga )
{
  ga->protocol = 'N';
  return SRCP_OK;
}

void add_extern_engine( long int busnumber, int address )
{
  int i;
  int ctr;

  i = -1;
  if ( __li100->extern_engine_ctr == 0 )
    i = 0;
  else
  {
    for ( ctr = 0; ctr < 100; ctr++ )
    {
      if ( __li100->extern_engine[ ctr ] == address )
        break;

    }
    if ( ctr < 100 )
      for ( ctr = 0; ctr < 100; ctr++ )
      {
        if ( __li100->extern_engine[ ctr ] == -1 )
        {
          i = ctr;
          break;
        }
      }
  }
  if ( i != -1 )
  {
    __li100->extern_engine[ i ] = address;
    __li100->extern_engine_ctr++;
  }
}

void remove_extern_engine( long int busnumber, int address )
{
  int ctr;

  for ( ctr = 0; ctr < 100; ctr++ )
  {
    if ( __li100->extern_engine[ ctr ] == address )
    {
      __li100->extern_engine[ ctr ] = -1;
      __li100->extern_engine_ctr--;
      break;
    }
  }
}
