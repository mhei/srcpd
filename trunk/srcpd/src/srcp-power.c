/* $Id$ */

/* 
 * Vorliegende Software unterliegt der General Public License, 
 * Version 2, 1991. (c) Matthias Trute, 2000-2001.
 *
 */


#include <string.h>
#include <stdio.h>

#include "srcp-power.h"
#include "config-srcpd.h"

void setPower(int bus, int state, char *msg)
{
  busses[bus].power_state = state;
  strcpy(busses[bus].power_msg, msg);
  busses[bus].power_changed = 1;
}

int getPower(int bus)
{
  return busses[bus].power_state;
}

void infoPower(int bus, char *msg)
{
  sprintf(msg, "INFO POWER %s %s\n", busses[bus].power_state?"ON":"OFF", busses[bus].power_msg);
}

void initPower(int bus)
{
}
