/* cvs: $Id$             */

/* 
 * Vorliegende Software unterliegt der General Public License, 
 * Version 2, 1991. (c) Matthias Trute, 2000-2001.
 *
 * 30.01.2002 Matthias Trute
 *            - Rueckmelder optimiert, Bugfixes (Anregungen von Michael Hodel)
 *
 * 05.07.2001 Frank Schmischke
 *            - Anpassungen wegen srcpd
 *            - es werden nur noch Befehle gesendet, wenn Auftrge vorliegen,
 *              es werden nicht mehr alle Adressen auf Verdacht durchsucht
 *
 */


/* Die Konfiguration des seriellen Ports von M6050emu (D. Schaefer)   */
/* wenngleich etwas verändert, mea culpa..                            */

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
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
#include "m605x.h"
#include "srcp-fb.h"
#include "srcp-ga.h"
#include "srcp-gl.h"
#include "srcp-power.h"
#include "srcp-srv.h"


#define __m6051 ((M6051_DATA*)busses[busnumber].driverdata)

/** readconfig_m605x: liest den Teilbaum der xml Configuration und parametriert
     den busspezifischen Datenteil, wird von register_bus() aufgerufen */

int readconfig_m605x(xmlDocPtr doc, xmlNodePtr node, int busnumber)
{
    xmlNodePtr child = node->children;
    busses[busnumber].type = SERVER_M605X;
    busses[busnumber].init_func = &init_bus_M6051;
    busses[busnumber].term_func = &term_bus_M6051;
    busses[busnumber].thr_func = &thr_sendrec_M6051;
    busses[busnumber].driverdata = malloc(sizeof(struct _M6051_DATA));
    ((M6051_DATA *) busses[busnumber].driverdata)->number_fb = 0;	/* max 31 */
    ((M6051_DATA *) busses[busnumber].driverdata)->number_ga = 256;
    ((M6051_DATA *) busses[busnumber].driverdata)->number_gl = 80;
    ((M6051_DATA *) busses[busnumber].driverdata)->ga_min_active_time = 75;
    ((M6051_DATA *) busses[busnumber].driverdata)->pause_between_cmd = 200;
    ((M6051_DATA *) busses[busnumber].driverdata)->pause_between_bytes = 2;
    strcpy(busses[busnumber].description, "GA GL FB POWER");

    while (child) {
          if(strncmp(child->name, "text", 4)==0) {
            child = child -> next;
            continue;
          }

	if (strcmp(child->name, "number_fb") == 0) {
	    char *txt = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
	    ((M6051_DATA *) busses[busnumber].driverdata)->number_fb =
		atoi(txt);
	    free(txt);
	}

	if (strcmp(child->name, "number_gl") == 0) {
	    char *txt =
		xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
	    ((M6051_DATA *) busses[busnumber].driverdata)->number_gl =
		atoi(txt);
	    free(txt);
	}
	if (strcmp(child->name, "number_ga") == 0) {
	    char *txt =
		xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
	    ((M6051_DATA *) busses[busnumber].driverdata)->number_ga =
		atoi(txt);
	    free(txt);
	}
	if (strcmp(child->name, "mode_m6020") == 0) {
	    char *txt =
		xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
	    if (strcmp(txt, "yes") == 0) {
		((M6051_DATA *) busses[busnumber].driverdata)->flags |=
		    M6020_MODE;
	    }
	    free(txt);
	}
	child = child->next;
    }
    return 0;
}


/*******************************************************
 *     SERIELLE SCHNITTSTELLE KONFIGURIEREN           
 *******************************************************
 */
static int init_lineM6051(int bus) {
  int FD;
  struct termios interface;

  if (busses[bus].debuglevel)
  {
    printf("Opening 605x: %s\n", busses[bus].device);
  }
  if ((FD = open(busses[bus].device, O_RDWR | O_NONBLOCK)) == -1)
  {
    printf("couldn't open device.\n");
    return -1;
  }
  tcgetattr(FD, &interface);
  interface.c_oflag = ONOCR | ONLRET;
  interface.c_oflag &= ~(OLCUC | ONLCR | OCRNL);
  interface.c_cflag = CS8 | CRTSCTS;
  interface.c_cflag &= ~(CSTOPB | PARENB);
  interface.c_iflag = IGNBRK | IGNPAR;
  interface.c_iflag &= ~(ISTRIP | IXON | IXOFF | IXANY);
  interface.c_lflag = NOFLSH | IEXTEN;
  interface.c_lflag &= ~(ISIG | ICANON | ECHO | ECHOE | TOSTOP | PENDIN);
  cfsetospeed(&interface, B2400);
  cfsetispeed(&interface, B2400);
  tcsetattr(FD, TCSAFLUSH, &interface);

  return FD;
}

int init_bus_M6051(int bus) {
  syslog(LOG_INFO," M605x  init: bus #%d, debug %d", bus, busses[bus].debuglevel);
  if(busses[bus].debuglevel==0)
  {
    busses[bus].fd = init_lineM6051(bus);
  }
  else
  {
    busses[bus].fd = -1;
  }
  syslog(LOG_INFO, "M605x bus init done");
  return 0;
}

int
term_bus_M6051(int bus)
{
  return 0;
}

