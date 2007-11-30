/* $Id$ */

/*
 * Vorliegende Software unterliegt der General Public License,
 * Version 2, 1991. (c) Matthias Trute, 2000-2001.
 *
 */


#ifndef _SRCP_SERVER_H
#define _SRCP_SERVER_H

#include <libxml/tree.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/param.h>

#include "config.h" /*for VERSION*/
#include "config-srcpd.h" /*for bus_t*/


typedef struct _SERVER_DATA {
    unsigned short int TCPPORT;
    char *listenip;
    char PIDFILE[MAXPATHLEN];
    char *username;
    char *groupname;
} SERVER_DATA;

int readconfig_server(xmlDocPtr doc, xmlNodePtr node, bus_t busnumber);

int startup_SERVER(void);

int init_bus_server(bus_t);
int term_bus_server(bus_t);
void server_reset(void);
void server_shutdown(void);

int describeSERVER(bus_t bus, int addr, char *reply);
int infoSERVER(char *msg);
#endif

