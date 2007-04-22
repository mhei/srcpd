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
#include "loconet.h"


/* SRCP server welcome message */
const char *WELCOME_MSG =
    "srcpd V" VERSION "; SRCP 0.8.3; SRCPOTHER 0.8.4-wip\n";

struct _BUS busses[MAX_BUSSES];
int num_busses;

// check that a bus has a device group or not

int bus_has_devicegroup(bus_t bus, int dg)
{
    switch (dg) {
        case DG_SESSION:
            return strstr(busses[bus].description, "SESSION") != NULL;
        case DG_POWER:
            return strstr(busses[bus].description, "POWER") != NULL;
        case DG_GA:
            return strstr(busses[bus].description, "GA") != NULL;
        case DG_GL:
            return strstr(busses[bus].description, "GL") != NULL;
        case DG_GM:
            return strstr(busses[bus].description, "GM") != NULL;
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

static bus_t register_bus(bus_t busnumber, xmlDocPtr doc, xmlNodePtr node)
{
    bus_t current_bus = busnumber;

    if (xmlStrcmp(node->name, BAD_CAST "bus"))
        return busnumber;

    if (busnumber >= MAX_BUSSES || busnumber < 0) {
        printf("Sorry, you have used an invalid bus number (%ld). "
               "If this is greater than or equal to %d,\n"
               "you need to recompile the sources. Exiting now.\n",
               busnumber, MAX_BUSSES);
        return busnumber;
    }

    num_busses = busnumber;

    /* some default values */
    busses[current_bus].debuglevel = DBG_INFO;
    busses[current_bus].flags = 0;

    /* Function pointers to NULL */
    busses[current_bus].thr_func = NULL;
    busses[current_bus].thr_timer = NULL;
    busses[current_bus].sig_reader = NULL;
    busses[current_bus].init_func = NULL;
    busses[current_bus].term_func = NULL;
    busses[current_bus].init_gl_func = NULL;
    busses[current_bus].init_ga_func = NULL;
    busses[current_bus].init_fb_func = NULL;

    /* Communication port to default values */
    busses[current_bus].devicetype = HW_FILENAME;
    /*TODO: what happens if malloc returns NULL? */
    busses[current_bus].filename.path = malloc(strlen("/dev/null") + 1);
    strcpy(busses[current_bus].filename.path, "/dev/null");
    busses[current_bus].fd = -1;
    busses[current_bus].baudrate = B2400;

    /* Definition of thread synchronisation  */
    pthread_mutex_init(&busses[current_bus].transmit_mutex, NULL);
    pthread_cond_init(&busses[current_bus].transmit_cond, NULL);

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
                printf("Sorry, type=server is only allowed at bus 0!\n");
        }

        /* but the most important are not ;=)  */
        else if (xmlStrcmp(child->name, BAD_CAST "zimo") == 0) {
            busnumber += readconfig_zimo(doc, child, busnumber);
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
            printf
                ("Sorry, DDL-S88 not (yet) available on this system.\n");
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
            printf("Sorry, I2C-DEV only available on Linux (yet)\n");
#endif
        }

        /* some attributes are common for all (real) buses */
        else if (xmlStrcmp(child->name, BAD_CAST "device") == 0) {
            txt2 = xmlGetProp(child, BAD_CAST "type");
            if (txt2 == NULL || xmlStrcmp(txt2, BAD_CAST "filename") == 0) {
                busses[current_bus].devicetype = HW_FILENAME;
            }
            else if (xmlStrcmp(txt2, BAD_CAST "network") == 0) {
                busses[current_bus].devicetype = HW_NETWORK;
            }
            else {
                printf
                    ("WARNING, \"%s\" (bus %ld) is an unknown device specifier!\n",
                     child->name, current_bus);
            }
            free(txt2);
            txt = xmlNodeListGetString(doc, child->children, 1);
            if (txt != NULL) {
                switch (busses[current_bus].devicetype) {
                    case HW_FILENAME:
                        free(busses[current_bus].filename.path);
                        busses[current_bus].filename.path =
                            malloc(strlen((char *) txt) + 1);
                        strcpy(busses[current_bus].filename.path,
                               (char *) txt);
                        break;
                    case HW_NETWORK:
                        free(busses[current_bus].filename.path);
                        busses[current_bus].net.hostname =
                            malloc(strlen((char *) txt) + 1);
                        strcpy(busses[current_bus].net.hostname,
                               (char *) txt);
                        txt2 = xmlGetProp(child, BAD_CAST "port");
                        if (txt2 != NULL) {
                            busses[current_bus].net.port =
                                atoi((char *) txt2);
                            free(txt2);
                        }
                        else {
                            busses[current_bus].net.port = 0;
                        }
                        txt2 = xmlGetProp(child, BAD_CAST "protocol");
                        if (txt2 != NULL) {
                            struct protoent *p;
                            p = getprotobyname((char *) txt2);
                            busses[current_bus].net.protocol = p->p_proto;
                            free(txt2);
                        }
                        else {
                            busses[current_bus].net.protocol = 6;       /* TCP */
                        }
                        break;
                }
                xmlFree(txt);
            }
            switch (busses[current_bus].devicetype) {
                case HW_FILENAME:
                    DBG(current_bus, DBG_INFO, "** Filename='%s'",
                        busses[current_bus].filename.path);
                    break;
                case HW_NETWORK:
                    DBG(current_bus, DBG_DEBUG,
                        "** Network Host='%s', Protocol=%d Port=%d",
                        busses[current_bus].net.hostname,
                        busses[current_bus].net.protocol,
                        busses[current_bus].net.port);
                    break;
            }
        }

        else if (xmlStrcmp(child->name, BAD_CAST "verbosity") == 0) {
            txt = xmlNodeListGetString(doc, child->children, 1);
            if (txt != NULL) {
                busses[current_bus].debuglevel = atoi((char *) txt);
                xmlFree(txt);
            }
        }

        else if (xmlStrcmp(child->name, BAD_CAST "use_watchdog") == 0) {
            txt = xmlNodeListGetString(doc, child->children, 1);
            if (txt != NULL) {
                if (xmlStrcmp(txt, BAD_CAST "yes") == 0)
                    busses[current_bus].flags |= USE_WATCHDOG;
                xmlFree(txt);
            }
        }

        else if (xmlStrcmp(child->name, BAD_CAST "restore_device_settings")
                 == 0) {
            txt = xmlNodeListGetString(doc, child->children, 1);
            if (txt != NULL) {
                if (xmlStrcmp(txt, BAD_CAST "yes") == 0)
                    busses[current_bus].flags |= RESTORE_COM_SETTINGS;
                xmlFree(txt);
            }
        }

        else if (xmlStrcmp(child->name, BAD_CAST "auto_power_on") == 0) {
            txt = xmlNodeListGetString(doc, child->children, 1);
            if (txt != NULL) {
                if (xmlStrcmp(txt, BAD_CAST "yes") == 0)
                    busses[current_bus].flags |= AUTO_POWER_ON;
                xmlFree(txt);
            }
        }

        else if (xmlStrcmp(child->name, BAD_CAST "speed") == 0) {
            txt = xmlNodeListGetString(doc, child->children, 1);
            if (txt != NULL) {
                int speed = atoi((char *) txt);

                switch (speed) {
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
                    case 57600:
                        busses[current_bus].baudrate = B57600;
                        break;
                    default:
                        busses[current_bus].baudrate = B2400;
                        break;
                }
                xmlFree(txt);
            }
        }

        else
            printf("WARNING, \"%s\" (bus %ld) is an unknown tag!\n",
                   child->name, current_bus);

        child = child->next;
    }

    return busnumber;
}

