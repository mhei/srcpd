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

#include <libxml/tree.h>

#include "config-srcpd.h"
#include "io.h"
#include "loopback.h"
#include "srcp-fb.h"
#include "srcp-ga.h"
#include "srcp-gl.h"
#include "srcp-power.h"
#include "srcp-srv.h"

void readconfig_loopback(xmlDocPtr doc, xmlNodePtr node, int busnumber)
{
    busses[busnumber].type = SERVER_LOOPBACK;
    busses[busnumber].init_func = &init_bus_Loopback;
    busses[busnumber].term_func = &term_bus_Loopback;
    busses[busnumber].thr_func = &thr_sendrec_Loopback;
    busses[busnumber].driverdata = malloc(sizeof(struct _LOOPBACK_DATA));
    strcpy(busses[busnumber].description, "GA GL FB POWER");
}

int init_lineLoopback (char *name) {
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
  struct _GL gltmp, glakt;
  struct _GA gatmp;
  int addr;
  char c;
  int bus = (int) v;
  
  syslog(LOG_INFO,"loopback started, bus #%d, %s", bus, busses[bus].device);

  busses[bus].watchdog = 1;

  while (1) {
      if (!queue_GL_isempty(bus))  {
        unqueueNextGL(bus, &gltmp);
        addr = gltmp.id;
        getGL(bus, addr, &glakt);

        if (gltmp.direction == 2)  {
          gltmp.speed = 0;
          gltmp.direction = !glakt.direction;
        }
        // Vorwärts/Rückwärts
        if (gltmp.direction != glakt.direction)
        {
          char c = 15 + 16 * ((gltmp.funcs & 0x10) ? 1 : 0);
          syslog(LOG_INFO, "loopback %d: GL %d func: %d", bus, addr, c);
        }
        // Geschwindigkeit und Licht setzen, erst recht nach Richtungswechsel
        if ((gltmp.speed != glakt.speed) ||
           ((gltmp.funcs & 0x10) != (glakt.funcs & 0x10)) ||
            (gltmp.direction != glakt.direction))
        {
          char c = calcspeed(gltmp.speed, gltmp.maxspeed, gltmp.n_fs) +
              16 * ((gltmp.funcs & 0x10) ? 1 : 0);
          /* jetzt aufpassen: n_fs erzwingt ggf. mehrfache Ansteuerungen des Dekoders */
          /* das Protokoll ist da wirklich eigenwillig, vorerst ignoriert!            */
        }
        // Erweiterte Funktionen des 6021 senden, manchmal
        if (( (busses[bus].flags & M6020_MODE) == 0) && (gltmp.funcs != glakt.funcs))
        {
          char c = (gltmp.funcs & 0x0f) + 64;
        }
        setGL(bus, addr, gltmp);
      }
      busses[bus].watchdog = 4;
    }
    busses[bus].watchdog = 5;
    /* Magnetantriebe, die muessen irgendwann sehr bald abgeschaltet werden */
    //fprintf(stderr, "und die Antriebe");
    if (!queue_GA_isempty(bus))
    {
      unqueueNextGA(bus, &gatmp);
      addr = gatmp.id;
      if (gatmp.action == 1)
      {
        gettimeofday(&gatmp.tv[gatmp.port], NULL);
        setGA(bus, addr, gatmp);
        if (gatmp.activetime >= 0)
        {
          gatmp.action = 0;  // nächste Aktion ist automatisches Aus
        }
        c = 33 + (gatmp.port ? 0 : 1);
      }
      setGA(bus, addr, gatmp);
      busses[bus].watchdog = 6;
  }
}
