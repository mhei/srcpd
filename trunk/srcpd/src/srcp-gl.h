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
  int direction;/* 0/1/2                                    */
  char funcs;   /* F1..F4, F                                */
  struct timeval tv; /* Last time of change                 */
  struct timeval inittime;
  struct timeval locktime;
  long int locked_by;
};

typedef struct _GL
{
  int numberOfGl;
  struct _GLSTATE *glstate;
} GL;

int startup_GL(void);
int init_GL(int busnumber, int number);
int get_number_gl(int busnumber);

int queueGL(int busnumber, int addr, int dir, int speed, int maxspeed, int f, 
      int f1, int f2, int f3, int f4);
int queue_GL_isempty(int busnumber);
int unqueueNextGL(int busnumber, struct _GLSTATE *l);

int getGL(int busnumber, int addr, struct _GLSTATE *l);
int setGL(int busnumber, int addr, struct _GLSTATE l);
int infoGL(int busnumber, int addr, char* info);
int describeGL(int busnumber, int addr, char *msg);
int initGL(int busnumber, int addr, const char *protocol, int protoversion, int n_fs, int n_func);
int termGL(int busnumber, int addr);
int isInitializedGL(int busnumber, int addr);

int lockGL(int busnumber, int addr, long int sessionid);
int getlockGL(int busnumber, int addr, long int *sessionid);
int unlockGL(int busnumber, int addr, long int sessionid);
void unlock_gl_bysessionid(long int sessionid);
int describeLOCKGL(int bus, int addr, char *reply);

#endif
