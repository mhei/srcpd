/* cvs: $Id$             */

/*
 * Vorliegende Software unterliegt der General Public License,
 * Version 2, 1991. (c) Matthias Trute, 2000-2001.
 *
 * 30.01.2002 Matthias Trute
 *            - Rueckmelder optimiert, Bugfixes (Anregungen von Michael Hodel)
 *
 * 05.07.2001 Frank Schmischke
 *            - Anpassungen wegen srcpd
 *            - es werden nur noch Befehle gesendet, wenn Auftraege vorliegen,
 *              es werden nicht mehr alle Adressen auf Verdacht durchsucht
 *
 */


/* Die Konfiguration des seriellen Ports von M6050emu (D. Schaefer)   */
/* wenngleich etwas ver�ndert, mea culpa..                            */

#include "stdincludes.h"
#include "config-srcpd.h"
#include "io.h"
#include "m605x.h"
#include "srcp-fb.h"
#include "srcp-ga.h"
#include "srcp-gl.h"
#include "srcp-power.h"
#include "srcp-info.h"
#include "srcp-srv.h"
#include "srcp-error.h"
#include "ttycygwin.h"

#define __m6051 ((M6051_DATA*)busses[busnumber].driverdata)

/**
 * readconfig_m605x: Liest den Teilbaum der xml Configuration und
 * parametriert den busspezifischen Datenteil, wird von register_bus()
 * aufgerufen.
 **/

int readconfig_m605x(xmlDocPtr doc, xmlNodePtr node, long int busnumber)
{
    busses[busnumber].type = SERVER_M605X;
    busses[busnumber].init_func = &init_bus_M6051;
    busses[busnumber].term_func = &term_bus_M6051;
    busses[busnumber].thr_func = &thr_sendrec_M6051;
    busses[busnumber].init_gl_func = &init_gl_M6051;
    busses[busnumber].init_ga_func = &init_ga_M6051;
    busses[busnumber].driverdata = malloc(sizeof(struct _M6051_DATA));
    /*TODO: what happens if malloc returns NULL? */
    busses[busnumber].flags |= FB_16_PORTS;
    __m6051->number_fb = 0;     /* max 31 */
    __m6051->number_ga = 256;
    __m6051->number_gl = 80;
    __m6051->ga_min_active_time = 75;
    __m6051->pause_between_cmd = 200;
    __m6051->pause_between_bytes = 2;
    strcpy(busses[busnumber].description,
           "GA GL FB POWER LOCK DESCRIPTION");

    xmlNodePtr child = node->children;
    xmlChar *txt = NULL;

    while (child != NULL) {
        if (xmlStrncmp(child->name, BAD_CAST "text", 4) == 0) {
            /* just do nothing, it is only a comment */
        }

        else if (xmlStrcmp(child->name, BAD_CAST "number_fb") == 0) {
            txt = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
            if (txt != NULL) {
                __m6051->number_fb = atoi((char *) txt);
                xmlFree(txt);
            }
        }

        else if (xmlStrcmp(child->name, BAD_CAST "number_gl") == 0) {
            txt = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
            if (txt != NULL) {
                __m6051->number_gl = atoi((char *) txt);
                xmlFree(txt);
            }
        }

        else if (xmlStrcmp(child->name, BAD_CAST "number_ga") == 0) {
            txt = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
            if (txt != NULL) {
                __m6051->number_ga = atoi((char *) txt);
                xmlFree(txt);
            }
        }

        else if (xmlStrcmp(child->name, BAD_CAST "mode_m6020") == 0) {
            txt = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
            if (txt != NULL) {
                if (xmlStrcmp(txt, BAD_CAST "yes") == 0)
                    __m6051->flags |= M6020_MODE;
                xmlFree(txt);
            }
        }

        else if (xmlStrcmp(child->name, BAD_CAST "p_time") == 0) {
            txt = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
            if (txt != NULL) {
                set_min_time(busnumber, atoi((char *) txt));
                xmlFree(txt);
            }
        }

        else if (xmlStrcmp(child->name, BAD_CAST "ga_min_activetime") == 0) {
            txt = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
            if (txt != NULL) {
                __m6051->ga_min_active_time = atoi((char *) txt);
                xmlFree(txt);
            }
        }

        else if (xmlStrcmp(child->name, BAD_CAST "pause_between_commands")
                 == 0) {
            txt = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
            if (txt != NULL) {
                __m6051->pause_between_cmd = atoi((char *) txt);
                xmlFree(txt);
            }
        }

        else if (xmlStrcmp(child->name, BAD_CAST "pause_between_bytes") ==
                 0) {
            txt = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
            if (txt != NULL) {
                __m6051->pause_between_bytes = atoi((char *) txt);
                xmlFree(txt);
            }
        }

        else
            DBG(busnumber, DBG_WARN,
                "WARNING, unknown tag found: \"%s\"!\n", child->name);;

        child = child->next;
    }

    if (init_GA(busnumber, __m6051->number_ga)) {
        __m6051->number_ga = 0;
        DBG(busnumber, DBG_ERROR, "Can't create array for assesoirs");
    }

    if (init_GL(busnumber, __m6051->number_gl)) {
        __m6051->number_gl = 0;
        DBG(busnumber, DBG_ERROR, "Can't create array for locomotivs");
    }

    if (init_FB(busnumber, __m6051->number_fb * 16)) {
        __m6051->number_fb = 0;
        DBG(busnumber, DBG_ERROR, "Can't create array for feedback");
    }

    return 1;
}


