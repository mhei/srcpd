/* $Id$ */

/* 
 * Vorliegende Software unterliegt der General Public License, 
 * Version 2, 1991. (c) Matthias Trute, 2000-2001.
 *
 * 25.12.2001 Matthias Trute
 *            - Queue geändert
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
#include <pthread.h>

#include "config-srcpd.h"
#include "srcp-error.h"
#include "srcp-gl.h"

/* aktueller Stand */
volatile struct _GL gl[MAX_BUSSES][MAXGLS];   // aktueller Stand, mehr gibt es nicht

/* queue fuer neue Aktionen */
volatile struct _GL ngl[MAX_BUSSES][QUEUEGL_LEN];      // max. 50 neue Werte puffern
volatile int writer_gl[MAX_BUSSES];
volatile int reader_gl[MAX_BUSSES];
pthread_mutex_t writer_gl_mut[MAX_BUSSES]; // = PTHREAD_MUTEX_INITIALIZER;

volatile struct _GL ogl[MAX_BUSSES][QUEUEGL_LEN];      // manuelle Änderungen

/* Übernehme die neuen Angaben für die Lok, einige wenige Prüfungen */
int setGL(int bus, int addr, int dir, int speed, int maxspeed, int f, 
     int f1, int f2, int f3, int f4)
{
  int i;
  struct timeval akt_time;
  pthread_mutex_lock(&writer_gl_mut[bus]);
  i = writer_gl[bus]; /* das ist jetzt unsere Lok */
  ngl[bus][i].speed     = speed;
  ngl[bus][i].maxspeed  = maxspeed;
  ngl[bus][i].direction = dir;
  ngl[bus][i].n_fkt     = gl[bus][addr].n_fkt;
  ngl[bus][i].flags     = f1 + (f2 << 1) + (f3 << 2) + (f4 << 3) + (f << 4);
  ngl[bus][i].n_fs      = gl[bus][addr].n_fs;
  gettimeofday(&akt_time, NULL);
  ngl[bus][i].tv        = akt_time;
  ngl[bus][i].id        = addr;
  syslog(LOG_INFO, "GL %i Position %i", addr, i);

  writer_gl[bus]++;    /* der nächste bekommt die hier */
  if(writer_gl[bus]>=QUEUEGL_LEN) writer_gl[bus]=0;
  pthread_mutex_unlock(&writer_gl_mut[bus]);
  return SRCP_OK;
}

int getGL(int bus, int addr, struct _GL *l)
{
  if((addr>0) && (addr <= busses[bus].number_gl))
  {
    *l = gl[bus][addr];
    return SRCP_OK;
  }
  else
  {
    return SRCP_NODATA;
  }
}

int infoGL(int bus, int addr, char* msg)
{
  if((addr>0) && (addr <= busses[bus].number_gl))
  {
  sprintf(msg, "%d GL %d %d %d %d %d %d %d %d %d",
      bus, addr, gl[bus][addr].direction, gl[bus][addr].speed, gl[bus][addr].maxspeed, 
      (gl[bus][addr].flags & 0x10)?1:0, 
      (gl[bus][addr].flags & 0x01)?1:0,
      (gl[bus][addr].flags & 0x02)?1:0,
      (gl[bus][addr].flags & 0x04)?1:0,
      (gl[bus][addr].flags & 0x08)?1:0);
  }
  else
  {
    return SRCP_NODATA;
  }
  return SRCP_INFO;
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

int initGL()
{
  int bus, i;
  for(bus=0; bus<MAX_BUSSES; bus++) {
      //      writer_gl_mut[bus] = PTHREAD_MUTEX_INITIALIZER;

    for(i=0; i<MAXGLS;i++)
    {
      gl[bus][i].direction = 1;
      gl[bus][i].id = i;
    }
    for(i=0;i<QUEUEGL_LEN;i++)
    {
      ngl[bus][i].direction = 1;
      ngl[bus][i].id = 0;
      ogl[bus][i].direction = 1;
      ogl[bus][i].id = 0;
    }
  }
  return SRCP_OK;
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
