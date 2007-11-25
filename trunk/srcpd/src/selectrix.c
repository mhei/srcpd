/* cvs: $Id$ */

/**
 * This software is published under the terms of the GNU General Public
 * License, Version 2.0
 * Gerard van der Sel 
 *
 * Version 2.1: 20071006: Re-release of SLX852 and error recovery
 * Version 2.0: 20070418: Release of SLX852
 * Version 1.4: 20070315: Communication with the SLX852
 * Version 1.3: 20070213: Communication with events
 * Version 1.2: 20060526: Text reformatting and error checking
 * Version 1.1: 20060505: Configuration of fb addresses from srcpd.conf
 * Version 1.0: 20050601: Release of Selectrix protocol
 * Version 0.4: 20050521: Feedback response
 * Version 0.3: 20050521: Controlling a switch/signal
 * Version 0.2: 20050514: Controlling a engine
 * Version 0.1: 20050508: Connection with CC-2000 and power on/off
 * Version 0.0: 20050501: Translated file from file M605X which compiles
 */

/**
 * This software does the translation for a selectrix central centre
 * An old Central centre is the default selection
 * In the XML-file the control centre can be changed to the new CC-2000
 *   A CC-2000 can program a engine
 *   (Control centre of MUT and Uwe Magnus are CC-2000 compatible).
 */

#include "stdincludes.h"
#include "portio.h"
#include "config-srcpd.h"
#include "srcp-power.h"
#include "srcp-info.h"
#include "srcp-server.h"
#include "srcp-error.h"
#include "srcp-gl.h"
#include "srcp-ga.h"
#include "srcp-fb.h"
#include "selectrix.h"
#include "ttycygwin.h"

/* Macro definition */
#define __selectrix ((SELECTRIX_DATA *)buses[busnumber].driverdata)
#define __checkSXflag(flag) ((__selectrix->SXflags & (flag)) == (flag))