void*
thr_sendrec_M6051(void *v)
{
  unsigned char SendByte;
  int fd, akt_S88, addr, temp, number_fb;
  char c;
  unsigned char rr;
  struct _GL gltmp, glakt;
  struct _GA gatmp;
  int bus = (int) v;
  unsigned int ga_min_active_time = ( (M6051_DATA *) busses[bus].driverdata)  ->ga_min_active_time;
  unsigned int pause_between_cmd = ( (M6051_DATA *) busses[bus].driverdata)  ->  pause_between_cmd;
  unsigned int pause_between_bytes = ( (M6051_DATA *) busses[bus].driverdata)  ->      pause_between_bytes;

  akt_S88 = 1;
  fd = busses[bus].fd;
  busses[bus].watchdog = 1;
  number_fb = ( (M6051_DATA *) busses[bus].driverdata)  -> number_fb;

  if (( (M6051_DATA *) busses[bus].driverdata)  -> cmd32_pending)
  {
    SendByte = 32;
    writeByte(fd, &SendByte, pause_between_cmd);
    ( (M6051_DATA *) busses[bus].driverdata)  -> cmd32_pending = 0;
  }

  while (1)
  {
    busses[bus].watchdog = 2;
    /* Start/Stop */
    if (busses[bus].power_changed)
    {
      SendByte = (busses[bus].power_state) ? 96 : 97;
      writeByte(fd, &SendByte, pause_between_cmd);
      writeByte(fd, &SendByte, pause_between_cmd);  /* zweimal, wir sind paranoid */
      busses[bus].power_changed = 0;
    }
    busses[bus].watchdog = 3;
    /* Lokdecoder */
    /* nur senden, wenn wirklich etwas vorliegt */
    if (!( (M6051_DATA *) busses[bus].driverdata)  -> cmd32_pending)
    {
      if (!queue_GL_isempty(bus))
      {
        unqueueNextGL(bus, &gltmp);
        addr = gltmp.id;
        getGL(bus, addr, &glakt);

        if (gltmp.direction == 2)
        {
          gltmp.speed = 0;
          gltmp.direction = !glakt.direction;
        }
        // Vorwärts/Rückwärts
        if (gltmp.direction != glakt.direction)
        {
          c = 15 + 16 * ((gltmp.funcs & 0x10) ? 1 : 0);
          writeByte(fd, &c, pause_between_bytes);
          SendByte = addr;
          writeByte(fd, &SendByte, pause_between_cmd);
        }
        // Geschwindigkeit und Licht setzen, erst recht nach Richtungswechsel
        if ((gltmp.speed != glakt.speed) ||
           ((gltmp.funcs & 0x10) != (glakt.funcs & 0x10)) ||
            (gltmp.direction != glakt.direction))
        {
          c = gltmp.speed + 16 * ((gltmp.funcs & 0x10) ? 1 : 0);
          /* jetzt aufpassen: n_fs erzwingt ggf. mehrfache Ansteuerungen des Dekoders */
          /* das Protokoll ist da wirklich eigenwillig, vorerst ignoriert!            */
          writeByte(fd, &c, pause_between_bytes);
          SendByte = addr;
          writeByte(fd, &SendByte, pause_between_cmd);
        }
        // Erweiterte Funktionen des 6021 senden, manchmal
        if (( (busses[bus].flags & M6020_MODE) == 0) && (gltmp.funcs != glakt.funcs))
        {
          c = (gltmp.funcs & 0x0f) + 64;
          writeByte(fd, &c, pause_between_bytes);
          SendByte = addr;
          writeByte(fd, &SendByte, pause_between_cmd);
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
          gatmp.activetime = (gatmp.activetime > ga_min_active_time) ?
                        ga_min_active_time : gatmp.activetime;
          gatmp.action = 0;  // nächste Aktion ist automatisches Aus
        }
        else
        {
          gatmp.activetime = ga_min_active_time;  // egal wieviel, mind. 75m ein
        }
        c = 33 + (gatmp.port ? 0 : 1);
        SendByte = gatmp.id;
        writeByte(fd, &c, pause_between_bytes);
        writeByte(fd, &SendByte, pause_between_bytes);
        usleep(1000L * (unsigned long) gatmp.activetime);
        ( (M6051_DATA *) busses[bus].driverdata)  -> cmd32_pending = 1;
      }
      if ((gatmp.action == 0) && ( (M6051_DATA *) busses[bus].driverdata)  -> cmd32_pending)
      {
        // fprintf(stderr, "32 abzusetzen\n");
        SendByte = 32;
        writeByte(fd, &SendByte, pause_between_cmd);
        ( (M6051_DATA *) busses[bus].driverdata)  -> cmd32_pending = 0;
      }
      setGA(bus, addr, gatmp);
      busses[bus].watchdog = 6;
    }
    busses[bus].watchdog = 7;
    //fprintf(stderr, "Feedback ...");
    /* S88 Status einlesen, einen nach dem anderen */
    if (!( (M6051_DATA *) busses[bus].driverdata)  -> cmd32_pending && number_fb)
    {
      ioctl(fd, FIONREAD, &temp);
      while (temp > 0)
      {
        readByte(fd, &rr);
        ioctl(fd, FIONREAD, &temp);
        syslog(LOG_INFO, "FB M6051: oops; ignoring unread byte: %d ", rr);
      }
      SendByte = 192 + akt_S88;
      writeByte(fd, &SendByte, pause_between_cmd);
      busses[bus].watchdog = 8;
      readByte(fd, &rr);
      temp = rr;
      temp <<= 8;
      busses[bus].watchdog = 9;
      readByte(fd, &rr);
      setFBmodul(bus, akt_S88, temp | rr);
      akt_S88++;
      if (akt_S88 > number_fb)
        akt_S88 = 1;
    }
    busses[bus].watchdog = 10;
    // fprintf(stderr, " ende\n");
  }
}
