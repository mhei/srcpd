/*
* This software is published under the terms of the GNU General Public
* License, Version 2, 1991. (c) Matthias Trute, 2000-2001.
*
* 04.07.2001 Frank Schmischke
*            Introducing configuration file
* 05.08.2001 Matthias Trute
*            changed to XML format
* 16.05.2005 Gerard van der Sel
*            addition of Selectrix
*/


#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xmlmemory.h>
#include <netdb.h>
#include <stdarg.h>                                 
#include <syslog.h>


#include "stdincludes.h"
#include "config-srcpd.h"
#include "srcp-fb.h"
#include "srcp-server.h"
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
#include "loconet.h"


/* SRCP server welcome message */
const char *WELCOME_MSG =
    "srcpd V" VERSION "; SRCP 0.8.3; SRCPOTHER 0.8.4-wip\n";


/* check if a bus has a device group or not */
int bus_has_devicegroup(bus_t bus, int dg)
{
    switch (dg) {
        case DG_SESSION:
            return strstr(buses[bus].description, "SESSION") != NULL;
        case DG_POWER:
            return strstr(buses[bus].description, "POWER") != NULL;
        case DG_GA:
            return strstr(buses[bus].description, "GA") != NULL;
        case DG_GL:
            return strstr(buses[bus].description, "GL") != NULL;
        case DG_GM:
            return strstr(buses[bus].description, "GM") != NULL;
        case DG_FB:
            return strstr(buses[bus].description, "FB") != NULL;
        case DG_SM:
            return strstr(buses[bus].description, "SM") != NULL;
        case DG_SERVER:
            return strstr(buses[bus].description, "SERVER") != NULL;
        case DG_TIME:
            return strstr(buses[bus].description, "TIME") != NULL;
        case DG_LOCK:
            return strstr(buses[bus].description, "LOCK") != NULL;
        case DG_DESCRIPTION:
            return strstr(buses[bus].description, "DESCRIPTION") != NULL;

    }
    return 0;
}

