/* $Id$ */

/* 
 * Vorliegende Software unterliegt der General Public License, 
 * Version 2, 1991. (c) Matthias Trute, 2000-2001.
 *
 */


#ifndef _SRCP_GL_H
#define _SRCP_GL_H

#include <sys/time.h>

#define MAXGLS 10240

/* Lokdekoder */
struct _GL
{
  char protocol[5];
  int protoversion;
  int n_func;
  int n_fs;
  int id;       /* Adresse, wird auch als Semaphor genutzt! */
  int speed;    /* Sollgeschwindigkeit skal. auf 0..14      */
  int maxspeed; /* Maximalgeschwindigkeit                   */
  int direction;/* 0/1/2                                    */
  char funcs;   /* F1..F4, F                                */
  struct timeval tv; /* Last time of change                 */
};

int startup_GL(void);

int queueGL(int bus, int addr, int dir, int speed, int maxspeed, int f, 
      int f1, int f2, int f3, int f4);
int queue_GL_isempty(int bus);
int unqueueNextGL(int bus, struct _GL *);

int getGL(int bus, int addr, struct _GL *l);
int setGL(int bus, int addr, struct _GL l);
int infoGL(int bus, int addr, char* info);
int initGL(int bus, int addr, const char *protocol, int protoversion, int n_fs, int n_func);
int termGL(int bus, int addr);

int cmpGL(struct _GL a, struct _GL b);
int calcspeed(int vs, int vmax, int n_fs);
#endif
