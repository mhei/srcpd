/* $Id$ */

/* 
 * Vorliegende Software unterliegt der General Public License, 
 * Version 2, 1991. (c) Matthias Trute, 2000-2001.
 *
 */


#ifndef _SRCPGL_H_
#define _SRCPGL_H_

#include <sys/time.h>

#define MAXGLS 10240

/* Lokdekoder */
struct _GL
{
  int id;       /* Adresse, wird auch als Semaphor genutzt! */
  int speed;    /* Sollgeschwindigkeit skal. auf 0..14      */
  int maxspeed; /* Maximalgeschwindigkeit                   */
  int direction;/* 0/1/2                                    */
  int n_fkt;    /* 0 oder 4, Anzahl der Zusatzfunktionen    */
  int flags;   /* F1..F4, F                                */
  int n_fs;     /* Anzahl der "wahren" Fahrstufen des Dekoders */
  struct timeval tv; /* Last time of change                 */
  int locked_by;     /* Session ID */
};

extern volatile struct _GL gl[MAX_BUSSES][MAXGLS];   // aktueller Stand, mehr gibt es nicht
extern volatile struct _GL ngl[MAX_BUSSES][50];      // max. 50 neue Werte puffern
extern volatile struct _GL ogl[MAX_BUSSES][50];      // manuelle Änderungenx 
int setGL(int sessionid, int bus, int addr, int dir, int speed, int maxspeed, int f, 
     int n_fkt, int f1, int f2, int f3, int f4);
     
int getGL(int bus, int addr, struct _GL *l);
void infoGL(struct _GL gl, char* msg);
int cmpGL(struct _GL a, struct _GL b);
int initGL(int sessionid);
int calcspeed(int vs, int vmax, int n_fs);

#endif