/*******************************************************************
* readconfig_Selectrix:
* Reads selectrix specific XML nodes and sets up bus specific data.
* Called by register_bus().
********************************************************************/
int readconfig_Selectrix(xmlDocPtr doc, xmlNodePtr node, bus_t busnumber)
{
        int i, offset;
        int portindex = 0;

        DBG(busnumber, DBG_ERROR, 
                "Entering selectrix specific data for bus: %d",
                busnumber);

        buses[busnumber].driverdata = malloc(sizeof(struct _SELECTRIX_DATA));
        if (buses[busnumber].driverdata == NULL) {
                DBG(busnumber, DBG_ERROR,
                        "Memory allocation error in module '%s'.",
                        node->name);
                return 0;
        }

        /* Selectrix specific data to the global data */
        buses[busnumber].type = SERVER_SELECTRIX;
        buses[busnumber].baudrate = B9600;
        buses[busnumber].init_func = &init_bus_Selectrix;
        buses[busnumber].term_func = &term_bus_Selectrix;
        buses[busnumber].thr_func = &thr_commandSelectrix;
        buses[busnumber].thr_timer = &thr_feedbackSelectrix;
        buses[busnumber].sigio_reader = &sig_processSelectrix;
        buses[busnumber].init_gl_func = &init_gl_Selectrix;
        buses[busnumber].init_ga_func = &init_ga_Selectrix;
        buses[busnumber].init_fb_func = &init_fb_Selectrix;
        /* Initialise Selectrix part */
        __selectrix->number_gl = 0;
        __selectrix->number_ga = 0;
        __selectrix->number_fb = 0;
        __selectrix->SXflags = 0;
        __selectrix->startFB = 0;
        __selectrix->max_address = 0x100; /* SXmax; */
        /* Initialise the two array's */
        for (i = 0; i < SXmax; i++) {
                __selectrix->bus_data[i] = 0;         /* Set all outputs to 0 */
                __selectrix->fb_adresses[i] = 255;    /* Set invalid addresses */
        }
        strcpy(buses[busnumber].description,
                "GA GL FB POWER LOCK DESCRIPTION");

        xmlNodePtr child = node->children;
        xmlChar *txt = NULL;

        while (child != NULL) {
                if (xmlStrncmp(child->name, BAD_CAST "text", 4) == 0) {
                        /* just do nothing, it is only a comment */
                } else if (xmlStrcmp(child->name, BAD_CAST "number_fb") == 0) {
                        txt = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
                        if (txt != NULL) {
                                __selectrix->number_fb = atoi((char *) txt);
                               xmlFree(txt);
                        }
                } else if (xmlStrcmp(child->name, BAD_CAST "number_gl") == 0) {
                        txt = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
                        if (txt != NULL) {
                                __selectrix->number_gl = atoi((char *) txt);
                                xmlFree(txt);
                        }
                } else if (xmlStrcmp(child->name, BAD_CAST "number_ga") == 0) {
                        txt = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
                        if (txt != NULL) {
                                __selectrix->number_ga = atoi((char *) txt);
                                xmlFree(txt);
                        }
                } else if (xmlStrcmp(child->name, BAD_CAST "controller") == 0) {
                        txt = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
                        if (txt != NULL) {
                                if (xmlStrcmp(txt, BAD_CAST "CC2000") == 0) {
                                        __selectrix->SXflags |= CC2000_MODE;
                                        /* Last 8 addresses for the CC2000 */
                                        /* __selectrix->max_address = SXcc2000; */
                                        strcpy(buses[busnumber].description,
                                               "GA GL FB SM POWER LOCK DESCRIPTION");
                                }
                        }
                } else if (xmlStrcmp(child->name, BAD_CAST "interface") == 0) {
                        txt = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
                        if (txt != NULL) {
                                if (xmlStrcmp(txt, BAD_CAST "RTHS_0") == 0) {
                                        /* Select  Selectrix mode */
                                        __selectrix->SXflags |= Rautenhaus_MODE;
                                } else if (xmlStrcmp(txt, BAD_CAST "RTHS_1") == 0) {
                                        /* Select  Selectrix mode and two buses */
                                        __selectrix->SXflags |= Rautenhaus_MODE;
                                        __selectrix->SXflags |= Rautenhaus_DBL;
                                } else if (xmlStrcmp(txt, BAD_CAST "RTHS_2") == 0) {
                                        /* Select Rautenhaus mode */
                                        __selectrix->SXflags |= Rautenhaus_MODE;
                                        __selectrix->SXflags |= Rautenhaus_FDBCK;
                                        __selectrix->SXflags |= Rautenhaus_ADR;
                                } else if (xmlStrcmp(txt, BAD_CAST "RTHS_3") == 0) {
                                        /* Select Rautenhaus mode and two buses */
                                        __selectrix->SXflags |= Rautenhaus_MODE;
                                        __selectrix->SXflags |= Rautenhaus_DBL;
                                        __selectrix->SXflags |= Rautenhaus_FDBCK;
                                        __selectrix->SXflags |= Rautenhaus_ADR;
                                }
                        xmlFree(txt);
                        }
                } else if (xmlStrcmp(child->name, BAD_CAST "ports") == 0) {
                        portindex = 1;
                        xmlNodePtr subchild = child->children;
                        xmlChar *subtxt = NULL;
                        while (subchild != NULL) {
                                if (xmlStrncmp(subchild->name,
                                                   BAD_CAST "text", 4) == 0) {
                                        /* just do nothing, it is only a comment */
                                } else if (xmlStrcmp(subchild->name,
                                                       BAD_CAST "port") == 0) {
                                        /* Check if on the second SX-bus */
                                        offset = 0;
                                        xmlChar* pOffset = xmlGetProp(subchild,
                                                        BAD_CAST "sxbus");
                                        if (pOffset != NULL ) {
                                                if (atoi((char *) pOffset) == 1) {
                                                        offset = 128;
                                                }
                                        }
                                        free(pOffset);
                                        /* Get address */
                                        subtxt = xmlNodeListGetString(doc,
                                                subchild->xmlChildrenNode, 1);
                                        if (subtxt != NULL) {
                                                /* Store address and number SXbus */
                                                __selectrix->fb_adresses[portindex] =
                                                        atoi((char *) subtxt) + offset;
                                                DBG(busnumber, DBG_WARN,
                                                    "Adding feedback port number"
                                                    " %d with address %s.",
                                                portindex, subtxt + offset);
                                                xmlFree(subtxt);
                                                if (__selectrix->number_fb > portindex) {
                                                        portindex++;
                                                }
                                        }
                                } else {
                                        DBG(busnumber, DBG_WARN,
	                                        "WARNING, unknown sub "
                                                "tag found: \"%s\"!\n",
	                                        subchild->name);
                                }
                                subchild = subchild->next;
                        }
                } else {
                        DBG(busnumber, DBG_WARN,
                        "WARNING, unknown tag found: \"%s\"!\n",
                        child->name);
                }
                child = child->next;
        }

        if ((__selectrix->number_gl + __selectrix->number_ga +
                __selectrix->number_fb) < __selectrix->max_address) {
                if (init_GL(busnumber, __selectrix->number_gl)) {
                        __selectrix->number_gl = 0;
                        DBG(busnumber, DBG_ERROR, "Can't create array "
                                                  "for locomotives");
                }
                if (init_GA(busnumber, __selectrix->number_ga)) {
                        __selectrix->number_ga = 0;
                        DBG(busnumber, DBG_ERROR, "Can't create array "
                                                  "for assesoirs");
                }
                if (init_FB(busnumber, __selectrix->number_fb * 8)) {
                        __selectrix->number_fb = 0;
                        DBG(busnumber, DBG_ERROR, "Can't create array "
                                                  "for feedback");
                }
        } else {
                __selectrix->number_gl = 0;
                __selectrix->number_ga = 0;
                __selectrix->number_fb = 0;
                DBG(busnumber, DBG_ERROR, "Too many devices on the SX-bus");
        }
        DBG(busnumber, DBG_WARN,
                "Found %d loco's.", __selectrix->number_gl);
        DBG(busnumber, DBG_WARN,
                "Found %d switches.", __selectrix->number_ga);
        DBG(busnumber, DBG_WARN,
                "Found %d feedback modules.", __selectrix->number_fb);
        if (portindex!= 0) {
                for (i = 1; i <= portindex; i++) {
                        DBG(busnumber, DBG_WARN,
                            "Found feedback port number %d with address %d.",
                            i, __selectrix->fb_adresses[i]);
                }
        }
        return 1;
}


