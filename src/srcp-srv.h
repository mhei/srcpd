/* $Id$ */

/* 
 * Vorliegende Software unterliegt der General Public License, 
 * Version 2, 1991. (c) Matthias Trute, 2000-2001.
 *
 */


#ifndef _SRCP_SRV_H
#define _SRCP_SRV_H

int startup_SERVER(void);

int init_bus_server(int);
int term_bus_server(int);

void server_reset(void);
void server_shutdown(void);

#endif

