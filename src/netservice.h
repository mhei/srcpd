/* $Id$ */

/*
 * Vorliegende Software unterliegt der General Public License,
 * Version 2, 1991. (c) Matthias Trute, 2000-2001.
 *
 */


#ifndef _NETSERVICE_H
#define _NETSERVICE_H

#include "config-srcpd.h"

typedef struct _thr_param
{
    bus_t busnumber;
} thr_param;

typedef struct _THREADS
{
  unsigned short int port;
  int socket;
  void *client_handler;
} net_thread_t;

void* thr_handlePort(void *);
void change_privileges(bus_t bus);

#endif