/****************************************************************************
* Manage a serial port for communication with a selectrix interface
*****************************************************************************/
/* Opens a serial port */
/* On success the port handle is changed to a value <> -1 */
int init_bus_Selectrix(bus_t busnumber)
{
        DBG(busnumber, DBG_INFO, "Selectrix init: debuglevel %d",
                buses[busnumber].debuglevel);
        if (buses[busnumber].debuglevel <= DBG_DEBUG) {
                open_port(busnumber);
        } else {
                buses[busnumber].fd = -1;
        }
        DBG(busnumber, DBG_INFO, "Selectrix init done, fd=%d",
                buses[busnumber].fd);
        DBG(busnumber, DBG_INFO, "Selectrix description: %s",
                buses[busnumber].description);
        DBG(busnumber, DBG_INFO, "Selectrix flags: %04X (hex)",
                __selectrix->SXflags);
        return 0;
}

/* Close serial port */
int term_bus_Selectrix(bus_t busnumber)
{
        if (buses[busnumber].fd != -1) {
                close_port(busnumber);
        }
        DBG(busnumber, DBG_INFO, "Selectrix bus term done, "
        "fd=%d", buses[busnumber].fd);
        return 0;
}

/*******************************************************
*     Device initialisation
********************************************************/
/* Engines */
/* INIT <bus> GL <adr> S 1 31 2 */
int init_gl_Selectrix(struct _GLSTATE *gl)
{
        if ((gl->protocol == 'S') || (gl->protocol == 's')) {
                return ((gl->n_fs == 31) && (gl->protocolversion == 1) &&
                        (gl->n_func == 2)) ? SRCP_OK : SRCP_WRONGVALUE;
        }
        return SRCP_UNSUPPORTEDDEVICEPROTOCOL;
}

/* Switches, signals, ... */
/* INIT <bus> GA <adr> S */
int init_ga_Selectrix(struct _GASTATE *ga)
{
        if ((ga->protocol == 'S') || (ga->protocol == 's')) {
                return SRCP_OK;
        }
        return SRCP_UNSUPPORTEDDEVICEPROTOCOL;
}

/* Feedback modules */
/* INIT <bus> FB <adr> S <index> */
int init_fb_Selectrix(bus_t busnumber, int adres,
                           const char protocol, int index)
{
        if ((protocol == 'S') || (protocol == 's')) {
                if ((__selectrix->max_address > adres) &&
                     (__selectrix->number_fb >= index)) {
                        __selectrix->fb_adresses[index] = adres;
                        return SRCP_OK;
                } else {
                        return SRCP_WRONGVALUE;
                }
        }
        return SRCP_UNSUPPORTEDDEVICEPROTOCOL;
}

