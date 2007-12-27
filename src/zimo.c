/* $Id$ */

/*
 * zimo: Zimo MX1 Treiber
 */

#include <errno.h>
#include <fcntl.h>
#include <sys/time.h>
#include <time.h>
#include <sys/ioctl.h>

#include "config-srcpd.h"
#include "io.h"
#include "zimo.h"
#include "srcp-fb.h"
#include "srcp-ga.h"
#include "srcp-gl.h"
#include "srcp-sm.h"
#include "srcp-power.h"
#include "srcp-server.h"
#include "srcp-info.h"
#include "srcp-session.h"
#include "syslogmessage.h"


#define __ZIMO ((zimo_DATA*)buses[busnumber].driverdata)

static long tdiff(struct timeval t1, struct timeval t2)
{
    return (t2.tv_sec * 1000 + t2.tv_usec / 1000 - t1.tv_sec * 1000 -
            t1.tv_usec / 1000);
}

int readanswer(bus_t bus, char cmd, char *buf, int maxbuflen,
               long timeout_ms)
{
    int i, status, cnt = 0;
    char c, lc = 13;
    struct timeval ts, tn;

    gettimeofday(&ts, NULL);
    while (1) {
        status = ioctl(buses[bus].device.file.fd, FIONREAD, &i);
        if (status < 0)
            return -1;
        if (i) {
            readByte(bus, 0, (unsigned char *) &c);
            syslog_bus(bus, DBG_INFO, "zimo read %02X", c);
            if ((lc == 10 || lc == 13) && c == cmd)
                cnt = 1;
            if (cnt) {
                if (c == 10 || c == 13)
                    return cnt;
                if (cnt > maxbuflen)
                    return -2;
                buf[cnt - 1] = c;
                cnt++;
            }
            /* syslog_bus(bus, DBG_INFO, "%ld", tdiff(ts,tn)); */
            gettimeofday(&tn, NULL);
            if (tdiff(ts, tn) > timeout_ms)
                return 0;
            lc = c;
        }
        else
            usleep(1);
    }
}