static bus_t register_bus(bus_t busnumber, xmlDocPtr doc, xmlNodePtr node)
{
    bus_t current_bus = busnumber;

    if (xmlStrcmp(node->name, BAD_CAST "bus"))
        return busnumber;

    if (busnumber >= MAX_BUSES) {
            DBG(0, DBG_ERROR,
               "Sorry, you have used an invalid bus number (%ld). "
               "If this is greater than or equal to %d,\n"
               "you need to recompile the sources.\n",
               busnumber, MAX_BUSES);
        return busnumber;
    }

    num_buses = busnumber;

    /* some default values */
    buses[current_bus].debuglevel = DBG_INFO;
    buses[current_bus].flags = 0;

    /* Function pointers to NULL */
    buses[current_bus].thr_func = NULL;
    buses[current_bus].thr_timer = NULL;
    buses[current_bus].sigio_reader = NULL;
    buses[current_bus].init_func = NULL;
    buses[current_bus].term_func = NULL;
    buses[current_bus].init_gl_func = NULL;
    buses[current_bus].init_ga_func = NULL;
    buses[current_bus].init_fb_func = NULL;

    /* Communication port to default values */
    buses[current_bus].devicetype = HW_FILENAME;

    /* FIXME: this will lead to a memory leak if initialization of
     * (busnumber - 1) failed */
    buses[current_bus].device.file.path = malloc(strlen("/dev/null") + 1);
    if (buses[current_bus].device.file.path == NULL)
        return current_bus;

    strcpy(buses[current_bus].device.file.path, "/dev/null");

    /* Definition of thread synchronisation  */
    pthread_mutex_init(&buses[current_bus].transmit_mutex, NULL);
    pthread_cond_init(&buses[current_bus].transmit_cond, NULL);

    xmlNodePtr child = node->children;
    xmlChar *txt = NULL;
    xmlChar *txt2 = NULL;

    while (child != NULL) {

        if ((xmlStrcmp(child->name, BAD_CAST "text") == 0) ||
            (xmlStrcmp(child->name, BAD_CAST "comment") == 0)) {
            /* just do nothing, it is only formatting text or a comment */
        }

        else if (xmlStrncmp(child->name, BAD_CAST "server", 6) == 0) {
            if (busnumber == 0)
                busnumber += readconfig_server(doc, child, busnumber);
            else
                DBG(0, DBG_ERROR, "Sorry, type=server is not allowed "
                                "at bus %ld!\n", busnumber);
        }

        /* but the most important are not ;=)  */
        else if (xmlStrcmp(child->name, BAD_CAST "zimo") == 0) {
            busnumber += readconfig_ZIMO(doc, child, busnumber);
        }

        else if (xmlStrcmp(child->name, BAD_CAST "ddl") == 0) {
            busnumber += readconfig_DDL(doc, child, busnumber);
        }

        else if (xmlStrcmp(child->name, BAD_CAST "m605x") == 0) {
            busnumber += readconfig_m605x(doc, child, busnumber);
        }

        else if (xmlStrcmp(child->name, BAD_CAST "intellibox") == 0) {
            busnumber += readConfig_IB(doc, child, busnumber);
        }

        else if (xmlStrcmp(child->name, BAD_CAST "loconet") == 0) {
            busnumber += readConfig_LOCONET(doc, child, busnumber);
        }

        else if (xmlStrcmp(child->name, BAD_CAST "loopback") == 0) {
            busnumber += readconfig_LOOPBACK(doc, child, busnumber);
        }

        else if (xmlStrcmp(child->name, BAD_CAST "ddl-s88") == 0) {
#if defined(linux) || defined(__CYGWIN__) || defined(__FreeBSD__)
            busnumber += readconfig_DDL_S88(doc, child, busnumber);
#else
            DBG(0, DBG_ERROR, "Sorry, DDL-S88 not (yet) available on "
                            "this system.\n");
#endif
        }

        else if (xmlStrcmp(child->name, BAD_CAST "hsi-88") == 0) {
            busnumber += readConfig_HSI_88(doc, child, busnumber);
        }

        else if (xmlStrcmp(child->name, BAD_CAST "li100usb") == 0) {
            busnumber += readConfig_LI100_USB(doc, child, busnumber);
        }

        else if (xmlStrcmp(child->name, BAD_CAST "li100") == 0) {
            busnumber += readConfig_LI100_SERIAL(doc, child, busnumber);
        }

        else if (xmlStrcmp(child->name, BAD_CAST "selectrix") == 0) {
            busnumber += readconfig_Selectrix(doc, child, busnumber);
        }

        else if (xmlStrcmp(child->name, BAD_CAST "i2c-dev") == 0) {
#ifdef linux
            busnumber += readconfig_I2C_DEV(doc, child, busnumber);
#else
            DBG(0, DBG_ERROR, "Sorry, I2C-DEV is only available on "
                            "Linux (yet).\n");
#endif
        }

        /* some attributes are common for all (real) buses */
        else if (xmlStrcmp(child->name, BAD_CAST "device") == 0) {
            txt2 = xmlGetProp(child, BAD_CAST "type");
            if (txt2 == NULL || xmlStrcmp(txt2, BAD_CAST "filename") == 0) {
                buses[current_bus].devicetype = HW_FILENAME;
            }
            else if (xmlStrcmp(txt2, BAD_CAST "network") == 0) {
                buses[current_bus].devicetype = HW_NETWORK;
            }
            else {
                DBG(0, DBG_ERROR, "WARNING, \"%s\" (bus %ld) is an "
                     "unknown device specifier!\n", child->name, current_bus);
            }
            free(txt2);
            txt = xmlNodeListGetString(doc, child->children, 1);
            if (txt != NULL) {
                switch (buses[current_bus].devicetype) {
                    case HW_FILENAME:
                        free(buses[current_bus].device.file.path);
                        buses[current_bus].device.file.path =
                            malloc(strlen((char *) txt) + 1);
                        strcpy(buses[current_bus].device.file.path,
                               (char *) txt);
                        break;
                    case HW_NETWORK:
                        free(buses[current_bus].device.file.path);
                        buses[current_bus].device.net.hostname =
                            malloc(strlen((char *) txt) + 1);
                        strcpy(buses[current_bus].device.net.hostname,
                               (char *) txt);
                        txt2 = xmlGetProp(child, BAD_CAST "port");
                        if (txt2 != NULL) {
                            buses[current_bus].device.net.port =
                                atoi((char *) txt2);
                            free(txt2);
                        }
                        else {
                            buses[current_bus].device.net.port = 0;
                        }
                        txt2 = xmlGetProp(child, BAD_CAST "protocol");
                        if (txt2 != NULL) {
                            struct protoent *p;
                            p = getprotobyname((char *) txt2);
                            buses[current_bus].device.net.protocol = p->p_proto;
                            free(txt2);
                        }
                        else {
                            buses[current_bus].device.net.protocol = 6; /* TCP */
                        }
                        break;
                }
                xmlFree(txt);
            }
            switch (buses[current_bus].devicetype) {
                case HW_FILENAME:
                    DBG(current_bus, DBG_DEBUG, "** Filename='%s'",
                        buses[current_bus].device.file.path);
                    break;
                case HW_NETWORK:
                    DBG(current_bus, DBG_DEBUG,
                        "** Network Host='%s', Protocol=%d Port=%d",
                        buses[current_bus].device.net.hostname,
                        buses[current_bus].device.net.protocol,
                        buses[current_bus].device.net.port);
                    break;
            }
        }

        else if (xmlStrcmp(child->name, BAD_CAST "verbosity") == 0) {
            txt = xmlNodeListGetString(doc, child->children, 1);
            if (txt != NULL) {
                buses[current_bus].debuglevel = atoi((char *) txt);
                xmlFree(txt);
            }
        }

        else if (xmlStrcmp(child->name, BAD_CAST "use_watchdog") == 0) {
            txt = xmlNodeListGetString(doc, child->children, 1);
            if (txt != NULL) {
                if (xmlStrcmp(txt, BAD_CAST "yes") == 0)
                    buses[current_bus].flags |= USE_WATCHDOG;
                xmlFree(txt);
            }
        }

        else if (xmlStrcmp(child->name, BAD_CAST "restore_device_settings")
                 == 0) {
            txt = xmlNodeListGetString(doc, child->children, 1);
            if (txt != NULL) {
                if (xmlStrcmp(txt, BAD_CAST "yes") == 0)
                    buses[current_bus].flags |= RESTORE_COM_SETTINGS;
                xmlFree(txt);
            }
        }

        else if (xmlStrcmp(child->name, BAD_CAST "auto_power_on") == 0) {
            txt = xmlNodeListGetString(doc, child->children, 1);
            if (txt != NULL) {
                if (xmlStrcmp(txt, BAD_CAST "yes") == 0)
                    buses[current_bus].flags |= AUTO_POWER_ON;
                xmlFree(txt);
            }
        }

        else if (xmlStrcmp(child->name, BAD_CAST "speed") == 0) {
            txt = xmlNodeListGetString(doc, child->children, 1);
            if (txt != NULL) {
                int speed = atoi((char *) txt);

                switch (speed) {
                    case 2400:
                        buses[current_bus].device.file.baudrate = B2400;
                        break;
                    case 4800:
                        buses[current_bus].device.file.baudrate = B4800;
                        break;
                    case 9600:
                        buses[current_bus].device.file.baudrate = B9600;
                        break;
                    case 19200:
                        buses[current_bus].device.file.baudrate = B19200;
                        break;
                    case 38400:
                        buses[current_bus].device.file.baudrate = B38400;
                        break;
                    case 57600:
                        buses[current_bus].device.file.baudrate = B57600;
                        break;
                    case 115200:
                        buses[current_bus].device.file.baudrate = B115200;
                        break;
                    default:
                        buses[current_bus].device.file.baudrate = B2400;
                        break;
                }
                xmlFree(txt);
            }
        }

        else
            DBG(0, DBG_ERROR,"WARNING, \"%s\" (bus %ld) is an unknown tag!\n",
                   child->name, current_bus);

        child = child->next;
    }

    return busnumber;
}

