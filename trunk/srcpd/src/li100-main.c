/***************************************************************************
                      li100-main.c  -  description
                        -------------------
begin                : Fri Oct 26 07:49:15 MEST 2006
copyright            : (C) 2006-2007 by Dipl.-Ing. Frank Schmischke
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
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <syslog.h>
#include <unistd.h>

#ifdef __CYGWIN__
#include <sys/socket.h>         /*for FIONREAD */
#endif
#ifdef __sun__
#include <sys/filio.h>
#endif

#include "config-srcpd.h"
#include "li100.h"
#include "io.h"
#include "toolbox.h"
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

#define __li100 ((LI100_DATA*)buses[busnumber].driverdata)
#define __li100t ((LI100_DATA*)buses[btd->bus].driverdata)

static void add_extern_engine(bus_t busnumber, int address);
static void remove_extern_engine(bus_t busnumber, int address);

#define LI100_USB
#include "li100.c"

#undef LI100_USB
#include "li100.c"


/**
 * cacheInitGL: modifies the gl data used to initialize the device
 * this is called whenever a new loco comes in town...
 */
int init_gl_LI100(gl_state_t * gl)
{
    gl->protocol = 'N';
    return SRCP_OK;
}

/**
 * initGA: modifies the ga data used to initialize the device
 */
int init_ga_LI100(ga_state_t * ga)
{
    ga->protocol = 'N';
    return SRCP_OK;
}

/**
 * add an extern controlled engine to internal table
 */
static void add_extern_engine(bus_t busnumber, int address)
{
    int i;
    int ctr;

    i = -1;
    if (__li100->extern_engine_ctr == 0)
        i = 0;                  // if no current extern engine, use first entry in table
    else {
        // search for address already existing in table
        for (ctr = 0; ctr < 100; ctr++) {
            if (__li100->extern_engine[ctr] == address)
                break;

        }
        // address not existing in table ==> search first free entry for update
        if (__li100->extern_engine_ctr < 100) {
            for (ctr = 0; ctr < 100; ctr++) {
                if (__li100->extern_engine[ctr] == -1) {
                    i = ctr;
                    break;
                }
            }
        }
    }
    if (i != -1) {
        __li100->extern_engine[i] = address;
        __li100->extern_engine_ctr++;
    }
}

/**
 * remove an extern controlled engine from internal table
 */
static void remove_extern_engine(bus_t busnumber, int address)
{
    int ctr;

    for (ctr = 0; ctr < 100; ctr++) {
        if (__li100->extern_engine[ctr] == address) {
            __li100->extern_engine[ctr] = -1;
            __li100->extern_engine_ctr--;
            break;
        }
    }
}
