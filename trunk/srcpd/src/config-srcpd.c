/* cvs: $Id$             */

/*
 * Vorliegende Software unterliegt der General Public License,
 * Version 2, 1991. (c) Matthias Trute, 2000-2001.
 *
 * 04.07.2001 Frank Schmischke
 *            Einf�hrung der Konfigurationsdatei
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

/* Einstellungen f�r Bus 0 */
int TCPPORT = 12345;		/* default command port         */
char *username;
char *groupname;

/* Willkommensmeldung */
const char *WELCOME_MSG = "srcpd V2; SRCP 0.8.x; do not use\n";
char PIDFILE[MAXPATHLEN];

struct _BUS busses[MAX_BUSSES];
int num_busses;

/* Busspezifische Funktionen */

static int register_intellibox(xmlDocPtr doc, xmlNodePtr node,
			       int busnumber)
{

    busses[busnumber].type = SERVER_IB;
    busses[busnumber].init_func = &init_bus_IB;
    busses[busnumber].term_func = &term_bus_IB;
    busses[busnumber].thr_func = &thr_sendrec_IB;
    busses[busnumber].driverdata = malloc(sizeof(struct _IB_DATA));
    strcpy(busses[busnumber].description, "GA GL FB POWER");
}

static int register_loopback(xmlDocPtr doc, xmlNodePtr node, int busnumber)
{
    busses[busnumber].type = SERVER_LOOPBACK;
    busses[busnumber].init_func = &init_bus_Loopback;
    busses[busnumber].term_func = &term_bus_Loopback;
    busses[busnumber].thr_func = &thr_sendrec_Loopback;
    busses[busnumber].driverdata = malloc(sizeof(struct _LOOPBACK_DATA));
    strcpy(busses[busnumber].description, "GA GL FB POWER");
}
static int register_DDL_S88(xmlDocPtr doc, xmlNodePtr node, int busnumber)
{
    busses[busnumber].type = SERVER_S88;
    busses[busnumber].init_func = &init_bus_S88;
    busses[busnumber].term_func = &term_bus_S88;
    busses[busnumber].thr_func = &thr_sendrec_S88;
    busses[busnumber].driverdata = malloc(sizeof(struct _DDL_S88_DATA));
    ((DDL_S88_DATA *) busses[busnumber].driverdata)->port = 0x0378;
    ((DDL_S88_DATA *) busses[busnumber].driverdata)->clockscale = 35;
    ((DDL_S88_DATA *) busses[busnumber].driverdata)->refresh = 100;
    strcpy(busses[busnumber].description, "FB POWER");
}
static int register_HSI_S88(xmlDocPtr doc, xmlNodePtr node, int busnumber)
{
    busses[busnumber].type = SERVER_HSI_88;
    busses[busnumber].init_func = &init_bus_HSI_88;
    busses[busnumber].term_func = &term_bus_HSI_88;
    busses[busnumber].thr_func = &thr_sendrec_HSI_88;
    busses[busnumber].driverdata = malloc(sizeof(struct _HSI_S88_DATA));
    strcpy(busses[busnumber].description, "FB POWER");
}

static void register_bus(xmlDocPtr doc, xmlNodePtr node)
{
    xmlNodePtr child;
    char *proptxt;
    int busnumber;

    if (strcmp(node->name, "bus"))
	return;			// only bus definitions

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
	txt = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);

	if (busnumber == 0) {
	    if (strcmp(child->name, "TCPport") == 0) {
		TCPPORT = atoi(txt);
	    }
	    if (strcmp(child->name, "PIDfile") == 0) {
		strcpy(PIDFILE, txt);
	    }
	    if (strcmp(child->name, "username") == 0) {
		free(username);
		username = malloc(strlen(txt) + 1);
		if (username == NULL) {
		    printf("cannot allocate memory\n");
		    exit(1);
		}
		strcpy(username, txt);
	    }
	    if (strcmp(child->name, "groupname") == 0) {
		free(groupname);
		groupname = malloc(strlen(txt) + 1);
		if (username == NULL) {
		    printf("cannot allocate memory\n");
		    exit(1);
		}
		strcpy(groupname, txt);
	    }
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
		readconfig_m605x(doc, child->children, busnumber);
	    }
	    if (strcmp(child->name, "Intellibox") == 0) {
		register_intellibox(doc, child->children, busnumber);
	    }
	    if (strcmp(child->name, "Loopback") == 0) {
		register_loopback(doc, child->children, busnumber);
	    }
	    if (strcmp(child->name, "DDL-S88") == 0) {
		register_DDL_S88(doc, child->children, busnumber);
	    }
	    if (strcmp(child->name, "HSI-S88") == 0) {
		register_HSI_S88(doc, child->children, busnumber);
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
    busses[0].type = SERVER_SERVER;
    busses[0].init_func = &init_bus_server;
    busses[0].term_func = &term_bus_server;
    strcpy(busses[0].description, "SESSION SERVER TIME");

    /* some defaults */
    strcpy(PIDFILE, "/var/run/srcpd.pid");
    doc = load_config_xml(filename);
    if (doc != 0) {
	walk_config_xml(doc);
	xmlFreeDoc(doc);
    }
}
