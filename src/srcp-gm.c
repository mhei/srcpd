/* $Id$ */

/*
 *
 */
#include "stdincludes.h"

#include "config-srcpd.h"
#include "srcp-error.h"
#include "srcp-info.h"
#include "srcp-gm.h"
#include "syslogmessage.h"


int setGM( bus_t bus, char *msg )
{
    struct timeval akt_time;
    char *msgtmp;
    DBG(0, DBG_DEBUG, "%s", msg);

    gettimeofday(&akt_time, NULL);
    msgtmp = malloc(strlen(msg)+30);
    sprintf( msgtmp, "%lu.%.3lu 100 INFO %ld GM %s\n",
           akt_time.tv_sec, akt_time.tv_usec / 1000, bus,
           msg );
  queueInfoMessage( msgtmp );
  free(msgtmp);
  return SRCP_OK;
}
