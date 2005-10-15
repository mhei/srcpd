
/*
* Vorliegende Software unterliegt der General Public License,
* Version 2, 1991. (c) Matthias Trute, 2000-2001.
*
* 04.07.2001 Frank Schmischke
*            Einfhrung der Konfigurationsdatei
* 05.08.2001 Matthias Trute
*            Umstellung auf XML Format
* 16.05.2005 Gerard van der Sel
*            addition of Selectrix 
*/

#include "stdincludes.h"
#include "config-srcpd.h"
#include "srcp-fb.h"
#include "srcp-srv.h"
#include "ddl.h"
#include "m605x.h"
#include "selectrix.h"
#include "ib.h"
#include "loopback.h"
#include "ddl-s88.h"
#include "hsi-88.h"
#include "i2c-dev.h"
#include "zimo.h"
#include "li100.h"

/* Willkommensmeldung */
const char *WELCOME_MSG = "srcpd V" VERSION "; SRCP 0.8.2\n";

struct _BUS busses[MAX_BUSSES];
int num_busses;

// check that a bus has a devicegroup or not

int bus_has_devicegroup(int bus, int dg) {
  switch (dg) {
    case DG_SESSION:
      return strstr(busses[bus].description, "SESSION") != NULL;
    case DG_POWER:
      return strstr(busses[bus].description, "POWER") != NULL;
    case DG_GA:
      return strstr(busses[bus].description, "GA") != NULL;
    case DG_GL:
      return strstr(busses[bus].description, "GL") != NULL;
    case DG_FB:
      return strstr(busses[bus].description, "FB") != NULL;
    case DG_SM:
      return strstr(busses[bus].description, "SM") != NULL;
    case DG_SERVER:
      return strstr(busses[bus].description, "SERVER") != NULL;
    case DG_TIME:
      return strstr(busses[bus].description, "TIME") != NULL;
    case DG_LOCK:
      return strstr(busses[bus].description, "LOCK") != NULL;
    case DG_DESCRIPTION:
      return strstr(busses[bus].description, "DESCRIPTION") != NULL;

  }
  return 0;
}
static int register_bus(int busnumber, xmlDocPtr doc, xmlNodePtr node)
{
 xmlNodePtr child;
 int found;
 int current_bus = busnumber;

 if (strcmp(node->name, "bus"))
   return busnumber;

 if (busnumber >= MAX_BUSSES || busnumber < 0)
 {
   printf(_("Sorry, you have used an invalid busnumber (%d). "
            "If this is greater than or equal to %d,\n"
            "you need to recompile the sources. Exiting now.\n"),
            busnumber, MAX_BUSSES);
   return busnumber;
 }

 num_busses = busnumber;
 child = node->children;
 /* some default values */
 busses[current_bus].debuglevel = 1;
 free(busses[current_bus].device);
 busses[current_bus].device = malloc(strlen("/dev/null") + 1);
 strcpy(busses[current_bus].device, "/dev/null");
 busses[current_bus].flags = 0;
 busses[current_bus].baudrate = B2400;

 while (child)
 {
   char *txt;
   if ( (strcmp(child->name, "text") == 0) || (strcmp(child->name, "comment") == 0))
   {
     child = child->next;
     continue;
   }
   txt = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
   found = 0;
   if (strncmp(child->name, "server", 6) == 0)
   {
     if (busnumber == 0)
     {
       busnumber += readconfig_server(doc, child, busnumber);
       found = 1;
     }
     else
     {
       printf(_("Sorry, type=server is only allowed at bus 0!\n"));
     }
   }
   /* but the most important are not ;=)  */
   if (strcmp(child->name, "zimo") == 0)
   {
     busnumber += readconfig_zimo(doc, child, busnumber);
     found = 1;
   }
   if (strcmp(child->name, "ddl") == 0)
   {
     busnumber += readconfig_DDL(doc, child, busnumber);
     found = 1;
   }

   if (strcmp(child->name, "m605x") == 0)
   {
     busnumber += readconfig_m605x(doc, child, busnumber);
     found = 1;
   }
   if (strcmp(child->name, "intellibox") == 0)
   {
     busnumber += readConfig_IB(doc, child, busnumber);
     found = 1;
   }
   if (strcmp(child->name, "loopback") == 0)
   {
     busnumber += readconfig_loopback(doc, child, busnumber);
     found = 1;
   }
   if (strcmp(child->name, "ddl-s88") == 0)
   {
     busnumber += readconfig_DDL_S88(doc, child, busnumber);
     found = 1;
   }
   if (strcmp(child->name, "hsi-88") == 0)
   {
     busnumber += readConfig_HSI_88(doc, child, busnumber);
     found = 1;
   }
   if (strcmp(child->name, "li100") == 0)
   {
     busnumber += readConfig_LI100(doc, child, busnumber);
     found = 1;
   }
   if (strcmp(child->name, "selectrix") == 0)
   {
     busnumber += readconfig_Selectrix(doc, child, busnumber);
     found = 1;
   }
   if (strcmp(child->name, "i2c-dev") == 0)
   {
#ifdef linux
     busnumber += readconfig_I2C_DEV(doc, child, busnumber);
     found = 1;
#else
  printf(_("Sorry, I2C-DEV only available on Linux (yet)\n"));
#endif
   }
   /* some attributes are common for all (real) busses */
   if (strcmp(child->name, "device") == 0)
   {
     found = 1;
     free(busses[current_bus].device);
     busses[current_bus].device = malloc(strlen(txt) + 1);
     if (busses[current_bus].device == NULL)
     {
       printf(_("cannot allocate memory\n"));
       exit(1);
     }
     strcpy(busses[current_bus].device, txt);
   }
   if (strcmp(child->name, "verbosity") == 0)
   {
     found = 1;
     busses[current_bus].debuglevel = atoi(txt);
   }
   if (strcmp(child->name, "use_watchdog") == 0)
   {
     found = 1;
     if (strcmp(txt, "yes") == 0)
       busses[current_bus].flags |= USE_WATCHDOG;
   }
   if (strcmp(child->name, "restore_device_settings") == 0)
   {
     found = 1;
     if (strcmp(txt, "yes") == 0)
       busses[current_bus].flags |= RESTORE_COM_SETTINGS;
   }
   if (strcmp(child->name, "auto_power_on") == 0)
   {
     found = 1;
     if (strcmp(txt, "yes") == 0)
          busses[current_bus].flags |= AUTO_POWER_ON ;
   }
   if (strcmp(child->name, "speed") == 0)
   {
     found = 1;
     switch(atoi(txt))
     {
       case 2400:
         busses[current_bus].baudrate = B2400;
         break;
       case 4800:
         busses[current_bus].baudrate = B4800;
         break;
       case 9600:
         busses[current_bus].baudrate = B9600;
         break;
       case 19200:
         busses[current_bus].baudrate = B19200;
         break;
       case 38400:
         busses[current_bus].baudrate = B38400;
         break;
       default:
         busses[current_bus].baudrate = B2400;
         break;
     }
   }
   if (!found)
   {
     printf(_("WARNING, \"%s\" (bus %d) is an unknown tag!\n"), child->name, current_bus);
   }
   free(txt);
   child = child->next;
 }
 return busnumber;
}