int readconfig_ZIMO(xmlDocPtr doc, xmlNodePtr node, bus_t busnumber)
{
    buses[busnumber].driverdata = malloc(sizeof(struct _zimo_DATA));

    if (buses[busnumber].driverdata == NULL) {
        syslog_bus(busnumber, DBG_ERROR,
                "Memory allocation error in module '%s'.", node->name);
        return 0;
    }

    buses[busnumber].type = SERVER_ZIMO;
    buses[busnumber].init_func = &init_bus_ZIMO;
    buses[busnumber].thr_func = &thr_sendrec_ZIMO;
    strcpy(buses[busnumber].description, "SM GL POWER LOCK DESCRIPTION");

    __ZIMO->number_fb = 0;      /* max 31 */
    __ZIMO->number_ga = 256;
    __ZIMO->number_gl = 80;

    xmlNodePtr child = node->children;
    xmlChar *txt = NULL;

    while (child != NULL) {
        if (xmlStrncmp(child->name, BAD_CAST "text", 4) == 0) {
            /* just do nothing, it is only a comment */
        }

        else if (xmlStrcmp(child->name, BAD_CAST "number_fb") == 0) {
            txt = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
            if (txt != NULL) {
                __ZIMO->number_fb = atoi((char *) txt);
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

        else if (xmlStrcmp(child->name, BAD_CAST "number_gl") == 0) {
            txt = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
            if (txt != NULL) {
                __ZIMO->number_gl = atoi((char *) txt);
                xmlFree(txt);
            }
        }
        
        else if (xmlStrcmp(child->name, BAD_CAST "number_ga") == 0) {
            txt = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
            if (txt != NULL) {
                __ZIMO->number_ga = atoi((char *) txt);
                xmlFree(txt);
            }
        }

        else
            syslog_bus(busnumber, DBG_WARN,
                    "WARNING, unknown tag found: \"%s\"!\n",
                    child->name);;

        child = child->next;
    }                           /* while */

    if (init_GL(busnumber, __ZIMO->number_gl)) {
        __ZIMO->number_gl = 0;
        syslog_bus(busnumber, DBG_ERROR, "Can't create array for locomotives");
    }

    if (init_GA(busnumber, __ZIMO->number_ga)) {
        __ZIMO->number_ga = 0;
        syslog_bus(busnumber, DBG_ERROR, "Can't create array for accessoires");
    }

    if (init_FB(busnumber, __ZIMO->number_fb)) {
        __ZIMO->number_fb = 0;
        syslog_bus(busnumber, DBG_ERROR, "Can't create array for feedback");
    }
    return (1);
}

int init_linezimo(bus_t bus, char *name)
{
    int fd;
    struct termios interface;

    syslog_bus(bus, DBG_INFO, "Try opening serial line %s for 9600 baud\n",
            name);
    fd = open(name, O_RDWR);
    if (fd == -1) {
        syslog_bus(bus, DBG_ERROR,
                "Open serial line failed: %s (errno = %d).\n",
                strerror(errno), errno);
    }
    else {
        tcgetattr(fd, &interface);
        interface.c_oflag = ONOCR;
        interface.c_cflag =
            CS8 | CRTSCTS | CSTOPB | CLOCAL | CREAD | HUPCL;
        interface.c_iflag = IGNBRK;
        interface.c_lflag = IEXTEN;
        cfsetispeed(&interface, B9600);
        cfsetospeed(&interface, B9600);
        interface.c_cc[VMIN] = 0;
        interface.c_cc[VTIME] = 1;
        tcsetattr(fd, TCSANOW, &interface);
    }
    return fd;
}

/* Initialisiere den Bus, signalisiere Fehler */
/* Einmal aufgerufen mit busnummer als einzigem Parameter */
/* return code wird ignoriert (vorerst) */
int init_bus_ZIMO(bus_t bus)
{
    syslog_bus(bus, DBG_INFO, "Zimo init: debug %d, device %s",
        buses[bus].debuglevel, buses[bus].device.file.path);
    buses[bus].device.file.fd = init_linezimo(bus, buses[bus].device.file.path);
    syslog_bus(bus, DBG_INFO, "Zimo init done");
    return 0;
}

/*thread cleanup routine for this bus*/
static void end_bus_thread(bus_thread_t *btd)
{
    int result;

    syslog_bus(btd->bus, DBG_INFO, "Zimo bus terminated.");

    if (buses[btd->bus].device.file.fd != -1)
        close(buses[btd->bus].device.file.fd);

    result = pthread_mutex_destroy(&buses[btd->bus].transmit_mutex);
    if (result != 0) {
        syslog_bus(btd->bus, DBG_WARN,
                "pthread_mutex_destroy() failed: %s (errno = %d).",
                strerror(result), result);
    }

    result = pthread_cond_destroy(&buses[btd->bus].transmit_cond);
    if (result != 0) {
        syslog_bus(btd->bus, DBG_WARN,
                "pthread_mutex_init() failed: %s (errno = %d).",
                strerror(result), result);
    }

    free(buses[btd->bus].driverdata);
    free(btd);
}

void *thr_sendrec_ZIMO(void *v)
{
    gl_state_t gltmp, glakt;
    struct _SM smtmp;
    int addr, temp, i;
    char msg[20];
    char rr;
    char databyte1, databyte2, databyte3;
    unsigned int error, cv, val;
    int last_cancel_state, last_cancel_type;

    bus_thread_t* btd = (bus_thread_t*) malloc(sizeof(bus_thread_t));
    if (btd == NULL)
        pthread_exit((void*) 1);
    btd->bus =  (bus_t) v;
    btd->fd = -1;

    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &last_cancel_state);
    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, &last_cancel_type);

    /*register cleanup routine*/
    pthread_cleanup_push((void *) end_bus_thread, (void *) btd);

    syslog_bus(btd->bus, DBG_INFO, "Zimo bus started (device = %s)",
        buses[btd->bus].device.file.path);
    buses[btd->bus].watchdog = 1;

    while (1) {
        pthread_testcancel();
        if (buses[btd->bus].power_changed == 1) {
            sprintf(msg, "S%c%c", (buses[btd->bus].power_state) ? 'E' : 'A',
                    13);
            writeString(btd->bus, (unsigned char *) msg, 0);
            buses[btd->bus].power_changed = 0;
            infoPower(btd->bus, msg);
            queueInfoMessage(msg);
        }
        if (!queue_GL_isempty(btd->bus)) {
            unqueueNextGL(btd->bus, &gltmp);
            addr = gltmp.id;
            cacheGetGL(btd->bus, addr, &glakt);
            databyte1 = (gltmp.direction ? 0 : 32);
            databyte1 |= (gltmp.funcs & 0x01) ? 16 : 0;
            if (glakt.n_fs == 128)
                databyte1 |= 12;
            if (glakt.n_fs == 28)
                databyte1 |= 8;
            if (glakt.n_fs == 14)
                databyte1 |= 4;
            databyte2 = gltmp.funcs >> 1;
            databyte3 = 0x00;
            if (addr > 128) {
                sprintf(msg, "E%04X%c", addr, 13);
                syslog_bus(btd->bus, DBG_INFO, "%s", msg);
                writeString(btd->bus, (unsigned char *) msg, 0);
                addr = 0;
                i = readanswer(btd->bus, 'E', msg, 20, 40);
                syslog_bus(btd->bus, DBG_INFO, "readed %d", i);
                switch (i) {
                    case 8:
                        sscanf(&msg[1], "%02X", &addr);
                        break;
                    case 10:
                        sscanf(&msg[3], "%02X", &addr);
                        break;
                }
            }
            if (addr) {
                sprintf(msg, "F%c%02X%02X%02X%02X%02X%c", glakt.protocol,
                        addr, gltmp.speed, databyte1, databyte2, databyte3,
                        13);
                syslog_bus(btd->bus, DBG_INFO, "%s", msg);
                writeString(btd->bus, (unsigned char *) msg, 0);
                ioctl(buses[btd->bus].device.file.fd, FIONREAD, &temp);
                while (temp > 0) {
                    readByte(btd->bus, 0, (unsigned char *) &rr);
                    ioctl(buses[btd->bus].device.file.fd, FIONREAD, &temp);
                    syslog_bus(btd->bus, DBG_INFO,
                            "ignoring unread byte: %d (%c)", rr, rr);
                }
                cacheSetGL(btd->bus, addr, gltmp);
            }
        }
        if (!queue_SM_isempty(btd->bus)) {
            int returnvalue = -1;
            unqueueNextSM(btd->bus, &smtmp);
            /* syslog_bus(btd->bus, DBG_INFO, "UNQUEUE SM, cmd:%d addr:%d type:%d typeaddr:%d bit:%04X ",smtmp.command,smtmp.addr,smtmp.type,smtmp.typeaddr,smtmp.bit); */
            addr = smtmp.addr;
            if (addr == 0 && smtmp.type == CV
                    && (smtmp.typeaddr >= 0 && smtmp.typeaddr < 255)) {
                switch (smtmp.command) {
                    case SET:
                        syslog_bus(btd->bus, DBG_INFO, "SM SET #%d %02X",
                                smtmp.typeaddr, smtmp.value);
                        sprintf(msg, "RN%02X%02X%c", smtmp.typeaddr,
                                smtmp.value, 13);
                        writeString(btd->bus, (unsigned char *) msg, 0);
                        session_processwait(btd->bus);
                        if (readanswer(btd->bus, 'Q', msg, 20, 1000) > 3) {
                            sscanf(&msg[1], "%2X%2X%2X", &error, &cv, &val);
                            if (!error && cv == smtmp.typeaddr
                                    && val == smtmp.value)
                                returnvalue = 0;
                        }
                        session_endwait(btd->bus, val);
                        gettimeofday(&smtmp.tv, NULL);
                        setSM(btd->bus, smtmp.type, addr, smtmp.typeaddr,
                                smtmp.bit, smtmp.value, 0);
                        break;
                    case GET:
                        syslog_bus(btd->bus, DBG_INFO, "SM GET #%d", smtmp.typeaddr);
                        sprintf(msg, "Q%02X%c", smtmp.typeaddr, 13);
                        writeString(btd->bus, (unsigned char *) msg, 0);
                        session_processwait(btd->bus);
                        if (readanswer(btd->bus, 'Q', msg, 20, 10000) > 3) {
                            /* sscanf(&msg[1],"%2X%2X%2X",&error,&cv,&val); */
                            sscanf(&msg[1], "%*3c%2X%2X", &cv, &val);
                            syslog_bus(btd->bus, DBG_INFO,
                                    "SM GET ANSWER: error %d, cv %d, val %d",
                                    error, cv, val);
                            /* if(!error && cv==smtmp.typeaddr) */
                            returnvalue = val;
                        }
                        session_endwait(btd->bus, returnvalue);
                        break;
                }
            }

            buses[btd->bus].watchdog = 4;
            usleep(10);
        }
    }

    /*run the cleanup routine*/
    pthread_cleanup_pop(1);
    return NULL;
}
