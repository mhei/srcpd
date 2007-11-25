/* cvs: $Id$             */

/* 
 * Vorliegende Software unterliegt der General Public License, 
 * Version 2, 1991. (c) Matthias Trute, 2000-2001.
 *
 */


#ifndef _CLIENTSERVICE_H
#define _CLIENTSERVICE_H


typedef struct CLIENT_THREAD
{
  int socket;
  sessionid_t session;
  int mode;
} client_thread_t;

void* thr_doClient(void *v);

#endif