/*walk through xml tree and return number of found buses*/
static bus_t walk_config_xml(xmlDocPtr doc)
{
    bus_t bus = 0;
    xmlNodePtr root, child;

    root = xmlDocGetRootElement(doc);
    if (root == NULL) {
        DBG(0, DBG_ERROR, "Error, no XML document root found.\n");
        return bus;
    }
    child = root->children;

    while (child != NULL) {
        bus = register_bus(bus, doc, child);
        child = child->next;
    }
    return bus;
}

/*read configuration file and return success value if some bus was found*/
int readConfig(char *filename)
{
    xmlDocPtr doc;
    bus_t rb = 0;

    /* something to initialize */
    memset(buses, 0, sizeof(buses));
    num_buses = 0;

    /* some defaults */
    DBG(0, DBG_DEBUG, "parsing %s", filename);
    doc = xmlParseFile(filename);

    /* always show a message */
    if (doc != NULL) {
        DBG(0, DBG_DEBUG, "walking %s", filename);
        rb = walk_config_xml(doc);
        DBG(0, DBG_DEBUG, " done %s; found %ld buses", filename, rb);
        xmlFreeDoc(doc);
        /*
         *Free the global variables that may
         *have been allocated by the parser.
         */
        xmlCleanupParser();
    }
    else {
        DBG(0, DBG_ERROR,
            "Error, no XML document tree found parsing %s.\n", filename);
    }
    return (rb > 0);
}

