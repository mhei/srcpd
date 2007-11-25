/* $Id$ */

/*
 * Vorliegende Software unterliegt der General Public License,
 * Version 2, 1991. (c) Matthias Trute, 2000-2001.
 *
 */

#include "stdincludes.h"

#include "config-srcpd.h"
#include "srcp-server.h"
#include "srcp-error.h"
#include "srcp-info.h"

#include <netdb.h>

int server_reset_state;
int server_shutdown_state;

#define __srv ((SERVER_DATA*)buses[busnumber].driverdata)

int readconfig_server(xmlDocPtr doc, xmlNodePtr node, bus_t busnumber)
{
    struct servent *serviceentry;
    
    DBG(busnumber, DBG_INFO, "Reading configuration for bus '%s'",
        node->name);

    buses[0].driverdata = malloc(sizeof(struct _SERVER_DATA));

    /* if there is too less memory for server data -> exit process */
    if (buses[0].driverdata == NULL) {
        DBG(busnumber, DBG_ERROR,
                "Memory allocation error in module '%s'.", node->name);
        exit(1);
    }

    buses[0].type = SERVER_SERVER;
    buses[0].init_func = &init_bus_server;
    buses[0].term_func = &term_bus_server;
    strcpy(buses[0].description, "SESSION SERVER TIME GM");

    /* initialize _SERVER_DATA with defaults */
    serviceentry = getservbyname("srcp","tcp");
    if (serviceentry == NULL) {
	__srv->TCPPORT = 4303;
    } else {
	__srv->TCPPORT = ntohs(serviceentry->s_port);
    }
    strcpy(__srv->PIDFILE, "/var/run/srcpd.pid");
    __srv->groupname = NULL;
    __srv->username = NULL;
    __srv->listenip = NULL;

    xmlNodePtr child = node->children;
    xmlChar *txt = NULL;

    while (child != NULL) {
        if (xmlStrncmp(child->name, BAD_CAST "text", 4) == 0) {
            /* just do nothing, it is only a comment */
        }

        else if (xmlStrcmp(child->name, BAD_CAST "tcp-port") == 0) {
            txt = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
            if (txt != NULL) {
                __srv->TCPPORT = atoi((char *) txt);
                xmlFree(txt);
            }
        }

        else if (xmlStrcmp(child->name, BAD_CAST "listen-ip") == 0) {
            txt = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
            if (txt != NULL) {
                xmlFree(__srv->listenip);
                __srv->listenip = malloc(strlen((char *) txt) + 1);
                strcpy(__srv->listenip, (char *) txt);
                DBG(busnumber, DBG_INFO, "listen-ip: %s", txt);
                xmlFree(txt);
            }
        }

        else if (xmlStrcmp(child->name, BAD_CAST "pid-file") == 0) {
            txt = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
            if (txt != NULL) {
                strncpy(__srv->PIDFILE, (char *) txt, MAXPATHLEN - 2);
                __srv->PIDFILE[MAXPATHLEN - 1] = 0x00;
                xmlFree(txt);
            }
        }

        else if (xmlStrcmp(child->name, BAD_CAST "username") == 0) {
            txt = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
            if (txt != NULL) {
                xmlFree(__srv->username);
                __srv->username = malloc(strlen((char *) txt) + 1);
                if (__srv->username == NULL) {
                    DBG(busnumber, DBG_ERROR, "Cannot allocate memory\n");
                    exit(1);
                }
                strcpy(__srv->username, (char *) txt);
                xmlFree(txt);
            }
        }

        else if (xmlStrcmp(child->name, BAD_CAST "groupname") == 0) {
            txt = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
            if (txt != NULL) {
                xmlFree(__srv->groupname);
                __srv->groupname = malloc(strlen((char *) txt) + 1);
                if (__srv->groupname == NULL) {
                    DBG(busnumber, DBG_ERROR, "Cannot allocate memory\n");
                    exit(1);
                }
                strcpy(__srv->groupname, (char *) txt);
                xmlFree(txt);
            }
        }

        else
            DBG(busnumber, DBG_WARN,
                    "WARNING, unknown tag found: \"%s\"!\n",
                    child->name);;
        
        child = child->next;
    }

    return (1);
}


int startup_SERVER(void)
{
    return 0;
}

int describeSERVER(bus_t bus, int addr, char *reply)
{
    return SRCP_UNSUPPORTEDOPERATION;
}

int init_bus_server(bus_t bus)
{
    gettimeofday(&buses[0].power_change_time, NULL);
    DBG(bus, DBG_INFO, "init_bus %ld", bus);
    return 0;
}

int term_bus_server(bus_t bus)
{
    DBG(bus, DBG_INFO, "term_bus %ld", bus);
    return 0;
}

void server_reset()
{
    char msg[100];
    server_reset_state = 1;
    infoSERVER(msg);
    queueInfoMessage(msg);
}

void server_shutdown()
{
    char msg[100];
    server_shutdown_state = 1;
    infoSERVER(msg);
    queueInfoMessage(msg);
}

int infoSERVER(char *msg)
{
    struct timeval akt_time;
    gettimeofday(&akt_time, NULL);
    if (server_reset_state == 1) {
        sprintf(msg, "%lu.%.3lu 100 INFO 0 SERVER RESETTING\n",
                akt_time.tv_sec, akt_time.tv_usec / 1000);
    }
    else {
        if (server_shutdown_state == 1) {
            sprintf(msg, "%lu.%.3lu 100 INFO 0 SERVER TERMINATING\n",
                    akt_time.tv_sec, akt_time.tv_usec / 1000);
        }
        else {
            sprintf(msg, "%lu.%.3lu 100 INFO 0 SERVER RUNNING\n",
                    akt_time.tv_sec, akt_time.tv_usec / 1000);
        }
    }
    return SRCP_OK;
}
