/* $Id$ */

/* 
 * Vorliegende Software unterliegt der General Public License, 
 * Version 2, 1991. (c) Matthias Trute, 2000-2001.
 *
 */


#include <stdio.h>
#include <string.h>
#include <syslog.h>

#include "config-srcpd.h"
#include "srcp-error.h"

#include "srcp-fb.h"

#include "srcp-fb-i8255.h"

/* one huge array of all possible feedbacks */
/* not visible outside of this module       */
static struct _FBSTATE _fbstate[MAX_BUSSES][MAXFBS];
// 20 x 256 x 16 x 10 bytes = 819200 bytes.

int getFB(int bus, int port)
{
  int result;
  result = _fbstate[bus-1][port-1].state;
  return result;
}

void updateFB(int bus, int port, int value)
{
  struct timezone dummy;
  if(_fbstate[bus-1][port-1].state != value)
  {
    // send_event()
  }
  _fbstate[bus-1][port-1].state = value;
  gettimeofday(& _fbstate[bus-1][port-1].timestamp, &dummy);
}

int setFBmodul(int bus, int mod, int values)
{
  int i;
  for(i=0; i<16;i++)
  {
    int c = (values & (1 << (15-i))) ? 1 : 0;
    updateFB(bus, (mod-1)*16 + i + 1, c);
  }
  return SRCP_OK;
}
  
int infoFB(int bus, int port, char *msg)
{
  int state = getFB(bus, port);
  if(state>=0)
  {
    sprintf(msg, "%d FB %d %d", bus, port, state);
    return SRCP_INFO;
  }
  else
  {
    return SRCP_NODATA;
  }
}

int describeFB(int bus, int addr, char *reply) {
    return SRCP_NOTSUPPORTED;
}

int
startup_FB()
{
  return 0;
}
