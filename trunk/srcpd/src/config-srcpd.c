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


static void register_bus(xmlDocPtr doc, xmlNodePtr node)
{
    xmlNodePtr child;
    char *proptxt;
    int busnumber;

    if (strcmp(node->name, "bus"))
	return;	// only bus definitions

    proptxt = xmlGetProp(node, "number");
    busnumber = atoi(proptxt);
    free(proptxt);
    if (busnumber > MAX_BUSSES || busnumber < 0) {
	// you need to recompile
	return;
    }

    busses[busnumber].number = busnumber;
    num_busses = busnumber;
    child = node->children;
    while (child) {
        char *txt;
        if(strcmp(child->name, "text")==0) {
            child = child->next;
            syslog(LOG_INFO, "ignoring");
            continue;
      }
 	txt = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
	if (busnumber == 0) {
          if(strncmp(child->name, "Server", 6)==0)
            readconfig_server(doc, child, busnumber);
	}
	if (busnumber < MAX_BUSSES) {
          /* some attributes are common for all (real) busses */
	    if (strcmp(child->name, "device") == 0) {
		free(busses[busnumber].device);
		busses[busnumber].device = malloc(strlen(txt) + 1);
		if (busses[busnumber].device == NULL) {
		    printf("cannot allocate memory\n");
		    exit(1);
		}
		strcpy(busses[busnumber].device, txt);
	    }
	    if (strcmp(child->name, "verbosity") == 0) {
		busses[busnumber].debuglevel = atoi(txt);
	    }
	    if (strcmp(child->name, "use_watchdog") == 0) {
		if (strcmp(txt, "yes") == 0)
		    busses[busnumber].flags |= USE_WATCHDOG;
	    }
	    if (strcmp(child->name, "restore_device_settings") == 0) {
		if (strcmp(txt, "yes") == 0)
		    busses[busnumber].deviceflags |= RESTORE_COM_SETTINGS;
	    }
         /* but the most important are not ;=)  */
	    if (strcmp(child->name, "M605X") == 0) {
		readconfig_m605x(doc, child, busnumber);
	    }
	    if (strcmp(child->name, "Intellibox") == 0) {
		readconfig_intellibox(doc, child, busnumber);
	    }
	    if (strcmp(child->name, "Loopback") == 0) {
		readconfig_loopback(doc, child, busnumber);
	    }
	    if (strcmp(child->name, "DDL-S88") == 0) {
		readconfig_DDL_S88(doc, child, busnumber);
	    }
	    if (strcmp(child->name, "HSI-S88") == 0) {
		readconfig_HSI_S88(doc, child, busnumber);
	    }
	} else {
	    // to many busses, needs recompilation
	}
	free(txt);
	child = child->next;
    }
}

static void walk_config_xml(xmlDocPtr doc)
{
    xmlNodePtr root, child;

    root = xmlDocGetRootElement(doc);
    child = root->children;
    num_busses = 0;

    while (child) {
	register_bus(doc, child);
	child = child->next;
    }
}

static xmlDocPtr load_config_xml(const char *filename)
{
    return xmlParseFile(filename);
}

void readConfig(const char *filename)
{
    xmlDocPtr doc;
    memset(busses, 0, sizeof(busses));

    /* some defaults */
    doc = load_config_xml(filename);
    if (doc != 0) {
      syslog(LOG_INFO, "parsing %s", filename);
	walk_config_xml(doc);
       syslog(LOG_INFO, " %s done", filename);
	xmlFreeDoc(doc);
    }
}
