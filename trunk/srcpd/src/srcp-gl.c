/* $Id$ */

/* 
 * Vorliegende Software unterliegt der General Public License, 
 * Version 2, 1991. (c) Matthias Trute, 2000-2001.
 *
 * 04.07.2001 Frank Schmischke
 *            - Feld f|r Vormerkungen wurde auf 50 reduziert
 *            - Position in Vormerkung ist jetzt unabhngig von der 
 *              Decoderadresse
 *            - Pr|fungen auf Protokoll f|r Motorola wurden entfernt, da IB
 *              auch NMRA verarbeitet
 */


#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <sys/time.h>

#include "config-srcpd.h"
#include "srcp-gl.h"

volatile struct _GL gl[MAXGLS];   // aktueller Stand, mehr gibt es nicht
volatile struct _GL ngl[50];      // max. 50 neue Werte puffern
volatile struct _GL ogl[50];      // manuelle Änderungen

volatile int commands_gl  = 0;
volatile int sending_gl		= 0;

int MAXGLS_SERVER[NUMBER_SERVERS] =
{
  10239, 80, 9999, 9999
};

extern int working_server;

/* Übernehme die neuen Angaben für die Lok, einige wenige Prüfungen */
void setGL(char *prot, int addr, int dir, int speed, int maxspeed, int f, 
	   int n_fkt, int f1, int f2, int f3, int f4)
{
  int i, n_fs;
  struct timeval akt_time;

  n_fs = 14;
  if(strncasecmp(prot, "M3", 2) == 0)
  {
	  n_fs = 28;
  }
  else
  {
    if(strncasecmp(prot, "M5", 2) == 0)
    {
	    n_fs = 27;
    }
    else
    {
      if(strncasecmp(prot, "N1", 2) == 0)
      {
	      n_fs = 28;
      }
      else
      {
        if(strncasecmp(prot, "N2", 2) == 0)
        {
        	n_fs = 128;
        }
        else
        {
          if(strncasecmp(prot, "N3", 2) == 0)
          {
          	n_fs = 28;
          }
          else
          {
            if(strncasecmp(prot, "N4", 2) == 0)
            {
            	n_fs = 128;
            }
          }
        }
      }
    }
  }
  syslog(LOG_INFO, "in setGL für %i", addr);

  /* Daten einfüllen, aber nur, wenn id == 0!!, darauf warten wir max. 1 Sekunde */
  if((addr > 0) && (addr < MAXGLS_SERVER[working_server]))
  {
    for(i=0;i<1000;i++)
    {
    	if(sending_gl == 0)
    		break;
    	usleep(100);
    }

  	for(i=0;i<50;i++)
	  {
	  	if(ngl[i].id == addr)
	  	{
	  		ngl[i].id = 0;
	  	  break;
	  	}
	  }
	  if(i == 50)
	  {
	  	for(i=0;i<50;i++)
		  {
	  		if(ngl[i].id == 0)
	  	  	break;
	  	}
    }
    if(i < 50)
    {
	    strcpy((void*)ngl[i].prot, prot);
			ngl[i].speed     = speed;
			ngl[i].maxspeed  = maxspeed;
			ngl[i].direction = dir;
			ngl[i].n_fkt     = n_fkt;
			ngl[i].flags     = f1 + (f2 << 1) + (f3 << 2) + (f4 << 3) + (f << 4);
	    ngl[i].n_fs      = n_fs;
	    gettimeofday(&akt_time, NULL);
		  ngl[i].tv        = akt_time;
			ngl[i].id 			 = addr;
      syslog(LOG_INFO, "GL %i Position %i", addr, i);
			commands_gl = 1;
		}
	}
}

int getGL(char *prot, int addr, struct _GL *l)
{
  if((addr>0) && (addr<MAXGLS_SERVER[working_server]))
  {
    *l = gl[addr];
    return 1;
  }
  else
  {
	  return 0;
	}
}

void infoGL(struct _GL gl, char* msg)
{
  sprintf(msg, "INFO GL %s %d %d %d %d %d %d %d %d %d %d\n",
	    gl.prot, gl.id, gl.direction, gl.speed, gl.maxspeed, 
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
  int i;
  for(i=0; i<MAXGLS;i++)
  {
	  strcpy((void *)gl[i].prot, "PS");
	  gl[i].direction = 1;
	  gl[i].id = i;
  }
  for(i=0;i<50;i++)
  {
	  strcpy((void *)ngl[i].prot, "PS");
	  ngl[i].direction = 1;
	  ngl[i].id = 0;
	  strcpy((void *)ogl[i].prot, "PS");
	  ogl[i].direction = 1;
	  ogl[i].id = 0;
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
