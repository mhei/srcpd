/* $Id$ */

/* 
 * Vorliegende Software unterliegt der General Public License, 
 * Version 2, 1991. (c) Matthias Trute, 2000-2001.
 *
 */


#ifndef _SRCP_FB_H
#define _SRCP_FB_H

#include <sys/time.h>

typedef struct _FBSTATE
{
  struct timeval timestamp;
  short int state;
  short int change;
} FBSTATE;

typedef struct _FB
{
  int numberOfFb;
  struct _FBSTATE *fbstate;
} FB;

typedef struct _RESET_FB
{
  int port;
  struct timeval timestamp;
} reset_FB;

int startup_FB(void);
int init_FB(int bus, int number);
int get_number_fb(int bus);

int getFB(int bus, int port, struct timeval *time, int *value);
void updateFB(int bus, int port, int value);
int setFBmodul(int bus, int mod, int values);
int infoFB(int bus, int port, char *msg);
int describeFB(int bus, int addr, char *reply);
void check_reset_fb(void);
void set_min_time(int busnumber, int mt);
#endif
