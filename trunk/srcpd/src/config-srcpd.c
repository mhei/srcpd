/* cvs: $Id$             */

/*
 * Vorliegende Software unterliegt der General Public License,
 * Version 2, 1991. (c) Matthias Trute, 2000-2001.
 *
 * 04.07.2001 Frank Schmischke
 *            Einführung der Konfigurationsdatei
 * 05.08.2001 Matthias Trute
 *        Umstellung auf XML Format
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <syslog.h>


#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

#include "config-srcpd.h"
#include "srcp-srv.h"
#include "m605x.h"
#include "ib.h"
#include "loopback.h"
#include "ddl-s88.h"
#include "hsi-88.h"

/* Willkommensmeldung */
const char *WELCOME_MSG = "srcpd V2; SRCP 0.8.x; do not use\n";

struct _BUS busses[MAX_BUSSES];
int num_busses;


// check that a real server is running on bus greather then zero
static void check_bus(int busnumber)
{
  if (busnumber == 0)
  {
    printf("Sorry, at bus 0 is only tag \"server\" allowed!\n");
    exit(1);
  }
}

static void register_bus(xmlDocPtr doc, xmlNodePtr node)
{
  xmlNodePtr child;
  char *proptxt;
  int busnumber;
  int found;

  if (strcmp(node->name, "bus"))
    return; // only bus definitions

  proptxt = xmlGetProp(node, "number");
  busnumber = atoi(proptxt);
  free(proptxt);
  if (busnumber > MAX_BUSSES || busnumber < 0)
  {
    // you need to recompile
    return;
  }

  if (busnumber <= num_busses)
  {
    printf("Sorry, there is an error in your srcpd.conf !\n");
    printf("bus %i is following bus %i, this is not allowed!",
      busnumber, num_busses);
    exit(1);
  }
  busses[busnumber].number = busnumber;
  num_busses = busnumber;
  child = node->children;
  while (child)
  {
    char *txt;
    if (strcmp(child->name, "text") == 0)
    {
      child = child->next;
      syslog(LOG_INFO, "ignoring");
      continue;
    }
    txt = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
    found = 0;
    if (strncmp(child->name, "server", 6) == 0)
    {
      if (busnumber == 0)
      {
        readconfig_server(doc, child, busnumber);
        found = 1;
      }
      else
      {
        printf("Sorry, tag \"server\" is only at bus 0 allowed!\n");
      }
    }
    /* but the most important are not ;=)  */
    if (strcmp(child->name, "m605x") == 0)
    {
      check_bus(busnumber);
      readconfig_m605x(doc, child, busnumber);
      found = 1;
    }
    if (strcmp(child->name, "intellibox") == 0)
    {
      check_bus(busnumber);
      readconfig_intellibox(doc, child, busnumber);
      found = 1;
    }
    if (strcmp(child->name, "loopback") == 0)
    {
      check_bus(busnumber);
      readconfig_loopback(doc, child, busnumber);
      found = 1;
    }
    if (strcmp(child->name, "ddl-S88") == 0)
    {
      check_bus(busnumber);
      readconfig_DDL_S88(doc, child, busnumber);
      found = 1;
    }
    if (strcmp(child->name, "hsi-88") == 0)
    {
      check_bus(busnumber);
      readconfig_HSI_88(doc, child, busnumber);
      found = 1;
    }
    /* some attributes are common for all (real) busses */
    if (strcmp(child->name, "device") == 0)
    {
      found = 1;
      if (busses[busnumber].device != NULL)
        free(busses[busnumber].device);
      busses[busnumber].device = malloc(strlen(txt) + 1);
      if (busses[busnumber].device == NULL)
      {
        printf("cannot allocate memory\n");
        exit(1);
      }
      strcpy(busses[busnumber].device, txt);
    }
    if (strcmp(child->name, "verbosity") == 0)
    {
      found = 1;
      busses[busnumber].debuglevel = atoi(txt);
    }
    if (strcmp(child->name, "use_watchdog") == 0)
    {
      found = 1;
      if (strcmp(txt, "yes") == 0)
        busses[busnumber].flags |= USE_WATCHDOG;
    }
    if (strcmp(child->name, "restore_device_settings") == 0)
    {
      found = 1;
      if (strcmp(txt, "yes") == 0)
        busses[busnumber].deviceflags |= RESTORE_COM_SETTINGS;
    }
    if (!found)
    {
      printf("WARNING, \"%s\" is an unknown tag!\n", child->name);
    }
    free(txt);
    child = child->next;
  }
}

static int walk_config_xml(xmlDocPtr doc)
{
  xmlNodePtr root, child;

  root = xmlDocGetRootElement(doc);
  child = root->children;

  while (child)
  {
    register_bus(doc, child);
    child = child->next;
  }
}

static xmlDocPtr load_config_xml(const char *filename)
{
  return xmlParseFile(filename);
}

int readConfig(const char *filename)
{
  xmlDocPtr doc;
  int i;

  // something to initialize
  memset(busses, 0, sizeof(busses));
  for(i=0;i<MAX_BUSSES;i++)
    busses[i].number = -1;
  num_busses = -1;

  /* some defaults */
  doc = load_config_xml(filename);
  if (doc != 0)
  {
    syslog(LOG_INFO, "parsing %s", filename);
    walk_config_xml(doc);
    syslog(LOG_INFO, " %s done", filename);
    xmlFreeDoc(doc);
  }
  else
  {
    exit(1);
  }
}
