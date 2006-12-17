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
  int state ;   /* 0==dead, 1==living, 2==terminating */
  char protocol;
  int protocolversion;
  int n_func;
  int n_fs;
  int id;       /* Adresse  */
  int speed;    /* Sollgeschwindigkeit skal. auf 0..14      */
  int direction;/* 0/1/2                                    */
  int funcs;   /* Fx..F1, F                                */
  struct timeval tv; /* Last time of change                 */
  struct timeval inittime;
  struct timeval locktime;
  long int lockduration;
  long int locked_by;
};

typedef struct _GL
{
  int numberOfGl;
  struct _GLSTATE *glstate;
} GL;

int startup_GL(void);
int init_GL(long int busnumber, int number);
int getMaxAddrGL(long int busnumber);
int isInitializedGL(long int busnumber, int addr);
int isValidGL(long int busnumber, int addr);

int queueGL(long int busnumber, int addr, int dir, int speed, int maxspeed, int f);
int queue_GL_isempty(long int busnumber);
int unqueueNextGL(long int busnumber, struct _GLSTATE *l);

int getGL(long int busnumber, int addr, struct _GLSTATE *l);
int setGL(long int busnumber, int addr, struct _GLSTATE l);
int infoGL(long int busnumber, int addr, char* info);
int describeGL(long int busnumber, int addr, char *msg);
int initGL(long int busnumber, int addr, const char protocol, int protoversion, int n_fs, int n_func);
int termGL(long int busnumber, int addr);

int lockGL(long int busnumber, int addr, long int duration, long int sessionid);
int getlockGL(long int busnumber, int addr, long int *sessionid);
int unlockGL(long int busnumber, int addr, long int sessionid);
void unlock_gl_bysessionid(long int sessionid);
void unlock_gl_bytime(void);
int describeLOCKGL(long int bus, int addr, char *reply);

void debugGL(long int busnumber, int start, int end);

#endif
