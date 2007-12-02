/* $Id$ */

/**
 * loopback: simple Bus driver without any hardware.
 **/

#include "stdincludes.h"

#include "config-srcpd.h"
#include "io.h"
#include "loopback.h"
#include "srcp-fb.h"
#include "srcp-ga.h"
#include "srcp-gl.h"
#include "srcp-power.h"
#include "srcp-server.h"
#include "srcp-info.h"
#include "srcp-error.h"
#include "syslogmessage.h"

#define __loopback ((LOOPBACK_DATA*)buses[busnumber].driverdata)

int readconfig_LOOPBACK(xmlDocPtr doc, xmlNodePtr node, bus_t busnumber)
{
    buses[busnumber].driverdata = malloc(sizeof(struct _LOOPBACK_DATA));

    if (buses[busnumber].driverdata == NULL) {
        syslog_bus(busnumber, DBG_ERROR,
                "Memory allocation error in module '%s'.", node->name);
        return 0;
    }

    buses[busnumber].type = SERVER_LOOPBACK;
    buses[busnumber].init_func = &init_bus_LOOPBACK;
    buses[busnumber].term_func = &term_bus_LOOPBACK;
    buses[busnumber].thr_func = &thr_sendrec_LOOPBACK;
    buses[busnumber].init_gl_func = &init_gl_LOOPBACK;
    buses[busnumber].init_ga_func = &init_ga_LOOPBACK;
    strcpy(buses[busnumber].description,
           "GA GL FB POWER LOCK DESCRIPTION");

    __loopback->number_fb = 0;  /* max 31 */
    __loopback->number_ga = 256;
    __loopback->number_gl = 80;

    xmlNodePtr child = node->children;
    xmlChar *txt = NULL;

    while (child != NULL) {
        if (xmlStrncmp(child->name, BAD_CAST "text", 4) == 0) {
            /* just do nothing, it is only a comment */
        }

        else if (xmlStrcmp(child->name, BAD_CAST "number_fb") == 0) {
            txt = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
            if (txt != NULL) {
                __loopback->number_fb = atoi((char *) txt);
                xmlFree(txt);
            }
        }
        
        else if (xmlStrcmp(child->name, BAD_CAST "number_gl") == 0) {
            txt = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
            if (txt != NULL) {
                __loopback->number_gl = atoi((char *) txt);
                xmlFree(txt);
            }
        }
        
        else if (xmlStrcmp(child->name, BAD_CAST "number_ga") == 0) {
            txt = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
            if (txt != NULL) {
                __loopback->number_ga = atoi((char *) txt);
                xmlFree(txt);
            }
        }

        else
            syslog_bus(busnumber, DBG_WARN,
                    "WARNING, unknown tag found: \"%s\"!\n",
                    child->name);;
        
        child = child->next;
    }

    if (init_GL(busnumber, __loopback->number_gl)) {
        __loopback->number_gl = 0;
        syslog_bus(busnumber, DBG_ERROR, "Can't create array for locomotives");
    }

    if (init_GA(busnumber, __loopback->number_ga)) {
        __loopback->number_ga = 0;
        syslog_bus(busnumber, DBG_ERROR, "Can't create array for accessoires");
    }

    if (init_FB(busnumber, __loopback->number_fb)) {
        __loopback->number_fb = 0;
        syslog_bus(busnumber, DBG_ERROR, "Can't create array for feedback");
    }
    return (1);
}

static int init_lineLoopback(bus_t bus)
{
    return -1;
}

int term_bus_LOOPBACK(bus_t bus)
{
    syslog_bus(bus, DBG_INFO, "loopback bus #%ld terminating", bus);
    free(buses[bus].driverdata);
    return 0;
}

/**
 * cacheInitGL: modifies the gl data used to initialize the device
 **/
int init_gl_LOOPBACK(gl_state_t *gl)
{
    switch (gl->protocol) {
        case 'L':
        case 'S': /*TODO: implement range checks*/
        case 'P':
            return SRCP_OK;
            break;
        case 'M':
            switch (gl->protocolversion) {
                case 1:
                    return (gl->n_fs == 14) ? SRCP_OK : SRCP_WRONGVALUE;
                    break;
                case 2:
                    return ((gl->n_fs == 14) ||
                            (gl->n_fs == 27) ||
                            (gl->n_fs == 28)) ? SRCP_OK : SRCP_WRONGVALUE;
                    break;
            }
            return SRCP_WRONGVALUE;
            break;
        case 'N':
            switch (gl->protocolversion) {
                case 1:
                    return ((gl->n_fs == 14) ||
                            (gl->n_fs == 128)) ? SRCP_OK : SRCP_WRONGVALUE;
                    break;
                case 2:
                    return ((gl->n_fs == 14) ||
                            (gl->n_fs == 128)) ? SRCP_OK : SRCP_WRONGVALUE;
                    break;
            }
            return SRCP_WRONGVALUE;
            break;
    }
    return SRCP_UNSUPPORTEDDEVICEPROTOCOL;
}

/**
 * initGA: modifies the ga data used to initialize the device
 **/
int init_ga_LOOPBACK(ga_state_t *ga)
{
    if ((ga->protocol == 'M') || (ga->protocol == 'N')
        || (ga->protocol == 'P') || (ga->protocol == 'S'))
        return SRCP_OK;
    return SRCP_UNSUPPORTEDDEVICEPROTOCOL;
}

/* Initialisiere den Bus, signalisiere Fehler
 * Einmal aufgerufen mit busnummer als einzigem Parameter
 * return code wird ignoriert (vorerst)
 */
int init_bus_LOOPBACK(bus_t i)
{
    syslog_bus(i, DBG_INFO, "loopback start initialization (verbosity = %d).",
        buses[i].debuglevel);
    if (buses[i].debuglevel == 0) {
        syslog_bus(i, DBG_INFO, "loopback open device %s (not really!).",
            buses[i].device.file.path);
        buses[i].device.file.fd = init_lineLoopback(i);
    }
    else {
        buses[i].device.file.fd = -1;
    }
    syslog_bus(i, DBG_INFO, "loopback initialization done.");
    return 0;
}

void *thr_sendrec_LOOPBACK(void *v)
{
    gl_state_t gltmp, glakt;
    ga_state_t gatmp;
    int addr;
    bus_t bus = (bus_t) v;

    syslog_bus(bus, DBG_INFO, "loopback bus thread started %s",
        buses[bus].device.file.path);

    buses[bus].watchdog = 1;

    while (1) {
        if (buses[bus].power_changed == 1) {
            buses[bus].power_changed = 0;
            /*
            char msg[110];
            infoPower(bus, msg);
            queueInfoMessage(msg);
            */
        }
        if (buses[bus].power_state == 0) {
            usleep(1000);
            continue;
        }

        if (!queue_GL_isempty(bus)) {
            unqueueNextGL(bus, &gltmp);
            addr = gltmp.id;
            cacheGetGL(bus, addr, &glakt);

            if (gltmp.direction == 2) {
                gltmp.speed = 0;
                gltmp.direction = !glakt.direction;
            }
            cacheSetGL(bus, addr, gltmp);
        }
        buses[bus].watchdog = 4;
        if (!queue_GA_isempty(bus)) {
            unqueueNextGA(bus, &gatmp);
            addr = gatmp.id;
            if (gatmp.action == 1) {
                gettimeofday(&gatmp.tv[gatmp.port], NULL);
            }
            setGA(bus, addr, gatmp);
            buses[bus].watchdog = 6;
        }
        usleep(1000);
    }
}
