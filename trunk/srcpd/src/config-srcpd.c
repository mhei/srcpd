/* cvs: $Id$             */

/*
 * Vorliegende Software unterliegt der General Public License,
 * Version 2, 1991. (c) Matthias Trute, 2000-2001.
 *
 * 04.07.2001 Frank Schmischke
 *            Einführung der Konfigurationsdatei
 * 05.08.2001 Matthias Trute
 *	      Umstellung auf XML Format
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
#include "m605x.h"
#include "ib.h"
#include "loopback.h"

/* Einstellungen für Bus 0 */
int TCPPORT = 12345;		/* default command port         */

int TIMER_RUNNING = 1;          /* Running timer module         */

/* Willkommensmeldung */
const char *WELCOME_MSG = "srcpd V2; SRCP 0.8.0; do not use\n";
char PIDFILE[MAXPATHLEN];

struct _BUS busses[MAX_BUSSES];
int num_busses;

static void
register_bus (xmlDocPtr doc, xmlNodePtr node)
{
  xmlNodePtr child;
  char *proptxt;
  int busnumber;
  if (strcmp (node->name, "bus"))
      return; // only bus definitions
  proptxt = xmlGetProp (node, "number");
  busnumber = atoi (proptxt);
  free (proptxt);
  if (busnumber > MAX_BUSSES){
	  // you need to recompile
	  return;
  }
  busses[busnumber].number = busnumber;
  child = node->children;
  while (child)
    {
      char *txt;
      txt = xmlNodeListGetString (doc, child->xmlChildrenNode, 1);

      if (busnumber == 0)
	{
	  if (strcmp (child->name, "baseport") == 0)
	    {
	      TCPPORT = atoi (txt);
	    }

	}
      if(busnumber < MAX_BUSSES) {
	  if (strcmp (child->name, "device") == 0)
	    {
	      free (busses[busnumber].device);
	      busses[busnumber].device = malloc (strlen (txt) + 1);
	      if (busses[busnumber].device == NULL)
		{
		  printf ("cannot allocate memory\n");
		  exit (1);
		}
	      strcpy (busses[busnumber].device, txt);
	    }
	  if (strcmp (child->name, "FB-modules") == 0)
	    {
	      busses[busnumber].number_fb = atoi (txt);
	    }
	  if (strcmp (child->name, "maximum_adress_for_locomotiv") == 0)
	    {
	      busses[busnumber].number_gl = atoi (txt);
	    }
	  if (strcmp (child->name, "maximum_adress_for_accessoires") == 0)
	    {
	       busses[busnumber].number_ga = atoi (txt);
	    }
	  if (strcmp (child->name, "use_watchdog") == 0)
	    {
	      if (strcmp (txt, "yes") == 0)
		 busses[busnumber].flags |= USE_WATCHDOG;
	    }
	  if (strcmp (child->name, "type") == 0)
	    {
		if (strcmp (txt, "M605X") == 0) {
		    busses[busnumber].type = SERVER_M605X;
		    busses[busnumber].init_func = &init_bus_M6051;
		    busses[busnumber].term_func = &term_bus_M6051;
		    busses[busnumber].thr_func = &thr_sendrec_M6051;
		}
		if (strcmp (txt, "IB") == 0) {
		    busses[busnumber].type = SERVER_IB;
		    busses[busnumber].init_func = &init_bus_IB;
		    busses[busnumber].term_func = &term_bus_IB;
		    busses[busnumber].thr_func = &thr_sendrec_IB;
		}
		if (strcmp(txt, "LOOPBACK") == 0) {
		    busses[busnumber].type = SERVER_LOOPBACK;
		    busses[busnumber].init_func = &init_bus_Loopback;
		    busses[busnumber].term_func = &term_bus_Loopback;
		    busses[busnumber].thr_func = &thr_sendrec_Loopback;
		}

	    }
	  if (strcmp (child->name, "use_M6020") == 0)
	    {
	      if (strcmp (txt, "yes") == 0)
		 busses[busnumber].flags |= M6020_MODE;
	    }
	  if (strcmp (child->name, "restore_com_parms") == 0)
	    {
	      if (strcmp (txt, "yes") == 0)
		 busses[busnumber].deviceflags |= RESTORE_COM_SETTINGS;
	    }
	  if (strcmp (child->name, "debuglevel") == 0)
	    {
	      busses[busnumber].debuglevel = atoi(txt);
	    }
	} else {
	    // to many busses, needs recompilation
	}
      free (txt);
      child = child->next;
    }
    num_busses = busnumber;
}

static void
walk_config_xml (xmlDocPtr doc)
{
  xmlNodePtr root, child;
  root = xmlDocGetRootElement(doc);
  child = root->children;
  while (child)
    {
      register_bus (doc, child);
      child = child->next;
    }
}

static xmlDocPtr
load_config_xml (const char *filename)
{
  return xmlParseFile (filename);
}

void
readConfig ()
{
  xmlDocPtr doc;
  memset(busses, 0, sizeof(busses));

  /* some defaults */
  strcpy(PIDFILE, "/var/run/srcpd.pid");

  doc = load_config_xml ("/usr/local/etc/srcpd.xml");
  if (doc == 0)
    doc = load_config_xml ("/usr/etc/srcpd.xml");
  if (doc == 0)
    doc = load_config_xml ("/etc/srcpd.xml");
  if (doc == 0)
    doc = load_config_xml ("srcpd.xml");
  if (doc != 0)
    {
      walk_config_xml (doc);
      xmlFreeDoc(doc);
    }
}
