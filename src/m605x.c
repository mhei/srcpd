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
#include "srcp-fb-s88.h"
#include "srcp-ga.h"
#include "srcp-gl.h"
#include "srcp-power.h"
#include "srcp-srv.h"

/* Folgende Werte lassen sich z.T. verändern, siehe Kommandozeilen/Configdatei */
/* unveränderliche sind mit const markiert                                     */

/* das Programm arbeitet mit vielen Threads. und in der C-Fibel steht was
 * von volatile, schadet vermutlich nicht.
 */
volatile int io_thread_running = 0;	/* eine Art Wachhund, siehe main()     */
volatile int server_shutdown_state = 0;	/* wird gesetzt und beendet den Server */
volatile int server_reset_state = 0;	/* Reset des Servers, unimplemented    */

/* Datenaustausch, zugeschnitten auf den 6051 von Märklin (tm?) */
volatile int cmd32_pending = 0;	/* Als nächstes muß ein 32 abgesetzt werden */

/* einige Zeitkonstanten, alles Millisekunden */
unsigned int ga_min_active_time = 75;
unsigned int pause_between_cmd = 200;
unsigned int pause_between_bytes = 2;

extern int debuglevel;
extern int power_changed;
extern int power_state;
extern int M6020MODE;
extern int NUMBER_FB;
extern int file_descriptor[NUMBER_SERVERS];

extern volatile struct _GL gl[MAXGLS];	/* aktueller Stand, mehr gibt es nicht */
extern volatile struct _GL ngl[MAXGLS];	/* neuer, evt geaenderter Wert         */
extern volatile struct _GA ga[MAXGAS];	/* soviele Generic Accessoires gibts             */
extern volatile struct _GA nga[MAXGAS];	/* soviele Generic Accessoires gibts, neue Werte */
extern volatile int fb[MAXFBS];

extern volatile int commands_gl;
extern volatile int sending_gl;
extern volatile int commands_ga;
extern volatile int sending_ga;

/********************************************************
 * von dem Thread gibt es genau einen pro Interface
 * also wohl nur einen überhaupt..
 * Datenaustausch über einige globale
 * Speicherbereiche..
 ********************************************************
 */
/*******************************************************
 *     SERIELLE SCHNITTSTELLE KONFIGURIEREN           
 *******************************************************
 */