/*******************************************************
*     Rautenhaus setup
********************************************************/
/* Make configuration byte for Rautenhaus interface */
void confRautenhaus(bus_t busnumber)
{
        int configuration;

        /* Check if a Rautenhaus devise is connected */
        if (__checkSXflag(Rautenhaus_MODE)) {
                /* Start with bus 0 selection */
                configuration = RautenhsB0;
                if (__checkSXflag(Rautenhaus_DBL + Rautenhaus_RTBS)) {
                        /* Bus 1 selected so change to bus 1. */
                        configuration = RautenhsB1;
                } else {
                        __selectrix->SXflags &= ~Rautenhaus_RTBS;
                }
                if (__checkSXflag(Rautenhaus_FDBCK)) {
                        /* Rautenhaus mode */
                        configuration |= (cntrlON + fdbckON);
                        if (__checkSXflag(CC2000_MODE)) {
                                /* CC2000, don't check address 111 */
                                /* Don't check bus 0 */
                                configuration |= clkOFF0;
                                if (__checkSXflag(Rautenhaus_DBL)) {
                                        /* Don't check bus 1 */
                                        configuration |= clkOFF1;
                                }
                        }
                } else {
                        /* Selectrix mode */
                        configuration |= (cntrlOFF + fdbckOFF);
                }
                /* Write configuration to the device */
                write_port(busnumber, SXwrite + RautenhsCC);
                write_port(busnumber, configuration);
                DBG(busnumber, DBG_INFO,
                        "Selectrix on bus %ld, Rautenhaus "
                        "configuration is: %02X (hex).",
                        busnumber, configuration);
        }
}

/* Make bus selector for Rautenhaus interface */
void selRautenhaus(bus_t busnumber, int adres)
{
        if (__checkSXflag(Rautenhaus_MODE + Rautenhaus_DBL)) {
                if (adres > 127) {
                        /* Addresses 128 ...  255 => bus 1 */
                        /* Check if bus 1 selected */
                        if (!(__checkSXflag(Rautenhaus_RTBS))) {
                                /* No, select bus 1 */
                                write_port(busnumber, SXwrite + RautenhsCC);
                                write_port(busnumber, RautenhsB1);
                                __selectrix->SXflags |= Rautenhaus_RTBS;
                                DBG(busnumber, DBG_INFO,
                                        "Selectrix on bus %ld, Rautenhaus "
                                        "bus 1 selected.",
                                        busnumber);
                        }
                } else {
                        /* Addresses 0 ... 127 => bus 0 */
                        /* Check if bus 0 selected */
                        if (__checkSXflag(Rautenhaus_RTBS)) {
                                /* No, select bus 0 */
                                write_port(busnumber, SXwrite + RautenhsCC);
                                write_port(busnumber, RautenhsB0);
                                __selectrix->SXflags &= ~Rautenhaus_RTBS;
                                DBG(busnumber, DBG_INFO,
                                        "Selectrix on bus %ld, Rautenhaus "
                                        "bus 0 selected.",
                                        busnumber);
                        }
                }
        }
}

/*******************************************************
*     Base communication with the interface (Selectrix)
********************************************************/
/* Read data from the SX-bus (8 bits) */
int readSXbus(bus_t busnumber)
{
        unsigned int rr;

        if (buses[busnumber].fd != -1 ) {
                /* Wait until a character arrives */
                rr = read_port(busnumber);
                DBG(busnumber, DBG_DEBUG,
                       "Selectrix on bus %ld, read byte %02X (hex).",
                       busnumber, rr);
                if (rr < 0x100) {
                        return rr ;
                }
        }
        return 0xFF;        /* Error or closed, return all blocked */
}

void commandreadSXbus(bus_t busnumber, int SXadres)
{
        if (buses[busnumber].fd != -1 ) {
                /* Select Rautenhaus bus */
                selRautenhaus(busnumber, SXadres);
                SXadres &= 0x7F;
                /* write SX-address and the read command */
                write_port(busnumber, SXread + SXadres);
                /* extra byte for power to receive data */
                write_port(busnumber, 0xaa);
                /* receive data */
        } else {
                DBG(busnumber, DBG_DEBUG,
                        "Selectrix on bus %ld, address %d not read.",
                        busnumber, SXadres);
        }
}

