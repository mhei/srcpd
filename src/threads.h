/* $Id$ */

/*
 * Vorliegende Software unterliegt der General Public License,
 * Version 2, 1991. (c) Matthias Trute, 2000-2001.
 *
 */


#ifndef _THREADS_H
#define _THREADS_H

typedef struct _THREADS
{
  int socket;
  void *func;
} THREADS;

void* thr_handlePort(void *);
void change_privileges(long int bus);

#endif
