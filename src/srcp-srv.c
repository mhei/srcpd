/* $Id$ */

/* 
 * Vorliegende Software unterliegt der General Public License, 
 * Version 2, 1991. (c) Matthias Trute, 2000-2001.
 *
 */

#include "stdincludes.h"

#include "srcp-srv.h"
#include "config-srcpd.h"
#include "srcp-error.h"
#include "srcp-info.h"

int server_reset_state;
int server_shutdown_state;

#define __srv ((SERVER_DATA*)busses[busnumber].driverdata)

void readconfig_server(xmlDocPtr doc, xmlNodePtr node, int busnumber)
{
  xmlNodePtr child = node->children;
  DBG(busnumber, DBG_INFO, "bus %d starting configuration child %s", busnumber, node->name);
  busses[0].type = SERVER_SERVER;
  busses[0].init_func = &init_bus_server;
  busses[0].term_func = &term_bus_server;
  strcpy(busses[0].description, "SESSION SERVER TIME");

  busses[0].driverdata = malloc(sizeof(struct _SERVER_DATA));
  // initialize _SERVER_DATA with defaults
  __srv->TCPPORT = 12345;
  strcpy(__srv->PIDFILE, "/var/run/srcpd.pid");
  __srv->groupname = NULL;
  __srv->username = NULL;

  while (child)
  {
    if (strncmp(child->name, "text", 4) == 0)
    {
      child = child -> next;
      continue;
    }
//    syslog(LOG_INFO, "bus %d child %s", busnumber, child->name);
    if (strcmp(child->name, "tcp-port") == 0)
    {
      char *txt = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
      __srv->TCPPORT = atoi(txt);
      free(txt);
    }
    if (strcmp(child->name, "pid-file") == 0)
    {
      char *txt = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
      strcpy(__srv->PIDFILE, txt);
      free(txt);
    }
    if (strcmp(child->name, "username") == 0)
    {
      char *txt = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
      if (__srv->username != NULL)
        free(__srv->username);
      __srv->username = malloc(strlen(txt) + 1);
      if (__srv->username == NULL)
      {
        printf("cannot allocate memory\n");
        exit(1);
      }
      strcpy(__srv->username, txt);
      free(txt);
    }
    if (strcmp(child->name, "groupname") == 0)
    {
      char *txt = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
      if (__srv->groupname != NULL)
        free(__srv->groupname);
      __srv->groupname = malloc(strlen(txt) + 1);
      if (__srv->groupname == NULL)
      {
        printf("cannot allocate memory\n");
        exit(1);
      }
      strcpy(__srv->groupname, txt);
      free(txt);
    }
    child = child->next;
  } /* while */
}


int startup_SERVER(void)
{
  return 0;
}

int describeSERVER(int bus, int addr, char *reply)
{
  return SRCP_UNSUPPORTEDOPERATION;
}

int init_bus_server(int bus)
{
  gettimeofday(& busses[0].power_change_time, NULL);
  DBG(bus, DBG_INFO, "init_bus %d", bus);
  return 0;
}

int term_bus_server(int bus)
{
  DBG(bus, DBG_INFO, "term_bus %d", bus);
  return 0;
}

void server_reset()
{
  server_reset_state = 1;
}

void server_shutdown()
{
  char msg[100];
  struct timeval akt_time;
  gettimeofday(&akt_time, NULL);  
  sprintf(msg, "%lu.%.3lu 100 INFO 0 SERVER TERMINATING\n", akt_time.tv_sec, akt_time.tv_usec/1000);
  server_shutdown_state = 1;
  queueInfoMessage(msg);
}

int infoSERVER( char *msg) {
  struct timeval akt_time;
  gettimeofday(&akt_time, NULL);
  if(server_reset_state == 1) {
    sprintf(msg, "%lu.%.3lu 100 INFO 0 SERVER RESETTING\n", akt_time.tv_sec, akt_time.tv_usec/1000);
  } else {
    if(server_shutdown_state==1) {
       sprintf(msg, "%lu.%.3lu 100 INFO 0 SERVER TERMINATING\n", akt_time.tv_sec, akt_time.tv_usec/1000);      
    } else {
       sprintf(msg, "%lu.%.3lu 100 INFO 0 SERVER RUNNING\n", akt_time.tv_sec, akt_time.tv_usec/1000);            
    }
  }
  return SRCP_OK;
}