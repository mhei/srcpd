/* $Id$ */

/* 
 * Vorliegende Software unterliegt der General Public License, 
 * Version 2, 1991. (c) Matthias Trute, 2000-2001.
 *
 */


#include "srcp-srv.h"
#include <libxml/tree.h>
#include <syslog.h>
#include "config-srcpd.h"
#include "srcp-error.h"

int server_reset_state;
int server_shutdown_state;

void readconfig_server(xmlDocPtr doc, xmlNodePtr node, int busnumber) {
    xmlNodePtr child = node->children;
   syslog(LOG_INFO, "Bus %d starting Konfiguration Child %s", busnumber, node->name);
    busses[0].type = SERVER_SERVER;
    busses[0].init_func = &init_bus_server;
    busses[0].term_func = &term_bus_server;
    strcpy(busses[0].description, "SESSION SERVER TIME");

    busses[0].driverdata = malloc(sizeof(struct _SERVER_DATA));

   strcpy(((SERVER_DATA *) busses[0].driverdata)->PIDFILE, "/var/run/srcpd.pid");
    while (child) {
          if(strncmp(child->name, "text", 4)==0) {
            child = child -> next;
            continue;
          }
//          syslog(LOG_INFO, "bus %d child %s", busnumber, child->name);
	    if (strcmp(child->name, "TCP-Port") == 0) {
            char *txt = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
		((SERVER_DATA *) busses[busnumber].driverdata)->TCPPORT = atoi(txt);
            free(txt);
	    }
	    if (strcmp(child->name, "PID-File") == 0) {
            char *txt = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
		strcpy(((SERVER_DATA *) busses[busnumber].driverdata)->PIDFILE, txt);
            free(txt);
	    }
	    if (strcmp(child->name, "username") == 0) {
            char *txt = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
		free(((SERVER_DATA *) busses[busnumber].driverdata)->username);
		((SERVER_DATA *) busses[busnumber].driverdata)->username = malloc(strlen(txt) + 1);
		if (((SERVER_DATA *) busses[busnumber].driverdata)->username == NULL) {
		    printf("cannot allocate memory\n");
		    exit(1);
		}
		strcpy(((SERVER_DATA *) busses[busnumber].driverdata)->username, txt);
            free(txt);
          }
	    if (strcmp(child->name, "groupname") == 0) {
            char *txt = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
		free(((SERVER_DATA *) busses[busnumber].driverdata)->groupname);
		((SERVER_DATA *) busses[busnumber].driverdata)->groupname = malloc(strlen(txt) + 1);
		if (((SERVER_DATA *) busses[busnumber].driverdata)->groupname == NULL) {
		    printf("cannot allocate memory\n");
		    exit(1);
		}
		strcpy(((SERVER_DATA *) busses[busnumber].driverdata)->groupname, txt);
            free(txt);
          }
          child = child->next;
    } /* while */
}


int
startup_SERVER(void)
{
  return 0;
}

int describeSERVER(int bus, int addr, char *reply) {
   return SRCP_NOTSUPPORTED;
}
int
init_bus_server(int bus)
{
  return 0;
}

int
term_bus_server(int bus)
{
  return 0;
}

void
server_reset()
{
  server_reset_state = 1;
}

void
server_shutdown()
{
  server_shutdown_state = 1;
}
