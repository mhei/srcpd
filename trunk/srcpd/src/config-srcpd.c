
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

#include "stdincludes.h"
#include "config-srcpd.h"
#include "srcp-fb.h"
#include "srcp-srv.h"
#include "m605x.h"
#include "ib.h"
#include "loopback.h"
#include "ddl-s88.h"
#include "hsi-88.h"

/* Willkommensmeldung */
const char *WELCOME_MSG = "srcpd V2 BETA; SRCP 0.8\n";

struct _BUS busses[MAX_BUSSES];
int num_busses;


// check that a real server is running on bus greather then zero
static void check_bus(int busnumber)
{
 if (busnumber == 0)
 {
   printf("Sorry, at bus 0 is only type=server allowed!\n");
   exit(1);
 }
}

static int register_bus(xmlDocPtr doc, xmlNodePtr node)
{
 xmlNodePtr child;
 char *proptxt;
 int busnumber;
 int found;

 if (strcmp(node->name, "bus"))
   return 1; // only bus definitions

 proptxt = xmlGetProp(node, "number");
 busnumber = atoi(proptxt);
 free(proptxt);
 if (busnumber >= MAX_BUSSES || busnumber < 0)
 {
   // you need to recompile
   return 1;
 }

 if (busnumber <= num_busses)
 {
   printf("Sorry, there is an error in your srcpd.conf !\n");
   printf("bus %i is following bus %i, this is not allowed!",
     busnumber, num_busses);
   exit(1);
 }
 busses[busnumber].number = busnumber;
 busses[busnumber].numberOfSM = 0;          // default no SM avalible
 num_busses = busnumber;
 child = node->children;
 while (child)
 {
   char *txt;
   if (strcmp(child->name, "text") == 0)
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
       readconfig_server(doc, child, busnumber);
       found = 1;
     }
     else
     {
       printf("Sorry, type=server is only at bus 0 allowed!\n");
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
#ifdef linux
     readconfig_intellibox(doc, child, busnumber);
     found = 1;
#else
	printf("Sorry, Intellibox support only available on Linux (yet)\n");
#endif
   }
   if (strcmp(child->name, "loopback") == 0)
   {
     check_bus(busnumber);
     readconfig_loopback(doc, child, busnumber);
     found = 1;
   }
   if (strcmp(child->name, "ddl-s88") == 0)
   {
     check_bus(busnumber);
#ifdef linux
     readconfig_DDL_S88(doc, child, busnumber);
     found = 1;
#else
	printf("Sorry, DDL_S88 only available on Linux (yet)\n");
#endif
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
       busses[busnumber].flags |= RESTORE_COM_SETTINGS;
   }
   if (strcmp(child->name, "p_time") == 0)
   {
     found = 1;
     set_min_time(busnumber, atoi(txt));
   }
   if (strcmp(child->name, "speed") == 0)
   {
     found = 1;
     switch(atoi(txt))
     {
       case 2400:
         busses[busnumber].baudrate = B2400;
         break;
       case 19200:
         busses[busnumber].baudrate = B19200;
         break;
       case 38400:
         busses[busnumber].baudrate = B38400;
         break;
       default:
         busses[busnumber].baudrate = B2400;
         break;
     }
   }
   if (!found)
   {
     printf("WARNING, \"%s\" (bus %d) is an unknown tag!\n", child->name, busnumber);
   }
   free(txt);
   child = child->next;
 }
 return 0;
}

static int walk_config_xml(xmlDocPtr doc)
{
 int rc = 0;
 xmlNodePtr root, child;

 root = xmlDocGetRootElement(doc);
 child = root->children;

 while (child)
 {
   rc = register_bus(doc, child);
   child = child->next;
 }
 return rc;
}

int readConfig(const char *filename)
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
   DBG(0, 0, " done %s", filename);
   xmlFreeDoc(doc);
 }
 else
 {
   exit(1);
 }
 return rc;
}

void DBG(int busnumber, int dbglevel, const char *fmt, ...)
{
  va_list parm;
  va_start(parm, fmt);
   /* need some more checks, may segfault! */
   if (dbglevel <= busses[busnumber].debuglevel) {
	if (busses[busnumber].debuglevel>DBG_WARN)
	{
	    fprintf(stderr,"[bus %d] ",busnumber);
	    vfprintf(stderr,fmt,parm);
	    if (strchr(fmt,'\n'==NULL)) fprintf(stderr,"\n");
	}
	else
	{
            char *msg;
            msg = (char *)malloc (sizeof(char) * (strlen(fmt) + 10));
	    if (msg == NULL) return; // MAM: Wat solls? Ist eh am Ende
	    sprintf (msg, "[bus %d] %s", busnumber, fmt);
            vsyslog(LOG_INFO, msg, parm);
            free (msg);
	}
    }
}
