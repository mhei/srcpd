/* $Id$ */

/* 
 * Vorliegende Software unterliegt der General Public License, 
 * Version 2, 1991. (c) Matthias Trute, 2000-2001.
 *
 */


#ifndef _SRCPFB_H_
#define _SRCPFB_H_

#include <sys/time.h>

#define MAXFBS 32

typedef struct _FBSTATE {
    struct timeval timestamp;
    short int state;
} FBSTATE;

int getFB(int bus, int port);

void updateFB(int bus, int port, int value);

/* setzt 16 Binärports auf einmal, für alle S88 Routinen */
void setFBmodule(int bus, int mod, int values);

int infoFB(int bus, int port, char *msg);

#endif
