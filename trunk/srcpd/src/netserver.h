/* cvs: $Id$             */

/* 
 * Vorliegende Software unterliegt der General Public License, 
 * Version 2, 1991. (c) Matthias Trute, 2000-2001.
 *
 */


#ifndef _NETSERVER_H_
#define _NETSERVER_H_

int   socket_readline(int socket, char *line, int len);
void* thr_doCmdClient(void* v);
void* thr_doFBClient(void* v);
void* thr_doInfoClient(void *v);

#endif
