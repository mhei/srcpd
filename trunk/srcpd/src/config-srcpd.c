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


#include <gnome-xml/xmlmemory.h>
#include <gnome-xml/parser.h>
#include <gnome-xml/tree.h>

#include "config-srcpd.h"

/* Folgende Werte lassen sich z.T. verändern, siehe Kommandozeilen/Configdatei */
/* unveränderliche sind mit const markiert                                     */

int debuglevel = 1;		/* stellt die Geschwätzigkeit          */

#ifdef TESTMODE
int testmode = 0;
#endif

/* Anschlußport */
/* globale Informationen für den Netzwerkanschluß */
int CMDPORT = 12345;		/* default command port         */
int FEEDBACKPORT = 12346;	/* default feedback port        */
int INFOPORT = 12347;		/* default info port            */

/* Willkommensmeldung */
const char *WELCOME_MSG = "srcpd V1.1; SRCP 0.7.3; August 2001; WILLKOMMEN\n";

/* Anschlußport */
char *DEV_COMPORT;

int working_server = SERVER_M605X;	// Einstellung in welchem Modus srcpd arbeitet
int NUMBER_FB = 2;		/* Anzahl der Feebackmodule     */
int NUMBER_GA = 256;		// Anzahl der generischen Accessoires
int NUMBER_GL = 80;		// Anzahl an Lokadressen
int M6020MODE = 0;		/* die 6020 mag keine erweiterten Befehle */
int use_i8255 = 0;		// soll auch I8255 genutzt werden ?
int use_watchdog = 0;		// soll der interne Wachhund genutzt werden ?
int restore_com_parms = 0;	// sollen die com-port Einstellungen gesichert werden?

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
  if (busnumber > 1)
    {
      // not yet  
      return;
    }
  child = node->childs;
  while (child)
    {
      char *txt;
      txt = xmlNodeListGetString (doc, child->xmlChildrenNode, 1);

      if (busnumber == 0)
	{
	  if (strcmp (child->name, "baseport") == 0)
	    {
	      CMDPORT = atoi (txt);
	      FEEDBACKPORT = CMDPORT + 1;
	      INFOPORT = CMDPORT + 2;
	  printf ("\t\t0 => %s\n", txt);

	    }

	}
      if (busnumber == 1)
	{
	  if (strcmp (child->name, "device") == 0)
	    {
	      free (DEV_COMPORT);
	      DEV_COMPORT = malloc (strlen (txt) + 1);
	      if (DEV_COMPORT == NULL)
		{
		  printf ("cannot allocate memory\n");
		  exit (1);
		}
	      strcpy (DEV_COMPORT, txt);
	    }
	  if (strcmp (child->name, "FB-modules") == 0)
	    {
	      NUMBER_FB = atoi (txt);
	    }
	  if (strcmp (child->name, "maximum_adress_for_locomotiv") == 0)
	    {
	      NUMBER_GL = atoi (txt);
	    }
	  if (strcmp (child->name, "maximum_adress_for_accessoires") == 0)
	    {
	      NUMBER_GA = atoi (txt);
	    }
	  if (strcmp (child->name, "use_watchdog") == 0)
	    {
	      if (strcmp (txt, "yes") == 0)
		use_watchdog = 1;
	    }
	  if (strcmp (child->name, "server") == 0)
	    {
	      if (strcmp (txt, "M605X") == 0)
		working_server = SERVER_M605X;
	      if (strcmp (txt, "IB") == 0)
		working_server = SERVER_IB;
	    }
	  if (strcmp (child->name, "use_M6020") == 0)
	    {
	      if (strcmp (txt, "yes") == 0)
		M6020MODE = 1;
	    }
	  if (strcmp (child->name, "restore_com_parms") == 0)
	    {
	      if (strcmp (txt, "yes") == 0)
		restore_com_parms = 1;
	    }
	}
      free (txt);
      child = child->next;
    }
}

static void
walk_config_xml (xmlDocPtr doc)
{
  xmlNodePtr child;
  child = doc->root->childs;
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
    }
}
