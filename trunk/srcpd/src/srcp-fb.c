
/* 
 * Vorliegende Software unterliegt der General Public License, 
 * Version 2, 1991. (c) Matthias Trute, 2000-2001.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

#include "config-srcpd.h"
#include "srcp-error.h"
#include "srcp-info.h"

#include "srcp-fb.h"

#include "srcp-fb-i8255.h"

/* one array for all busses             */
/* not visible outside of this module   */
static struct _FB fb[MAX_BUSSES];

int getFB(int bus, int port, struct timeval *time)
{
  int result;
  result = fb[bus].fbstate[port-1].state;
  *time  = fb[bus].fbstate[port-1].timestamp;
  return result;
}

void updateFB(int bus, int port, int value)
{
  struct timezone dummy;
  struct timeval akt_time;

  // we read 8 or 16 ports at once, but we will only change those ports,
  // which are really changed
  if(fb[bus].fbstate[port-1].state != value)
  {
    // send_event()
    syslog(LOG_INFO, "changed: %d FB %d %d -> %d", bus, port,
      fb[bus].fbstate[port-1].state, value);

    gettimeofday(&akt_time, &dummy);
    fb[bus].fbstate[port-1].state = value;
    fb[bus].fbstate[port-1].timestamp = akt_time;

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
  // compute startcontact ( (modul - 1) * ports + 1 )
  fb_contact = modul - 1;
  fb_contact *= ports;
  fb_contact++;

  for(i=0; i<ports; i++)
  {
//    syslog(LOG_INFO, "%d ports, order %d, mask 0x%04x, value 0x%04x", ports, dir, mask, values);
    c = (values & mask) ? 1 : 0;
    updateFB(bus, fb_contact++, c);
    if (dir)
      mask >>= 1;
    else
      mask <<= 1;
  }
  return SRCP_OK;
}

int infoFB(int bus, int port, char *msg)
{
  struct timeval time;
  int state = getFB(bus, port, &time);
  if(state>=0)
  {
    sprintf(msg, "%ld.%ld 100 INFO %d FB %d %d",
     time.tv_sec, time.tv_usec/1000, bus, port, state);
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
  int i;
  for(i=0;i<MAX_BUSSES;i++)
  {
    fb[i].numberOfFb = 0;
    fb[i].fbstate = NULL;
  }
  return 0;
}

int init_FB(int bus, int number)
{
  struct timeval akt_time;
  int i;

  if (bus >= MAX_BUSSES)
    return 1;

  if (number > 0)
  {
    gettimeofday(&akt_time, NULL);

    fb[bus].fbstate = malloc(number * sizeof(struct _FBSTATE));
    if (fb[bus].fbstate == NULL)
      return 1;
    fb[bus].numberOfFb = number;
    for(i=0;i<number;i++)
    {
      fb[bus].fbstate[i].state = -1;
      fb[bus].fbstate[i].timestamp = akt_time;
    }
  }
  return 0;
}

int get_number_fb(int bus)
{
  return fb[bus].numberOfFb;
}

