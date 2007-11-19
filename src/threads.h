/* $Id$ */

/*
 * Vorliegende Software unterliegt der General Public License,
 * Version 2, 1991. (c) Matthias Trute, 2000-2001.
 *
 */


#ifndef _THREADS_H
#define _THREADS_H

typedef struct _thr_param
{
    bus_t busnumber;
} thr_param;

typedef struct _THREADS
{
  unsigned short int port;
  int socket;
  void *client_handler;
} net_thread_data;

void* thr_handlePort(void *);
void change_privileges(bus_t bus);

#endif
