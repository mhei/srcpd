/* cvs: $Id$             */

/* 
 * Vorliegende Software unterliegt der General Public License, 
 * Version 2, 1991. (c) Matthias Trute, 2000-2001.
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
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <signal.h>
#include <syslog.h>

#include "config-srcpd.h"
#include "io.h"
#include "m605x.h"
#include "srcp-fb.h"
#include "srcp-ga.h"
#include "srcp-gl.h"
#include "srcp-power.h"
#include "srcp-srv.h"

/* Folgende Werte lassen sich z.T. verändern, siehe Kommandozeilen/Configdatei */
/* unveränderliche sind mit const markiert                                     */

/* das Programm arbeitet mit vielen Threads. und in der C-Fibel steht was
 * von volatile, schadet vermutlich nicht.
 */

/* einige Zeitkonstanten, alles Millisekunden */
unsigned int ga_min_active_time = 75;
unsigned int pause_between_cmd = 200;
unsigned int pause_between_bytes = 2;

/********************************************************
 * von dem Thread gibt es genau einen pro Interface
 * Datenaustausch über einige globale Speicherbereiche..
 ********************************************************
 */
/*******************************************************
 *     SERIELLE SCHNITTSTELLE KONFIGURIEREN           
 *******************************************************
 */
int init_line6051 (char *name)
{
  int FD;
  struct termios interface;
  syslog(LOG_INFO,"m605x open device %s", name);
  if ((FD = open (name, O_RDWR)) == -1)
  {
    printf ("couldn't open device.\n");
    return -1;
  }
  tcgetattr (FD, &interface);
  interface.c_oflag = ONOCR | ONLRET;
  interface.c_oflag &= ~(OLCUC | ONLCR | OCRNL);
  interface.c_cflag = CS8 | CRTSCTS;
  interface.c_cflag &= ~(CSTOPB | PARENB);
  interface.c_iflag = IGNBRK | IGNPAR;
  interface.c_iflag &= ~(ISTRIP | IXON | IXOFF | IXANY);
  interface.c_lflag = NOFLSH | IEXTEN;
  interface.c_lflag &= ~(ISIG | ICANON | ECHO | ECHOE | TOSTOP | PENDIN);
  cfsetospeed (&interface, B2400);
  cfsetispeed (&interface, B2400);
  tcsetattr (FD, TCSAFLUSH, &interface);

  return FD;
}

/* Initialisiere den Bus, signalisiere Fehler */
int init_bus_M6051(int i) {
  syslog(LOG_INFO,"m605x init: bus #%d, debug %d", i, busses[i].debuglevel);
  if(busses[i].debuglevel==0) {
    busses[i].fd = init_line6051(busses[i].device);
    if(busses[i].fd < 0)
    {
      syslog(LOG_INFO,"device M605x an %s nicht vorhanden?!\n", busses[i].device);
    }
    if(busses[i].deviceflags && RESTORE_COM_SETTINGS) {
       save_comport(i);
    }
  }
  syslog(LOG_INFO, "m605x init done");
  return 1;
}

int term_bus_M6051(int bus) {
    return 0;
}