/**
 * suspendThread: Holds the thread until a resume command is given.
        The thread waits in this routines
 * @param busnumber
       bus_t given the bus which thread has to be stopped.
 */
void suspendThread(bus_t busnumber)
{
    DBG(0, DBG_DEBUG, "Thread on bus %d is going to stop.", busnumber);
    /* Lock thread till new data to process arrives */
    pthread_mutex_lock(&buses[busnumber].transmit_mutex);
    pthread_cond_wait(&buses[busnumber].transmit_cond,
            &buses[busnumber].transmit_mutex);
     /* mutex released.       */
    pthread_mutex_unlock(&buses[busnumber].transmit_mutex);
    DBG(0, DBG_DEBUG, "Thread on bus %d is working again.", busnumber);
}

/**
 * resumeThread: continue a stopped thread
 * @param busnumber
       bus_t given the bus which thread has to be stopped.
 */
void resumeThread(bus_t busnumber)
{
    /* Let thread process a feedback */
    pthread_mutex_lock(&buses[busnumber].transmit_mutex);
    pthread_cond_signal(&buses[busnumber].transmit_cond);
    pthread_mutex_unlock(&buses[busnumber].transmit_mutex);
    DBG(0, DBG_DEBUG, "Thread on bus %d is woken up", busnumber);
}

/**
  * DBG: write some syslog information is current debug level of the
         bus is greater then the the debug level of the message. e.g.
         if a debug message is deb_info and the bus is configured
         to inform only about deb_error, no message will be generated.
  * @param busnumber
    integer, busnumber
  * @param dbglevel
    one of the constants DBG_FATAL, DBG_ERROR, DBG_WARN, DBG_INFO, DBG_DEBUG
  * @param fmt
    const char *: standard c format string
  * @param ...
    remaining parameters according to format string
  */
void DBG(bus_t busnumber, int dbglevel, const char *fmt, ...)
{
    if (dbglevel <= buses[busnumber].debuglevel) {
        va_list parm;
        va_start(parm, fmt);
        char *msg;
        msg = (char *) malloc(sizeof(char) * (strlen(fmt) + 10));
        if (msg == NULL)
            return;
        sprintf(msg, "[bus %ld] %s", busnumber, fmt);
        vsyslog(LOG_INFO, msg, parm);
        free(msg);

        va_end(parm);
    }
}
