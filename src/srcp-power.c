/* $Id$ */

/* 
 * Vorliegende Software unterliegt der General Public License, 
 * Version 2, 1991. (c) Matthias Trute, 2000-2001.
 *
 */
#include "stdincludes.h"

#include "config-srcpd.h"
#include "srcp-error.h"

#include "srcp-power.h"

int
setPower(int bus, int state, char *msg)
{
  busses[bus].power_state = state;
  strcpy(busses[bus].power_msg, msg);
  busses[bus].power_changed = 1;
  return SRCP_OK;
}

int
getPower(int bus)
{
  return busses[bus].power_state;
}

int
infoPower(int bus, char *msg)
{
  sprintf(msg, "%d POWER %s %s", bus, busses[bus].power_state?"ON":"OFF", busses[bus].power_msg);
  return SRCP_INFO;
}

int
initPower(int bus)
{
    return SRCP_OK;
}
