/* $Id$ */

/* 
 * Vorliegende Software unterliegt der General Public License, 
 * Version 2, 1991. (c) Matthias Trute, 2000-2001.
 *
 */


#ifndef _SRCP_SRV_H
#define _SRCP_SRV_H

#include <libxml/tree.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/param.h>

typedef struct _SERVER_DATA {
    int TCPPORT;
    char *listenip;
    char PIDFILE[MAXPATHLEN];
    char *username;
    char *groupname;
} SERVER_DATA;

void readconfig_server(xmlDocPtr doc, xmlNodePtr node, int busnumber);

int startup_SERVER(void);

int init_bus_server(int);
int term_bus_server(int);

void server_reset(void);
void server_shutdown(void);

int describeSERVER(int bus, int addr, char *reply);
int infoSERVER( char *msg);
#endif