/* Write data to the SX-bus (8bits) */
void writeSXbus(bus_t busnumber, int SXadres, int SXdata)
{
        if (buses[busnumber].fd != -1) {
                /* Select Rautenhaus bus */
                selRautenhaus(busnumber, SXadres);
                SXadres &= 0x7F;
                /* write SX-address and the write command */
                write_port(busnumber, SXwrite + SXadres);
                /* write data to the SX-bus */
                write_port(busnumber, SXdata);
        } else {
                DBG(busnumber, DBG_DEBUG,
                        "Selectrix on bus %ld, byte %d not to address %d.",
                        busnumber, SXdata, SXadres);
        }
}

/*******************************************************
*     Command generation (Selectrix)
********************************************************/
void *thr_commandSelectrix(void *v)
{
        int addr, data, state;
        struct _GLSTATE gltmp;
        struct _GASTATE gatmp;

        bus_t busnumber = (bus_t) v;
        state = 0;
        buses[busnumber].watchdog = 0;
        DBG(busnumber, DBG_INFO,
                "Selectrix on bus #%ld command thread started.",
                busnumber);
        while (1) {
                buses[busnumber].watchdog = 1;
                /* Start/Stop */
                if (buses[busnumber].power_changed != 0) {
                        state = 1;
                        char msg[1000];
                        buses[busnumber].watchdog = 2;
                        if ((buses[busnumber].power_state)) {
                                /* Turn power on */
                                writeSXbus(busnumber, SXcontrol, 0x80);
                                confRautenhaus(busnumber);
                        } else {
                                /* Turn power off */
                                writeSXbus(busnumber, SXcontrol, 0x00);
                        }
                        infoPower(busnumber, msg);
                        queueInfoMessage(msg);
                        DBG(busnumber, DBG_DEBUG,
                                "Selectrix on bus %ld had a power change.",
                                busnumber);
                        buses[busnumber].power_changed = 0;
                }
                buses[busnumber].watchdog = 3;
                /* Do nothing, if power off (maybe programming) */
                if (buses[busnumber].power_state == 0) {
                        buses[busnumber].watchdog = 4;
                        usleep(100000);
                        continue;
                }
                /* Loco decoders */
                buses[busnumber].watchdog = 5;
                if (!queue_GL_isempty(busnumber)) {
                        state = 2;
                        buses[busnumber].watchdog = 6;
                        unqueueNextGL(busnumber, &gltmp);
                        /* Address of the engine */
                        addr = gltmp.id;
                        /* Check if valid address */
                        if (__selectrix->max_address > addr) {
                                /* Check: terminating the engine */
                                if (gltmp.state == 2) {
                                        DBG(busnumber, DBG_DEBUG,
                                                "Selectrix on bus %ld, engine "
                                                "with address %d is removed",
                                                busnumber, addr);
                                        __selectrix->number_gl--;
                                } else {
                                        /* Direction */
                                        switch (gltmp.direction) {
                                        case 0:
                                                /* Backward */
                                                data = 0x20;
                                                break;
                                        case 1:
                                                /* Forward */
                                                data = 0x00;
                                                break;
                                        default:
                                                /* Emergency stop or ... */
                                                /* Get last direction */
                                                data = __selectrix->bus_data[addr] & 0x20;
                                                gltmp.speed = 0;
                                                break;
                                        }
                                        /* Speed, Light, Function */
                                        data = data + gltmp.speed +
                                                ((gltmp.funcs & 0x01) ? 0x40 : 0) +
                                                ((gltmp.funcs & 0x02) ? 0x80 : 0);
                                        writeSXbus(busnumber, addr, data);
                                        __selectrix->bus_data[addr] = data;
                                        cacheSetGL(busnumber, addr, gltmp);
                                        DBG(busnumber, DBG_DEBUG,
                                                "Selectrix on bus %ld, "
                                                "engine with address %d "
                                                "has data %02X (hex).",
                                                busnumber, addr, data);
                                }
                        } else {
                                DBG(busnumber, DBG_DEBUG,
                                        "Selectrix on bus %ld, invalid "
                                        "address %d with engine",
                                        busnumber, addr);
                        }
                }
                buses[busnumber].watchdog = 7;
                /* drives solenoids and signals */
                if (!queue_GA_isempty(busnumber)) {
                        state = 3;
                        buses[busnumber].watchdog = 8;
                        unqueueNextGA(busnumber, &gatmp);
                        addr = gatmp.id;
                        if (__selectrix->max_address > addr) {
                                data = __selectrix->bus_data[addr];
                                /* Select the action to do */
                                if (gatmp.action == 0) {
                                        /* Set pin to "0" */
                                        switch (gatmp.port) {
                                        case 1:
                                                data &= 0xfe;
                                                break;
                                        case 2:
                                                data &= 0xfd;
                                                break;
                                        case 3:
                                                data &= 0xfb;
                                                break;
                                        case 4:
                                                data &= 0xf7;
                                                break;
                                        case 5:
                                                data &= 0xef;
                                                break;
                                        case 6:
                                                data &= 0xdf;
                                                break;
                                        case 7:
                                                data &= 0xbf;
                                                break;
                                        case 8:
                                                data &= 0x7f;
                                                break;
                                        default:
                                                DBG(busnumber, DBG_DEBUG,
                                                        "Selectrix on "
                                                        "bus %ld, invalid "
                                                        "port number %d with "
                                                        "switch/signal or ...",
                                                        busnumber,  gatmp.port);
                                                break;
                                        }
                                } else {
                                        /* Set pin to "1" */
                                        switch (gatmp.port){
                                        case 1:
                                                data |= 0x01;
                                                break;
                                        case 2:
                                                data |= 0x02;
                                                break;
                                        case 3:
                                                data |= 0x04;
                                                break;
                                        case 4:
                                                data |= 0x08;
                                                break;
                                        case 5:
                                                data |= 0x10;
                                                break;
                                        case 6:
                                                data |= 0x20;
                                                break;
                                        case 7:
                                                data |= 0x40;
                                                break;
                                        case 8:
                                                data |= 0x80;
                                                break;
                                        default:
                                                DBG(busnumber, DBG_DEBUG,
                                                        "Selectrix on "
                                                        "bus %ld, invalid "
                                                        "port number %d with "
                                                        "switch/signal or ...",
                                                        busnumber, gatmp.port);
                                               break;
                                        }
                                }
                                writeSXbus(busnumber, addr, data);
                                __selectrix->bus_data[addr] = data;
                                DBG(busnumber, DBG_DEBUG,
                                        "Selectrix on bus %ld, address %d "
                                        "has new data %02X (hex).",
                                        busnumber, addr, data);
                        } else {
                                DBG(busnumber, DBG_DEBUG,
                                        "Selectrix on bus %ld, invalid "
                                        "address %d with switch/signal or ...",
                                        busnumber, addr);
                        }
                }
                buses[busnumber].watchdog = 9;
                /* Feed back contacts */
                if ((__selectrix->number_fb > 0) &&
                     (__selectrix->startFB == 1)) {
                        state = 4;
                        buses[busnumber].watchdog = 10;
                        /* Fetch the module address */
                        addr = __selectrix->fb_adresses[__selectrix->currentFB];
                        if (__selectrix->max_address > addr) {
                                /* Send command to read the SX-bus */
                                __selectrix->startFB = 2;
                                commandreadSXbus(busnumber, addr);
                        } else {
                                __selectrix->startFB = 0;
                                DBG(busnumber, DBG_DEBUG,
                                        "Selectrix on bus %ld, invalid "
                                        "address %d with feedback index %d.",
                                        busnumber, addr, __selectrix->currentFB);
                        }
                }
                if (state == 0) {
                        /* Lock thread till new data to process arrives */
                        suspendThread(busnumber);
                }
        }
}

