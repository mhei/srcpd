/* $Id$ */

/**
 * loopback: simple bus driver without any hardware.
 **/

#include <errno.h>

#include "config-srcpd.h"
#include "io.h"
#include "loopback.h"
#include "srcp-fb.h"
#include "srcp-ga.h"
#include "srcp-gl.h"
#include "srcp-sm.h"
#include "srcp-power.h"
#include "srcp-server.h"
#include "srcp-info.h"
#include "srcp-error.h"
#include "syslogmessage.h"

#define __loopback ((LOOPBACK_DATA*)buses[busnumber].driverdata)
#define __loopbackt ((LOOPBACK_DATA*)buses[btd->bus].driverdata)

#define MAX_CV_NUMBER 255

int cmpTime(struct timeval *t1, struct timeval *t2);


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
    buses[busnumber].thr_func = &thr_sendrec_LOOPBACK;
    buses[busnumber].init_gl_func = &init_gl_LOOPBACK;
    buses[busnumber].init_ga_func = &init_ga_LOOPBACK;
    strcpy(buses[busnumber].description,
           "GA GL FB SM POWER LOCK DESCRIPTION");

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
        syslog_bus(busnumber, DBG_ERROR,
                   "Can't create array for locomotives");
    }

    if (init_GA(busnumber, __loopback->number_ga)) {
        __loopback->number_ga = 0;
        syslog_bus(busnumber, DBG_ERROR,
                   "Can't create array for accessoires");
    }

    if (init_FB(busnumber, __loopback->number_fb)) {
        __loopback->number_fb = 0;
        syslog_bus(busnumber, DBG_ERROR,
                   "Can't create array for feedback");
    }
    return (1);
}

static int init_lineLoopback(bus_t bus)
{
    return -1;
}

/**
 * cacheInitGL: modifies the gl data used to initialize the device
 **/
