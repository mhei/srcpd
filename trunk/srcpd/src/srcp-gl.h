/* $Id$ */

/* 
 * Vorliegende Software unterliegt der General Public License, 
 * Version 2, 1991. (c) Matthias Trute, 2000-2001.
 *
 */


#ifndef _SRCP_GL_H
#define _SRCP_GL_H

#include <sys/time.h>

/* Lokdekoder */
struct _GLSTATE
{
  char protocol[5];
  int protocolversion;
  int n_func;
  int n_fs;
  int id;       /* Adresse, wird auch als Semaphor genutzt! */
  int speed;    /* Sollgeschwindigkeit skal. auf 0..14      */
  int maxspeed; /* Maximalgeschwindigkeit                   */
  int direction;/* 0/1/2                                    */
  char funcs;   /* F1..F4, F                                */
  struct timeval tv; /* Last time of change                 */
  long int locked_by;
};

typedef struct _GL
{
  int numberOfGl;
  struct _GLSTATE *glstate;
} GL;

int startup_GL(void);
int init_GL(int bus, int number);
int get_number_gl(int bus);

int queueGL(int bus, int addr, int dir, int speed, int maxspeed, int f, 
      int f1, int f2, int f3, int f4);
int queue_GL_isempty(int bus);
int unqueueNextGL(int bus, struct _GLSTATE *l);

int getGL(int bus, int addr, struct _GLSTATE *l);
int setGL(int bus, int addr, struct _GLSTATE l);
int infoGL(int bus, int addr, char* info);
int describeGL(int bus, int addr, char *msg);
int initGL(int bus, int addr, const char *protocol, int protoversion, int n_fs, int n_func);
int termGL(int bus, int addr);
int isInitializedGL(int bus, int addr);

int lockGL(int bus, int addr, long int sessionid);
int getlockGL(int bus, int addr, long int sessionid);
int unlockGL(int bus, int addr, long int sessionid);
void unlock_gl_bysessionid(long int sessionid);

#endif
