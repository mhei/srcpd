/* $Id$ */

/* 
 * Vorliegende Software unterliegt der General Public License, 
 * Version 2, 1991. (c) Matthias Trute, 2000-2001.
 *
 * 04.07.2001 Frank Schmischke
 *            - Feld für Vormerkungen wurde auf 50 reduziert
 *            - Position in Vormerkung ist jetzt unabhängig von der
 *              Decoderadresse
 *            - Prüfungen auf Protokoll fü|r Motorola wurden entfernt, da IB
 *              auch NMRA verarbeitet
 */


#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <sys/time.h>

#include "config-srcpd.h"
#include "srcp-gl.h"

volatile struct _GL gl[MAX_BUSSES][MAXGLS];   // aktueller Stand, mehr gibt es nicht
volatile struct _GL ngl[MAX_BUSSES][50];      // max. 50 neue Werte puffern
volatile struct _GL ogl[MAX_BUSSES][50];      // manuelle Änderungen

/* Übernehme die neuen Angaben für die Lok, einige wenige Prüfungen */
void setGL(int bus, int addr, int dir, int speed, int maxspeed, int f, 
     int n_fkt, int f1, int f2, int f3, int f4)
{
  int i, n_fs;
  struct timeval akt_time;

  n_fs = gl[bus][addr].n_fs;

  /* Daten einfüllen, aber nur, wenn id == 0!!, darauf warten wir max. 1 Sekunde */
  if((addr > 0) && (addr <= busses[bus].number_gl))
  {
    for(i=0;i<1000;i++)
    {
      if(busses[bus].sending_gl == 0)
        break;
      usleep(100);
    }

    for(i=0;i<50;i++)
    {
      if(ngl[bus][i].id == addr)
      {
        ngl[bus][i].id = 0;
        break;
      }
    }
    if(i == 50)
    {
      for(i=0;i<50;i++)
      {
        if(ngl[bus][i].id == 0)
          break;
      }
    }
    if(i < 50)
    {
      ngl[bus][i].speed     = speed;
      ngl[bus][i].maxspeed  = maxspeed;
      ngl[bus][i].direction = dir;
      ngl[bus][i].n_fkt     = n_fkt;
      ngl[bus][i].flags     = f1 + (f2 << 1) + (f3 << 2) + (f4 << 3) + (f << 4);
      ngl[bus][i].n_fs      = n_fs;
      gettimeofday(&akt_time, NULL);
      ngl[bus][i].tv        = akt_time;
      ngl[bus][i].id        = addr;
      syslog(LOG_INFO, "GL %i Position %i", addr, i);
      busses[bus].command_gl = 1;
    }
  }
}

int getGL(int bus, int addr, struct _GL *l)
{
  if((addr>0) && (addr <= busses[bus].number_gl))
  {
    *l = gl[bus][addr];
    return 1;
  }
  else
  {
    return 0;
  }
}

void infoGL(struct _GL gl, char* msg)
{
  sprintf(msg, "GL %d %d %d %d %d %d %d %d %d %d\n",
      gl.id, gl.direction, gl.speed, gl.maxspeed, 
      (gl.flags & 0x10)?1:0, 
      4, 
      (gl.flags & 0x01)?1:0,
      (gl.flags & 0x02)?1:0,
      (gl.flags & 0x04)?1:0,
      (gl.flags & 0x08)?1:0);
}

int cmpGL(struct _GL a, struct _GL b)
{
  return ((a.direction == b.direction) &&
      (a.speed     == b.speed)     &&
      (a.maxspeed  == b.maxspeed)  &&
      (a.n_fkt     == b.n_fkt)     &&
      (a.n_fs      == b.n_fs)      &&
      (a.flags     == b.flags));
}

void initGL()
{
  int bus, i;
  for(bus=0; bus<MAX_BUSSES; bus++) {
    for(i=0; i<MAXGLS;i++)
    {
      gl[bus][i].direction = 1;
      gl[bus][i].id = i;
    }
    for(i=0;i<50;i++)
    {
      ngl[bus][i].direction = 1;
      ngl[bus][i].id = 0;
      ogl[bus][i].direction = 1;
      ogl[bus][i].id = 0;
    }
  }
}

// es gibt Decoder für 14, 27, 28 und 128 FS
// Achtung bei IB alles auf 126 FS bezogen (wenn Ergebnis > 0, dann noch eins aufaddieren)
int calcspeed(int vs, int vmax, int n_fs)
{
  int rs;
  if(0==vmax)
    return vs;
  if(vs<0)
    vs = 0;
  if(vs>vmax)
    vs = vmax;
  rs = (vs * n_fs) / vmax;
  if((rs==0) && (vs!=0))
    rs = 1;
  return rs;
}