void* thr_sendrec_M6051 (void *v)
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
  syslog(LOG_INFO,"m605x started, bus #%d, %s", bus, busses[bus].device);

  /* erst mal alle Schaltaktionen canceln?, lieber nicht pauschal.. */
  busses[bus].watchdog = 1;
  if (busses[bus].cmd32_pending)
  {
    SendByte = 32;
    writeByte (bus, &SendByte, pause_between_cmd);
    busses[bus].cmd32_pending = 0;
  }

  while (1)
  {
    busses[bus].watchdog = 2;
    /* Start/Stop */
    //fprintf(stderr, "START/STOP... ");
    if (busses[bus].power_changed)
    {
      SendByte = (busses[bus].power_state) ? 96 : 97;
      writeByte (bus, &SendByte, pause_between_cmd);
      writeByte (bus, &SendByte, pause_between_cmd);  /* zweimal, wir sind paranoid */
      busses[bus].power_changed = 0;
    }
    busses[bus].watchdog = 3;
    /* Lokdecoder */
    //fprintf(stderr, "LOK's... ");
    /* nur senden, wenn wirklich etwas vorliegt */
    if (!busses[bus].cmd32_pending)
    {
      if(! queue_gl_empty())
      {

	  unqueueNextGL(&gltmp);
            addr = gltmp.id;
            if (gltmp.direction == 2)
            {
              gltmp.speed = 0;
              gltmp.direction = !gl[bus][addr].direction;
            }
            // Vorwärts/Rückwärts
            if (gltmp.direction != gl[bus][addr].direction)
            {
              c = 15 + 16 * ((gltmp.flags & 0x10) ? 1 : 0);
              writeByte (bus, &c, pause_between_bytes);
              SendByte = gltmp.id;
              writeByte (bus, &SendByte, pause_between_cmd);
            }
            // Geschwindigkeit und Licht setzen, erst recht nach Richtungswechsel
            if ((gltmp.speed != gl[bus][addr].speed) ||
               ((gltmp.flags & 0x10) != (gl[bus][addr].flags & 0x10)) ||
               (gltmp.direction != gl[bus][addr].direction))
            {
              c = calcspeed (gltmp.speed, gltmp.maxspeed,
               gltmp.n_fs) +
               16 * ((gltmp.flags & 0x10) ? 1 : 0);
              /* jetzt aufpassen: n_fs erzwingt ggf. mehrfache Ansteuerungen des Dekoders */
              /* das Protokoll ist da wirklich eigenwillig, vorerst ignoriert!            */
              writeByte (bus, &c, pause_between_bytes);
              SendByte = gltmp.id;
              writeByte (bus, &SendByte, pause_between_cmd);
            }
            // Erweiterte Funktionen des 6021 senden, manchmal
            if ((busses[bus].flags && M6020_MODE) && (gltmp.flags != gl[bus][addr].flags))
            {
              c = (gltmp.flags & 0x0f) + 64;
              writeByte (bus, &c, pause_between_bytes);
              SendByte = gltmp.id;
              writeByte (bus, &SendByte, pause_between_cmd);
            }
            gettimeofday (&gltmp.tv, NULL);
            gl[bus][addr] = gltmp;
          }
          busses[bus].watchdog = 4;
        }
        busses[bus].sending_gl = 0;
        if (commands_ok)
          busses[bus].command_gl = 0;
      }
    }
    busses[bus].watchdog = 5;
    /* Magnetantriebe, die muessen irgendwann sehr bald abgeschaltet werden */
    //fprintf(stderr, "und die Antriebe");
    if (busses[bus].command_ga)
    {
      //                      commands_ok = 1;
      busses[bus].sending_ga = 1;
      for (i = 0; i < 50; i++)
      {
        if (nga[busses[bus].number][i].id)
        {
          gatmp = nga[busses[bus].number][i];
          addr = gatmp.id;
          if (gatmp.action == 1)
          {
            gettimeofday (&gatmp.tv[gatmp.port], NULL);
            ga[bus][addr] = gatmp;
            if (gatmp.activetime >= 0)
            {
              gatmp.activetime += (gatmp.activetime > ga_min_active_time) ? 0 : ga_min_active_time;  // mind. 75ms
              gatmp.action = 0;  // nächste Aktion ist automatisches Aus
            }
            else
            {
              gatmp.activetime = ga_min_active_time;  // egal wieviel, mind. 75m ein
            }
            c = 33 + (gatmp.port ? 0 : 1);
            writeByte (bus, &c, pause_between_bytes);
            SendByte = gatmp.id;
            writeByte (bus, &SendByte, (unsigned long) gatmp.activetime);
	    busses[bus].cmd32_pending = 1;
          }
          if ((gatmp.action == 0) && busses[i].cmd32_pending)
          {
            // fprintf(stderr, "32 abzusetzen\n");
            SendByte = 32;
            writeByte (bus, &SendByte, pause_between_cmd);
            busses[bus].cmd32_pending = 0;
          }
          gettimeofday (&gatmp.tv[gatmp.port], NULL);
          ga[bus][addr] = gatmp;
          nga[bus][i].id = 0;
          busses[bus].watchdog = 6;
        }
      }
      busses[bus].sending_ga = 0;
      busses[bus].command_ga = 0;
    }
    busses[bus].watchdog = 7;
    /* S88 Status einlesen, einen nach dem anderen */
    if (!busses[bus].cmd32_pending && NUMBER_FB)
    {
      SendByte = 128 + NUMBER_FB;
      writeByte (bus, &SendByte, pause_between_bytes);
      busses[bus].watchdog = 8;
      for (i = 0; i < NUMBER_FB; i++)
      {
        readByte (bus, &rr);
        temp = rr;
        temp <<= 8;
        busses[bus].watchdog = 9;
        readByte (bus, &rr);
        setFBmodule(busses[bus].number, i, temp | rr);
      }
      usleep (pause_between_cmd * 1000);
    }
    busses[bus].watchdog = 10;
  }
}