/*******************************************************
 *     SERIELLE SCHNITTSTELLE KONFIGURIEREN
 *******************************************************/
static int init_lineM6051(long int bus)
{
    int FD;
    struct termios interface;

    if (busses[bus].debuglevel > 0) {
        DBG(bus, DBG_INFO, "Opening 605x: %s", busses[bus].device);
    }
    if ((FD = open(busses[bus].device, O_RDWR | O_NONBLOCK)) == -1) {
        DBG(bus, DBG_FATAL, "Couldn't open device %s.",
            busses[bus].device);
        return -1;
    }
    tcgetattr(FD, &interface);
#ifdef linux
    interface.c_cflag = CS8 | CRTSCTS | CREAD | CSTOPB;
    interface.c_oflag = ONOCR | ONLRET;
    interface.c_oflag &= ~(OLCUC | ONLCR | OCRNL);
    interface.c_iflag = IGNBRK | IGNPAR;
    interface.c_iflag &= ~(ISTRIP | IXON | IXOFF | IXANY);
    interface.c_lflag = NOFLSH | IEXTEN;
    interface.c_lflag &= ~(ISIG | ICANON | ECHO | ECHOE | TOSTOP | PENDIN);

    cfsetospeed(&interface, B2400);
    cfsetispeed(&interface, B2400);
#else
    cfmakeraw(&interface);
    interface.c_ispeed = interface.c_ospeed = B2400;
    interface.c_cflag = CREAD | HUPCL | CS8 | CSTOPB | CRTSCTS;
#endif
    tcsetattr(FD, TCSANOW, &interface);
    DBG(bus, DBG_INFO, "Opening 605x succeeded FD=%d", FD);
    return FD;
}

long int init_bus_M6051(long int bus)
{

    DBG(bus, DBG_INFO, "M605x  init: debug %d", busses[bus].debuglevel);
    if (busses[bus].debuglevel <= DBG_DEBUG) {
        busses[bus].fd = init_lineM6051(bus);
    }
    else {
        busses[bus].fd = -1;
    }
    DBG(bus, DBG_INFO, "M605x init done, fd=%d", busses[bus].fd);
    DBG(bus, DBG_INFO, "M605x: %s", busses[bus].description);
    DBG(bus, DBG_INFO, "M605x flags: %d",
        busses[bus].flags & AUTO_POWER_ON);
    return 0;
}

long int term_bus_M6051(long int bus)
{
    DBG(bus, DBG_INFO, "M605x bus term done, fd=%d", busses[bus].fd);
    return 0;
}

/**
 * initGL: modifies the gl data used to initialize the device
 **/
long int init_gl_M6051(struct _GLSTATE *gl)
{
    if (gl->protocol != 'M')
        return SRCP_UNSUPPORTEDDEVICEPROTOCOL;
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
}

/**
 * initGA: modifies the ga data used to initialize the device
 **/
long int init_ga_M6051(struct _GASTATE *ga)
{
    if ((ga->protocol != 'M') || (ga->protocol != 'P'))
        return SRCP_UNSUPPORTEDDEVICEPROTOCOL;
    return SRCP_OK;
}