static int walk_config_xml(xmlDocPtr doc)
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

int readConfig(char *filename)
{
    xmlDocPtr doc;
    int rc;

    // something to initialize
    memset(busses, 0, sizeof(busses));
    num_busses = 0;

    /* some defaults */
    DBG(0, DBG_DEBUG, "parsing %s", filename);
    doc = xmlParseFile(filename);
    /* always show a message */
    if (doc != NULL) {
        DBG(0, DBG_DEBUG, "walking %s", filename);
        rc = walk_config_xml(doc);
        DBG(0, DBG_DEBUG, " done %s; found %d buses", filename, rc);
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
        exit(1);
    }
    return rc;
}

/**
 * suspendThread: Holds the thread until a resume command is given.
        The thread waits in this routines
 * @param busnumber
       bus_t given the bus which thread has to be stopped.
 */
void suspendThread(bus_t busnumber)
{
    /* Lock thread till new data to process arrives */
    pthread_mutex_lock(&busses[busnumber].transmit_mutex);
     /* mutex released.       */
    pthread_cond_wait(&busses[busnumber].transmit_cond, &busses[busnumber].transmit_mutex);
}

/**
 * resumeThread: continue a stopped thread
 * @param busnumber
       bus_t given the bus which thread has to be stopped.
 */
void resumeThread(bus_t busnumber)
{
    /* Let thread process a feedback */
    pthread_mutex_lock(&busses[busnumber].transmit_mutex);
    pthread_cond_signal(&busses[busnumber].transmit_cond);
    pthread_mutex_unlock(&busses[busnumber].transmit_mutex);
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
    va_list parm;
    va_start(parm, fmt);
    if (dbglevel <= busses[busnumber].debuglevel) {
        va_list parm2;
        va_start(parm2, fmt);
        char *msg;
        msg = (char *) malloc(sizeof(char) * (strlen(fmt) + 10));
        if (msg == NULL)
            return;
        sprintf(msg, "[bus %ld] %s", busnumber, fmt);
        vsyslog(LOG_INFO, msg, parm);
        free(msg);
        if (busses[busnumber].debuglevel > DBG_WARN) {
            fprintf(stderr, "[bus %ld] ", busnumber);
            vfprintf(stderr, fmt, parm2);
            if (strchr(fmt, '\n') == NULL)
                fprintf(stderr, "\n");
        }
        va_end(parm2);
    }
    va_end(parm);
}
