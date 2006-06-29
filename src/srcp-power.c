/* $Id$ */

/*
 * Vorliegende Software unterliegt der General Public License,
 * Version 2, 1991. (c) Matthias Trute, 2000-2001.
 *
 */
#include "stdincludes.h"

#include "config-srcpd.h"
#include "srcp-error.h"
#include "srcp-info.h"
#include "srcp-power.h"

int setPower(long int bus, int state, char *msg)
{
    gettimeofday(&busses[bus].power_change_time, NULL);
//  busses[bus].power_state = (state == -1) ? 0 : state;
    busses[bus].power_state = state;
    strcpy(busses[bus].power_msg, msg);
//  busses[bus].power_changed = (state == -1) ? 0 : 1;
    busses[bus].power_changed = 1;
    char reply[200];
    infoPower(bus, reply);
    queueInfoMessage(reply);
    return SRCP_OK;
}

int getPower(long int bus)
{
    return busses[bus].power_state;
}

int infoPower(long int bus, char *msg)
{
    sprintf(msg, "%lu.%.3lu 100 INFO %ld POWER %s %s\n",
            busses[bus].power_change_time.tv_sec,
            busses[bus].power_change_time.tv_usec / 1000, bus,
            busses[bus].power_state ? "ON" : "OFF", busses[bus].power_msg);
    return SRCP_INFO;
}

int initPower(long int bus)
{
    return SRCP_OK;
}