void *thr_sendrec_M6051(void *v)
{
    unsigned char SendByte;
    int akt_S88, addr, temp, number_fb;
    char c;
    unsigned char rr;
    struct _GLSTATE gltmp, glakt;
    struct _GASTATE gatmp;
    long int bus = (long int) v;
    int ga_min_active_time =
        ((M6051_DATA *) busses[bus].driverdata)->ga_min_active_time;
    int pause_between_cmd =
        ((M6051_DATA *) busses[bus].driverdata)->pause_between_cmd;
    int pause_between_bytes =
        ((M6051_DATA *) busses[bus].driverdata)->pause_between_bytes;
    number_fb = ((M6051_DATA *) busses[bus].driverdata)->number_fb;

    akt_S88 = 1;
    busses[bus].watchdog = 1;

    DBG(bus, DBG_INFO, "M605x on bus %d thread started, fd=%d", bus,
        busses[bus].fd);
    ioctl(busses[bus].fd, FIONREAD, &temp);
    if (((M6051_DATA *) busses[bus].driverdata)->cmd32_pending) {
        SendByte = 32;
        writeByte(bus, SendByte, pause_between_cmd);
        ((M6051_DATA *) busses[bus].driverdata)->cmd32_pending = 0;
    }
    while (temp > 0) {
        readByte(bus, 0, &rr);
        ioctl(busses[bus].fd, FIONREAD, &temp);
        DBG(bus, DBG_INFO, "Ignoring unread byte: %d ", rr);
    }

    while (1) {
        busses[bus].watchdog = 2;

        /* Start/Stop */
        if (busses[bus].power_changed) {
            char msg[1000];
            SendByte = (busses[bus].power_state) ? 96 : 97;
            /* zweimal, wir sind paranoid */
            writeByte(bus, SendByte, pause_between_cmd);
            writeByte(bus, SendByte, pause_between_cmd);
            busses[bus].power_changed = 0;
            infoPower(bus, msg);
            queueInfoMessage(msg);
        }

        /* do nothing, if power off */
        if (busses[bus].power_state == 0) {
            usleep(1000);
            continue;
        }
        busses[bus].watchdog = 3;

        /* Lokdecoder */
        if (!((M6051_DATA *) busses[bus].driverdata)->cmd32_pending) {
            if (!queue_GL_isempty(bus)) {
                unqueueNextGL(bus, &gltmp);
                addr = gltmp.id;
                getGL(bus, addr, &glakt);

                if (gltmp.direction == 2) {
                    gltmp.speed = 0;
                    gltmp.direction = !glakt.direction;
                }
                /* forward/backward */
                if (gltmp.direction != glakt.direction) {
                    c = 15 + 16 * ((gltmp.funcs & 0x10) ? 1 : 0);
                    writeByte(bus, c, pause_between_bytes);
                    SendByte = addr;
                    writeByte(bus, SendByte, pause_between_cmd);
                }
                /* Geschwindigkeit und Licht setzen, erst recht nach
                   Richtungswechsel */
                c = gltmp.speed + 16 * ((gltmp.funcs & 0x01) ? 1 : 0);
                /* jetzt aufpassen: n_fs erzwingt ggf. mehrfache
                   Ansteuerungen des Dekoders, das Protokoll ist da
                   wirklich eigenwillig, vorerst ignoriert! */
                writeByte(bus, c, pause_between_bytes);
                SendByte = addr;
                writeByte(bus, SendByte, pause_between_cmd);
                /* Erweiterte Funktionen des 6021 senden, manchmal */
                if (!((((M6051_DATA *) busses[bus].driverdata)->
                       flags & M6020_MODE) == M6020_MODE)
                    && (gltmp.funcs != glakt.funcs)) {
                    c = ((gltmp.funcs >> 1) & 0x0f) + 64;
                    writeByte(bus, c, pause_between_bytes);
                    SendByte = addr;
                    writeByte(bus, SendByte, pause_between_cmd);
                }
                setGL(bus, addr, gltmp);
            }
            busses[bus].watchdog = 4;
        }
        busses[bus].watchdog = 5;

        /* Magnetantriebe, die muessen irgendwann sehr bald
           abgeschaltet werden */
        if (!queue_GA_isempty(bus)) {
            unqueueNextGA(bus, &gatmp);
            addr = gatmp.id;
            if (gatmp.action == 1) {
                gettimeofday(&gatmp.tv[gatmp.port], NULL);
                setGA(bus, addr, gatmp);
                if (gatmp.activetime >= 0) {
                    gatmp.activetime =
                        (gatmp.activetime > ga_min_active_time) ?
                        ga_min_active_time : gatmp.activetime;
                    // n�chste Aktion ist automatisches Aus
                    gatmp.action = 0;
                }
                else {
                    // egal wieviel, mind. 75m ein
                    gatmp.activetime = ga_min_active_time;
                }
                c = 33 + (gatmp.port ? 0 : 1);
                SendByte = gatmp.id;
                writeByte(bus, c, pause_between_bytes);
                writeByte(bus, SendByte, pause_between_bytes);
                usleep(1000L * (unsigned long) gatmp.activetime);
                ((M6051_DATA *) busses[bus].driverdata)->cmd32_pending = 1;
            }
            if ((gatmp.action == 0)
                && ((M6051_DATA *) busses[bus].driverdata)->cmd32_pending) {
                SendByte = 32;
                writeByte(bus, SendByte, pause_between_cmd);
                ((M6051_DATA *) busses[bus].driverdata)->cmd32_pending = 0;
                setGA(bus, addr, gatmp);
            }

            busses[bus].watchdog = 6;
        }
        busses[bus].watchdog = 7;

        /* S88 Status einlesen, einen nach dem anderen */
        if ((number_fb > 0)
            && !((M6051_DATA *) busses[bus].driverdata)->cmd32_pending) {
            ioctl(busses[bus].fd, FIONREAD, &temp);
            while (temp > 0) {
                readByte(bus, 0, &rr);
                ioctl(busses[bus].fd, FIONREAD, &temp);
                DBG(bus, DBG_INFO,
                    "FB M6051: oops; ignoring unread byte: %d ", rr);
            }
            SendByte = 192 + akt_S88;
            writeByte(bus, SendByte, pause_between_cmd);
            busses[bus].watchdog = 8;
            readByte(bus, 0, &rr);
            temp = rr;
            temp <<= 8;
            busses[bus].watchdog = 9;
            readByte(bus, 0, &rr);
            setFBmodul(bus, akt_S88, temp | rr);
            akt_S88++;
            if (akt_S88 > number_fb)
                akt_S88 = 1;
        }
        busses[bus].watchdog = 10;
        check_reset_fb(bus);
        // fprintf(stderr, " ende\n");
    }
}