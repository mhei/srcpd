/* $Id$ */

/* 
 * Vorliegende Software unterliegt der General Public License, 
 * Version 2, 1991. (c) Matthias Trute, 2000-2001.
 *
 */


#ifndef _THREADS_H_
#define _THREADS_H_

typedef struct _THREADS {
    int socket;
    void *func;
} THREADS;

void* thr_handlePort(void *);

extern struct _VTIME vtime;

#endif