/*******************************************************
*     Timed command generation (Selectrix)
********************************************************/
void *thr_feedbackSelectrix(void *v)
{
        int addr;

        bus_t busnumber = (bus_t) v;
        DBG(busnumber, DBG_INFO, "Selectrix on bus #%ld "
                "feedback thread started.",
                busnumber);
        __selectrix->currentFB = __selectrix->number_fb;
        while (1) {
                /* Feed back contacts */
                if ((__selectrix->number_fb > 0) &&
                    !(__checkSXflag(Rautenhaus_MODE + Rautenhaus_FDBCK))) {
                        switch (__selectrix->startFB) {
                        case 2:
                                __selectrix->startFB = 1;
                        case 1:
                                resumeThread(busnumber);
                                break;
                        case 0:
                                /* Select the next module */
                                if (__selectrix->currentFB >= __selectrix->number_fb) {
                                           /* Reset to start */
                                        __selectrix->currentFB = 1;
                                } else {
                                        /* Next */
                                        __selectrix->currentFB++;
                                }
                                /* Fetch the module address */
                                addr = __selectrix->fb_adresses[__selectrix->currentFB];
                                if (__selectrix->max_address > addr) {
                                        /* Let thread process a feedback */
                                        __selectrix->startFB = 1;
                                        resumeThread(busnumber);
                                } else {
                                        DBG(busnumber, DBG_INFO,
                                                "Selectrix on bus %ld, "
                                                "invalid address %d "
                                                "with feedback index %d.",
                                                busnumber, addr,
                                                __selectrix->currentFB);
                                }
                                break;
                        default:
                                __selectrix->startFB = 0;
                                break;
                        }
                        /* Process every feedback 4 times per second */
                        usleep(250000 / __selectrix->number_fb);
                } else {
                        usleep(1000000);
                }
        }
}

