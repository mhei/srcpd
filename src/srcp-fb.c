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
#include "srcp-info.h"

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
  struct timeval akt_time;

  // we read 8 or 16 ports at once, but we will only change those ports,
  // which are really changed
  if(_fbstate[bus-1][port-1].state != value)
  {
    // send_event()
    syslog(LOG_INFO, "changed: %d FB %d %d -> %d", bus, port,
      _fbstate[bus-1][port-1].state, value);

    gettimeofday(&akt_time, &dummy);
    _fbstate[bus-1][port-1].state = value;
    _fbstate[bus-1][port-1].timestamp = akt_time;

    // queue changes for writing info-message
    queueInfoFB(bus, port, value, &akt_time);
  }
}

/* all moduls with 8 or 16 ports */
int setFBmodul(int bus, int modul, int values)
{
  int i;
  int c;
  int fb_contact;
  int ports;
  int dir;
  int mask;

  ports = (busses[bus].flags & FB_16_PORTS) ? 16 : 8;
  if (busses[bus].flags & FB_ORDER_0)
  {
    dir = 0;
    mask = 1;
  }
  else
  {
    dir = 1;
    mask = 1 << (ports - 1);
  }
  // compute startcontact ( (modul - 1) * 16 + 1 )
  fb_contact = modul - 1;
  fb_contact *= ports;
  fb_contact++;

  for(i=0; i<ports;i++)
  {
    c = (values & mask) ? 1 : 0;
    updateFB(bus, fb_contact++, c);
    if (dir)
      mask >>= 1;
    else
      mask <<= 1;
  }
  return SRCP_OK;
}

///* Kurzes Modul mit 8 Ports, u.a. DDL S88 */
//int setFBmodul8(int bus, int mod, int values)
//{
//  int i;
//  for(i=0; i<8;i++)
//  {
//    int c = (values & (1 << (7-i))) ? 1 : 0;
//    updateFB(bus, (mod-1)*8 + i + 1, c);
//  }
//  return SRCP_OK;
//}

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

int describeFB(int bus, int addr, char *reply)
{
  return SRCP_NOTSUPPORTED;
}

int startup_FB()
{
  return 0;
}
