/* $Id$ */

/* 
 * Vorliegende Software unterliegt der General Public License, 
 * Version 2, 1991. (c) Matthias Trute, 2000-2001.
 *
 */
#include "stdincludes.h"
#include "srcp-descr.h"
#include "srcp-error.h"
#include "config-srcpd.h"

int startup_DESCRIPTION(void)
{
  return 0;
}

int describeBus(int bus, char *reply) {
      sprintf(reply, "%lu.%.3lu 100 INFO %d DESCRIPTION %s\n",
        busses[bus].power_change_time.tv_sec,  busses[bus].power_change_time.tv_usec/1000,
        bus, busses[bus].description);
      return SRCP_INFO;
}