static int walk_config_xml(xmlDocPtr doc)
{
 int bus = 0;
 xmlNodePtr root, child;

 root = xmlDocGetRootElement(doc);
 child = root->children;

 while (child)
 {
   bus = register_bus(bus, doc, child);
   child = child->next;
 }
 return bus;
}

int readConfig(char *filename)
{
 xmlDocPtr doc;
 int i, rc;

 // something to initialize
 memset(busses, 0, sizeof(busses));
 for(i=0;i<MAX_BUSSES;i++)
   busses[i].number = -1;
 num_busses = -1;

 /* some defaults */
 DBG(0, 0, "parsing %s", filename);
 doc = xmlParseFile(filename);
 if (doc != 0)
 { /* always put a message */
   DBG(0, 0, "walking %s", filename);
   rc = walk_config_xml(doc);
   DBG(0, 0, " done %s; found %d busses", filename, rc);
   xmlFreeDoc(doc);
 }
 else
 {
   exit(1);
 }
 return rc;
}

/**
  * DBG: write some syslog information is current debuglevel of the
         bus is greater then the the debug level of the message. e.g.
         if a debug message is deb_info and the bus is configured
         to inform only about deb_error, no message will be generated.
  * @param busnumber
    integer, busnumber
  * @param dbglevel
    one of the constants DBG_FATAL, DBG_ERROR, DBG_WARN, DBG_INFO, DBG_DEBUG
  * @param fmt
    const char *: standard c formatstring
  * @param ...
    remaining parameters according to formatstring
  */
void DBG(int busnumber, int dbglevel, const char *fmt, ...)
{
  va_list parm;
  va_start(parm, fmt);
  if (dbglevel <= busses[busnumber].debuglevel) {
    char *msg;
    msg = (char *)malloc (sizeof(char) * (strlen(fmt) + 10));
    if (msg == NULL) return; // MAM: Wat solls? Ist eh am Ende
    sprintf (msg, "[bus %d] %s", busnumber, fmt);
    vsyslog(LOG_INFO, msg, parm);
    free (msg);
    if (busses[busnumber].debuglevel>DBG_WARN) {
	fprintf(stderr,"[bus %d] ",busnumber);
	vfprintf(stderr,fmt,parm);
	if (strchr(fmt,'\n')==NULL) 
	    fprintf(stderr,"\n");
        }
    }
  va_end(parm);
}
