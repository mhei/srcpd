/* $Id$ */

/* loopback: simple Busdriver without any hardware.
 *
 */
 
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <signal.h>
#include <syslog.h>

#include "config-srcpd.h"
#include "io.h"
#include "loopback.h"
#include "srcp-fb.h"
#include "srcp-ga.h"
#include "srcp-gl.h"
#include "srcp-power.h"
#include "srcp-srv.h"

int
init_lineLoopback (char *name)
{
  int FD;
  syslog(LOG_INFO,"loopback open device %s", name);
  FD = -1;
  return FD;
}

int
term_bus_Loopback(int bus)
{
  return 0;
}

/* Initialisiere den Bus, signalisiere Fehler */
/* Einmal aufgerufen mit busnummer als einzigem Parameter */
/* return code wird ignoriert (vorerst) */
int init_bus_Loopback(int i)
{
  syslog(LOG_INFO,"loopback init: bus #%d, debug %d", i, busses[i].debuglevel);
  if(busses[i].debuglevel==0)
  {
    busses[i].fd = init_lineLoopback(busses[i].device);
  }
  else
  {
    busses[i].fd = -1;
  }
  syslog(LOG_INFO, "loopback init done");
  return 1;
}

void*
thr_sendrec_Loopback (void *v)
{
  int bus, i, commands_ok, addr;
  int temp, NUMBER_FB;
  char c;
  unsigned char rr;
  struct _GL gltmp;
  struct _GA gatmp;
  unsigned char SendByte;
 
  bus = (int) v;
  
  NUMBER_FB = busses[bus].number_fb;
  syslog(LOG_INFO,"loopback started, bus #%d, %s", bus, busses[bus].device);

  busses[bus].watchdog = 1;

  while (1)
  {
    usleep(1000);
    busses[bus].watchdog = 1;
  }
}
