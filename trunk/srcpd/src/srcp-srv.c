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

int readconfig_server(xmlDocPtr doc, xmlNodePtr node, long int busnumber)
{
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
  __srv->listenip = NULL;

  xmlNodePtr child = node->children;
  xmlChar *txt = NULL;

  while (child)
  {
      if (xmlStrncmp(child->name, BAD_CAST "text", 4) == 0)
      {
          child = child -> next;
          continue;
      }

      if (xmlStrcmp(child->name, BAD_CAST "tcp-port") == 0)
      {
          txt = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
          if (txt != NULL) {
              __srv->TCPPORT = atoi((char*) txt);
              xmlFree(txt);
          }
      }

      if (xmlStrcmp(child->name, BAD_CAST "listen-ip") == 0)
      {
          txt = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
          if (txt != NULL) {
              xmlFree(__srv->listenip);
              __srv->listenip = malloc(strlen((char*) txt) + 1);
              strcpy(__srv->listenip, (char*) txt);
              DBG(busnumber, DBG_INFO, "listen-ip: %s", txt);
              xmlFree(txt);
          }
      }

      if (xmlStrcmp(child->name, BAD_CAST "pid-file") == 0)
      {
          txt = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
          if (txt != NULL) {
              strncpy(__srv->PIDFILE, (char*) txt, MAXPATHLEN-2);
              __srv->PIDFILE[MAXPATHLEN-1] = 0x00;
              xmlFree(txt);
          }
      }

      if (xmlStrcmp(child->name, BAD_CAST "username") == 0)
      {
          txt = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
          if (txt != NULL) {
              xmlFree(__srv->username);
              __srv->username = malloc(strlen((char*) txt) + 1);
              if (__srv->username == NULL)
              {
                  printf("Cannot allocate memory\n");
                  exit(1);
              }
              strcpy(__srv->username, (char*) txt);
              xmlFree(txt);
          }
      }

      if (xmlStrcmp(child->name, BAD_CAST "groupname") == 0)
      {
          txt = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
          if (txt != NULL) {
              xmlFree(__srv->groupname);
              __srv->groupname = malloc(strlen((char*) txt) + 1);
              if (__srv->groupname == NULL)
              {
                  printf("Cannot allocate memory\n");
                  exit(1);
              }
              strcpy(__srv->groupname, (char*) txt);
              xmlFree(txt);
          }
      }

      child = child->next;
  }

  return(1);
}


int startup_SERVER(void)
{
  return 0;
}

int describeSERVER(long int bus, int addr, char *reply)
{
  return SRCP_UNSUPPORTEDOPERATION;
}

long int init_bus_server(long int bus)
{
  gettimeofday(& busses[0].power_change_time, NULL);
  DBG(bus, DBG_INFO, "init_bus %d", bus);
  return 0;
}

long int term_bus_server(long int bus)
{
  DBG(bus, DBG_INFO, "term_bus %d", bus);
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