int init_gl_LOOPBACK(gl_state_t * gl)
{
    switch (gl->protocol) {
        case 'L':
        case 'S':              /*TODO: implement range checks */
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
int init_ga_LOOPBACK(ga_state_t * ga)
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
    static char *protocols = "LSPMN";

    buses[i].protocols = protocols;

    syslog_bus(i, DBG_INFO,
               "loopback start initialization (verbosity = %d).",
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

/*thread cleanup routine for this bus*/
static void end_bus_thread(bus_thread_t * btd)
{
    int result;

    syslog_bus(btd->bus, DBG_INFO, "Loopback bus terminated.");

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

/*main thread routine for this bus*/
void *thr_sendrec_LOOPBACK(void *v)
{
    gl_state_t gltmp, glakt;
    ga_state_t gatmp;
    struct _SM smtmp;
    struct timeval akt_time, cmp_time;
    int addr, ctr;
    int last_cancel_state, last_cancel_type;
    int cv[MAX_CV_NUMBER + 1];

    /* registers 1-4 == CV#1-4; reg5 == CV#29; reg 7-8 == CV#7-8 */
    int reg6 = 1;

    memset(cv, 0, MAX_CV_NUMBER + 1);
    bus_thread_t *btd = (bus_thread_t *) malloc(sizeof(bus_thread_t));

    if (btd == NULL)
        pthread_exit((void *) 1);
    btd->bus = (bus_t) v;
    btd->fd = -1;

    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &last_cancel_state);
    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, &last_cancel_type);

    /*register cleanup routine */
    pthread_cleanup_push((void *) end_bus_thread, (void *) btd);

    /* initialize tga-structure */
    for (ctr = 0; ctr < 50; ctr++)
        __loopbackt->tga[ctr].id = 0;

    syslog_bus(btd->bus, DBG_INFO, "Loopback bus started (device = %s).",
               buses[btd->bus].device.file.path);

    /*enter endless loop to process work tasks */
    while (1) {

        buses[btd->bus].watchdog = 1;

        /*POWER action arrived */
        if (buses[btd->bus].power_changed == 1) {
            buses[btd->bus].power_changed = 0;
            char msg[110];

            infoPower(btd->bus, msg);
            enqueueInfoMessage(msg);
            buses[btd->bus].watchdog++;
        }

        /* loop shortcut to prevent processing of GA, GL, SM (and FB)
         * without power on */
        if (buses[btd->bus].power_state == 0) {

            /* wait 1 ms */
            if (usleep(1000) == -1) {
                syslog_bus(btd->bus, DBG_ERROR,
                           "usleep() failed: %s (errno = %d)\n",
                           strerror(errno), errno);
            }
            continue;
        }

        /*GL action arrived */
        if (!queue_GL_isempty(btd->bus)) {
            dequeueNextGL(btd->bus, &gltmp);
            addr = gltmp.id;
            cacheGetGL(btd->bus, addr, &glakt);

            if (gltmp.direction == 2) {
                gltmp.speed = 0;
                gltmp.direction = !glakt.direction;
            }
            cacheSetGL(btd->bus, addr, gltmp);
            buses[btd->bus].watchdog++;
        }

        gettimeofday(&akt_time, NULL);
        /* first switch of decoders */
        for (ctr = 0; ctr < 50; ctr++) {
            if (__loopbackt->tga[ctr].id) {
                cmp_time = __loopbackt->tga[ctr].t;

                /* switch off time reached? */
                if (cmpTime(&cmp_time, &akt_time)) {
                    gatmp = __loopbackt->tga[ctr];
                    addr = gatmp.id;
                    gatmp.action = 0;
                    setGA(btd->bus, addr, gatmp);
                    __loopbackt->tga[ctr].id = 0;
                }
            }
        }

        /*GA action arrived */
        if (!queue_GA_isempty(btd->bus)) {
            dequeueNextGA(btd->bus, &gatmp);
            addr = gatmp.id;

            gettimeofday(&gatmp.tv[gatmp.port], NULL);
            setGA(btd->bus, addr, gatmp);
            if (gatmp.action && (gatmp.activetime > 0)) {
                for (ctr = 0; ctr < 50; ctr++) {
                    if (__loopbackt->tga[ctr].id == 0) {
                        gatmp.t = akt_time;
                        gatmp.t.tv_sec += gatmp.activetime / 1000;
                        gatmp.t.tv_usec +=
                            (gatmp.activetime % 1000) * 1000;
                        if (gatmp.t.tv_usec > 1000000) {
                            gatmp.t.tv_sec++;
                            gatmp.t.tv_usec -= 1000000;
                        }
                        __loopbackt->tga[ctr] = gatmp;
                        break;
                    }
                }
            }
            buses[btd->bus].watchdog++;
        }

        /*SM action arrived (process only with power on) */
        if (!queue_SM_isempty(btd->bus)) {
            dequeueNextSM(btd->bus, &smtmp);
            session_lock_wait(btd->bus);
            smtmp.value &= 255;
            switch (smtmp.command) {
                case GET:
                    smtmp.value = -1;
                    switch (smtmp.type) {
                        case REGISTER:
                            if ((smtmp.typeaddr > 0) &&
                                (smtmp.typeaddr < 9)) {
                                if (smtmp.typeaddr < 5
                                    || smtmp.typeaddr > 6) {
                                    smtmp.value = cv[smtmp.typeaddr];
                                    if (smtmp.typeaddr < 5)
                                        reg6 = 1;
                                }
                                else if (smtmp.typeaddr == 5) {
                                    cv[29] = smtmp.value = cv[29];
                                }
                                else {
                                    smtmp.value = reg6;
                                }
                            }
                            break;
                        case PAGE:
                        case CV:
                            if ((smtmp.typeaddr >= 0) &&
                                (smtmp.typeaddr <= MAX_CV_NUMBER)) {
                                smtmp.value = cv[smtmp.typeaddr];
                            }
                            break;
                        case CV_BIT:
                            if ((smtmp.typeaddr >= 0) && (smtmp.bit >= 0)
                                && (smtmp.bit < 8) &&
                                (smtmp.typeaddr <= MAX_CV_NUMBER)) {
                                smtmp.value = (1 &
                                               (cv[smtmp.typeaddr] >>
                                                smtmp.bit));
                            }
                            break;
                    }
                    break;
                case SET:
                    switch (smtmp.type) {
                        case REGISTER:
                            if ((smtmp.typeaddr > 0) &&
                                (smtmp.typeaddr < 9)) {
                                if (smtmp.typeaddr < 5
                                    || smtmp.typeaddr > 6) {
                                    if (smtmp.typeaddr < 5)
                                        reg6 = 1;
                                    cv[smtmp.typeaddr] = smtmp.value;
                                }
                                else if (smtmp.typeaddr == 5) {
                                    cv[29] = smtmp.value;
                                }
                                else {
                                    reg6 = smtmp.value;
                                }
                            }
                            else {
                                smtmp.value = -1;
                            }
                            break;
                        case PAGE:
                        case CV:
                            if ((smtmp.typeaddr >= 0) &&
                                (smtmp.typeaddr <= MAX_CV_NUMBER)) {
                                cv[smtmp.typeaddr] = smtmp.value;
                            }
                            else {
                                smtmp.value = -1;
                            }
                            break;
                        case CV_BIT:
                            if ((smtmp.typeaddr >= 0) && (smtmp.bit >= 0)
                                && (smtmp.bit < 8) && (smtmp.value >= 0) &&
                                (smtmp.value <= 1) &&
                                (smtmp.typeaddr <= MAX_CV_NUMBER)) {
                                if (smtmp.value) {
                                    cv[smtmp.typeaddr] |= (1 << smtmp.bit);
                                }
                                else {
                                    cv[smtmp.typeaddr] &=
                                        ~(1 << smtmp.bit);
                                }
                            }
                            else {
                                smtmp.value = -1;
                            }
                            break;
                    }
                    break;
                case VERIFY:
                    switch (smtmp.type) {
                        case REGISTER:
                            if ((smtmp.typeaddr > 0) &&
                                (smtmp.typeaddr < 9)) {
                                if (smtmp.typeaddr < 5
                                    || smtmp.typeaddr > 6) {
                                    if (smtmp.typeaddr < 5)
                                        reg6 = 1;
                                    if (smtmp.value != cv[smtmp.typeaddr])
                                        smtmp.value = -1;
                                }
                                else if (smtmp.typeaddr == 5) {
                                    if (cv[29] != smtmp.value)
                                        smtmp.value = -1;
                                }
                                else {
                                    if (reg6 != smtmp.value)
                                        smtmp.value = -1;
                                }
                            }
                            else {
                                smtmp.value = -1;
                            }
                            break;
                        case PAGE:
                        case CV:
                            if ((smtmp.typeaddr >= 0) &&
                                (smtmp.typeaddr <= MAX_CV_NUMBER)) {
                                if (smtmp.value != cv[smtmp.typeaddr])
                                    smtmp.value = -1;
                            }
                            else {
                                smtmp.value = -1;
                            }
                            break;
                        case CV_BIT:
                            if ((smtmp.typeaddr >= 0) && (smtmp.bit >= 0)
                                && (smtmp.bit < 8) &&
                                (smtmp.typeaddr <= MAX_CV_NUMBER)) {
                                if (smtmp.value != (1 &
                                                    (cv[smtmp.typeaddr] >>
                                                     smtmp.bit)))
                                    smtmp.value = -1;
                            }
                            else {
                                smtmp.value = -1;
                            }
                            break;
                    }
                    break;
            }
            session_endwait(btd->bus, smtmp.value);

            buses[btd->bus].watchdog++;
        }

        /*FB action arrived */
        /* currently nothing to do here */
        buses[btd->bus].watchdog++;

        /* busy wait and continue loop */
        /* wait 1 ms */
        if (usleep(1000) == -1) {
            syslog_bus(btd->bus, DBG_ERROR,
                       "usleep() failed: %s (errno = %d)\n",
                       strerror(errno), errno);
        }
    }

    /*run the cleanup routine */
    pthread_cleanup_pop(1);
    return NULL;
}