int
init_line6051 (char *name)
{
  int FD;
  struct termios interface;

  if (debuglevel)
  {
    printf ("Opening 605x: %s\n", name);
  }
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

void *
thr_sendrec6051 (void *v)
{
  unsigned char SendByte;
  int i, fd;
  int commands_ok;
  int addr;
  int temp;
  char c;
  unsigned char rr;
  struct _GL gltmp;
  struct _GA gatmp;

  fd = file_descriptor[SERVER_M605X];
  /* erst mal alle Schaltaktionen canceln?, lieber nicht pauschal.. */
  io_thread_running = 1;
  if (cmd32_pending)
    {
      SendByte = 32;
      writeByte (fd, &SendByte, pause_between_cmd);
      cmd32_pending = 0;
    }

  while (1)
    {
      io_thread_running = 2;
      /* Start/Stop */
      //fprintf(stderr, "START/STOP... ");
      if (power_changed)
	{
	  SendByte = (power_state) ? 96 : 97;
	  writeByte (fd, &SendByte, pause_between_cmd);
	  writeByte (fd, &SendByte, pause_between_cmd);	/* zweimal, wir sind paranoid */
	  power_changed = 0;
	}
      io_thread_running = 3;
      /* Lokdecoder */
      //fprintf(stderr, "LOK's... ");
      /* nur senden, wenn wirklich etwas vorliegt */
      if (!cmd32_pending)
	{
	  if (commands_gl)
	    {
	      commands_ok = 1;
	      sending_gl = 1;
	      for (i = 0; i < 50; i++)
		{
		  if (ngl[i].id)
		    {
		      gltmp = ngl[i];
		      ngl[i].id = 0;
		      addr = gltmp.id;
		      if (gltmp.direction == 2)
			{
			  gltmp.speed = 0;
			  gltmp.direction = !gl[addr].direction;
			}
		      // Vorwärts/Rückwärts
		      if (gltmp.direction != gl[addr].direction)
			{
			  c = 15 + 16 * ((gltmp.flags & 0x10) ? 1 : 0);
			  writeByte (fd, &c, pause_between_bytes);
			  SendByte = gltmp.id;
			  writeByte (fd, &SendByte, pause_between_cmd);
			}
		      // Geschwindigkeit und Licht setzen, erst recht nach Richtungswechsel
		      if ((gltmp.speed != gl[addr].speed) ||
			  ((gltmp.flags & 0x10) != (gl[addr].flags & 0x10)) ||
			  (gltmp.direction != gl[addr].direction))
			{
			  c =
			    calcspeed (gltmp.speed, gltmp.maxspeed,
				       gltmp.n_fs) +
			    16 * ((gltmp.flags & 0x10) ? 1 : 0);
			  /* jetzt aufpassen: n_fs erzwingt ggf. mehrfache Ansteuerungen des Dekoders */
			  /* das Protokoll ist da wirklich eigenwillig, vorerst ignoriert!            */
			  writeByte (fd, &c, pause_between_bytes);
			  SendByte = gltmp.id;
			  writeByte (fd, &SendByte, pause_between_cmd);
			}
		      // Erweiterte Funktionen des 6021 senden, manchmal
		      if ((M6020MODE == 0) && (gltmp.flags != gl[addr].flags))
			{
			  c = (gltmp.flags & 0x0f) + 64;
			  writeByte (fd, &c, pause_between_bytes);
			  SendByte = gltmp.id;
			  writeByte (fd, &SendByte, pause_between_cmd);
			}
		      gettimeofday (&gltmp.tv, NULL);
		      gl[addr] = gltmp;
		    }
		  io_thread_running = 4;
		}
	      sending_gl = 0;
	      if (commands_ok)
		commands_gl = 0;
	    }
	}
      io_thread_running = 5;
      /* Magnetantriebe, die muessen irgendwann sehr bald abgeschaltet werden */
      //fprintf(stderr, "und die Antriebe");
      if (commands_ga)
	{
//                      commands_ok = 1;
	  sending_ga = 1;
	  for (i = 0; i < 50; i++)
	    {
	      if (nga[i].id)
		{
		  gatmp = nga[i];
		  addr = gatmp.id;
		  if (gatmp.action == 1)
		    {
		      gettimeofday (&gatmp.tv[gatmp.port], NULL);
		      ga[addr] = gatmp;
		      if (gatmp.activetime >= 0)
			{
			  gatmp.activetime += (gatmp.activetime > ga_min_active_time) ? 0 : ga_min_active_time;	// mind. 75ms
			  gatmp.action = 0;	// nächste Aktion ist automatisches Aus
			}
		      else
			{
			  gatmp.activetime = ga_min_active_time;	// egal wieviel, mind. 75m ein
			}
		      c = 33 + (gatmp.port ? 0 : 1);
		      writeByte (fd, &c, pause_between_bytes);
		      SendByte = gatmp.id;
		      writeByte (fd, &SendByte,
				 (unsigned long) gatmp.activetime);
		      cmd32_pending = 1;
		    }
		  if ((gatmp.action == 0) && cmd32_pending)
		    {
		      // fprintf(stderr, "32 abzusetzen\n");
		      SendByte = 32;
		      writeByte (fd, &SendByte, pause_between_cmd);
		      cmd32_pending = 0;
		    }
		  gettimeofday (&gatmp.tv[gatmp.port], NULL);
		  ga[addr] = gatmp;
		  nga[i].id = 0;
		  io_thread_running = 5;
		}
	    }
	  sending_ga = 0;
	  commands_ga = 0;
	}
      io_thread_running = 6;
      //fprintf(stderr, "Feedback ...");
      /* S88 Status einlesen, einen nach dem anderen */
      if (!cmd32_pending && NUMBER_FB)
	{
	  SendByte = 128 + NUMBER_FB;
	  writeByte (fd, &SendByte, pause_between_bytes);
	  io_thread_running = 7;
	  for (i = 0; i < NUMBER_FB; i++)
	    {
	      readByte (fd, &rr);
	      temp = rr;
	      temp <<= 8;
	      io_thread_running = 8;
	      readByte (fd, &rr);
	      fb[i] = temp | rr;
	    }
	  usleep (pause_between_cmd * 1000);
	}
      io_thread_running = 9;
      // fprintf(stderr, " ende\n");
    }
}
