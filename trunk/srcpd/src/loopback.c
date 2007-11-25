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

#define __loopback ((LOOPBACK_DATA*)buses[busnumber].driverdata)

int readconfig_LOOPBACK(xmlDocPtr doc, xmlNodePtr node, bus_t busnumber)
{
    buses[busnumber].driverdata = malloc(sizeof(struct _LOOPBACK_DATA));

    if (buses[busnumber].driverdata == NULL) {
        DBG(busnumber, DBG_ERROR,
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
            DBG(busnumber, DBG_WARN,
                    "WARNING, unknown tag found: \"%s\"!\n",
                    child->name);;
        
        child = child->next;
    }

    if (init_GL(busnumber, __loopback->number_gl)) {
        __loopback->number_gl = 0;
        DBG(busnumber, DBG_ERROR, "Can't create array for locomotives");
    }

    if (init_GA(busnumber, __loopback->number_ga)) {
        __loopback->number_ga = 0;
        DBG(busnumber, DBG_ERROR, "Can't create array for accessoires");
    }

    if (init_FB(busnumber, __loopback->number_fb)) {
        __loopback->number_fb = 0;
        DBG(busnumber, DBG_ERROR, "Can't create array for feedback");
    }
    return (1);
}

static int init_lineLoopback(bus_t bus)
{
    int FD;
    FD = -1;
    return FD;
}

int term_bus_LOOPBACK(bus_t bus)
{
    DBG(bus, DBG_INFO, "loopback bus #%ld terminating", bus);
    return 0;
}

/**
 * initGL: modifies the gl data used to initialize the device
 **/
int init_gl_LOOPBACK(struct _GLSTATE *gl)
{
    switch (gl->protocol) {
        case 'L':
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
int init_ga_LOOPBACK(struct _GASTATE *ga)
{
    if ((ga->protocol == 'M') || (ga->protocol == 'N')
        || (ga->protocol == 'P'))
        return SRCP_OK;
    return SRCP_UNSUPPORTEDDEVICEPROTOCOL;
}

/* Initialisiere den Bus, signalisiere Fehler
 * Einmal aufgerufen mit busnummer als einzigem Parameter
 * return code wird ignoriert (vorerst)
 */
int init_bus_LOOPBACK(bus_t i)
{
    DBG(i, DBG_INFO, "loopback init: bus #%ld, debug %d", i,
        buses[i].debuglevel);
    if (buses[i].debuglevel == 0) {
        DBG(i, DBG_INFO, "loopback bus #%ld open device %s (not really!)",
            i, buses[i].device.filename.path);
        buses[i].fd = init_lineLoopback(i);
    }
    else {
        buses[i].fd = -1;
    }
    DBG(i, DBG_INFO, "loopback init done");
    return 0;
}

void *thr_sendrec_LOOPBACK(void *v)
{
    struct _GLSTATE gltmp, glakt;
    struct _GASTATE gatmp;
    int addr;
    bus_t bus = (bus_t) v;

    DBG(bus, DBG_INFO, "loopback started, bus #%d, %s", bus,
        buses[bus].device.filename.path);

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