/*******************************************************
*     Command processing (Selectrix)
********************************************************/
void sig_processSelectrix(bus_t busnumber)
{
        int data, addr;
        int found;
        int dataUP, i;

        DBG(busnumber, DBG_INFO, "Selectrix on bus #%ld signal processed.",
                busnumber);
        /* Read the SX-bus */
        data = readSXbus(busnumber);
        if (__selectrix->startFB == 2) {
                addr = __selectrix->fb_adresses[__selectrix->currentFB];
                __selectrix->bus_data[addr] = data;
                DBG(busnumber, DBG_INFO,
                        "Selectrix on bus %ld, address %d "
                        "has feedback data %02X (hex).",
                        busnumber, addr, data);
                /* Rotate bits 7 ... 0 to 1 ... 8 */
                dataUP = 0;
                for (i = 0; i < 8; i++) {
                        dataUP = dataUP * 2;
                        dataUP = dataUP + (data & 0x01);
                        data = data / 2;
                }
                /* Set the daemon global data */
                /* Use 1, 2, ... as address for feedback */
                setFBmodul(busnumber, __selectrix->currentFB, dataUP);
                /* Use real address for feedback */
                /* setFBmodul(busnumber, addr, dataUP); */
                __selectrix->startFB = 0;
        } else if (__checkSXflag(Rautenhaus_MODE + Rautenhaus_FDBCK)) {
                if (__checkSXflag(Rautenhaus_ADR)) {
                        /* 1: SX-bus address */
                        found = TRUE;
                        __selectrix->currentFB = 1;
                        while ((found == TRUE) &&
                                 !(__selectrix->currentFB > __selectrix->number_fb)) {
                                if (data == __selectrix->fb_adresses[__selectrix->currentFB]) {
                                        found = FALSE;
                                        __selectrix->SXflags &= ~Rautenhaus_ADR;
                                } else {
                                        __selectrix->currentFB++;
                                }
                        }
                } else {
                        /* 0: SX-bus data */
                        addr = __selectrix->fb_adresses[__selectrix->currentFB];
                        __selectrix->bus_data[addr] = data;
                        DBG(busnumber, DBG_INFO,
                                "Selectrix on bus %ld, address %d "
                                "has feedback data %02X (hex).",
                                busnumber, addr, data);
                        /* Rotate bits 7 ... 0 to 1 ... 8 */
                        dataUP = 0;
                        for (i = 0; i < 8; i++) {
                                dataUP = dataUP * 2;
                                dataUP = dataUP + (data & 0x01);
                                data = data / 2;
                        }
                        /* Set the daemon global data */
                        /* Use 1, 2, ... as address for feedback */
                        setFBmodul(busnumber, __selectrix->currentFB, dataUP);
                        /* Use real address for feedback */
                        /* setFBmodul(busnumber, addr, dataUP); */
                        __selectrix->SXflags |= Rautenhaus_ADR;
                }
        } else {
                DBG(busnumber, DBG_INFO,
                        "Selectrix on bus %ld, discarded data %02X (hex).",
                        busnumber, data);
        }
}
