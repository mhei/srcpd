/*
 * Vorliegende Software unterliegt der General Public License,
 * Version 2, 1991. (c) Matthias Trute, 2000-2001.
 *
 */

#include <string.h>

#include "config-srcpd.h"
#include "io.h"
#include "srcp-session.h"
#include "srcp-command.h"
#include "srcp-descr.h"
#include "srcp-error.h"
#include "srcp-fb.h"
#include "srcp-gm.h"
#include "srcp-info.h"
#include "srcp-power.h"
#include "srcp-server.h"
#include "srcp-sm.h"
#include "srcp-time.h"
#include "syslogmessage.h"


/**
 * Core SRCP Commands
 * handle all aspects of the command for all commands
 */

static int handle_setcheck(sessionid_t sessionid, bus_t bus, char *device,
                           char *parameter, char *reply, int setorcheck)
{
    struct timeval time;
    int rc = SRCP_UNSUPPORTEDDEVICEGROUP;
    *reply = 0x00;

    if (bus_has_devicegroup(bus, DG_GL)
        && strncasecmp(device, "GL", 2) == 0) {
        long laddr, direction, speed, maxspeed, f[13];
        int func, i, anzparms;
        func = 0;
        /* We could provide a maximum of 32 on/off functions,
           but for now 12+1 will be good enough */
        anzparms = sscanf(parameter, "%ld %ld %ld %ld %ld %ld %ld %ld "
                "%ld %ld %ld %ld %ld %ld %ld %ld %ld",
                   &laddr, &direction, &speed, &maxspeed, &f[0], &f[1],
                   &f[2], &f[3], &f[4], &f[5], &f[6], &f[7], &f[8], &f[9],
                   &f[10], &f[11], &f[12]);
        for (i = 0; i < anzparms - 4; i++) {
            func += (f[i] ? 1 : 0) << i;
        }
        if (anzparms >= 4) {
            sessionid_t lockid;
            /* Only if not locked or emergency stop !! */
            cacheGetLockGL(bus, laddr, &lockid);
            if (lockid == 0 || lockid == sessionid || direction == 2) {
                rc = SRCP_OK;
                if (setorcheck == 1)
                    rc = queueGL(bus, laddr, direction, speed, maxspeed,
                                 func);
            }
            else {
                rc = SRCP_DEVICELOCKED;
            }
        }
        else {
            rc = SRCP_LISTTOOSHORT;
        }
    }

    else if (bus_has_devicegroup(bus, DG_GA)
        && strncasecmp(device, "GA", 2) == 0) {
        long gaddr, port, aktion, delay;
        sessionid_t lockid;
        int anzparms;
        anzparms =
            sscanf(parameter, "%ld %ld %ld %ld", &gaddr, &port, &aktion,
                   &delay);
        if (anzparms >= 4) {
            /* Port 0,1; Action 0,1 */
            /* Only if not locked!! */
            getlockGA(bus, gaddr, &lockid);
            if (lockid == 0 || lockid == sessionid) {
                rc = SRCP_OK;
                if (setorcheck == 1)
                    rc = queueGA(bus, gaddr, port, aktion, delay);
            }
            else {
                rc = SRCP_DEVICELOCKED;
            }
        }
        else {
            rc = SRCP_LISTTOOSHORT;
        }
    }

    else if (bus_has_devicegroup(bus, DG_FB)
        && strncasecmp(device, "FB", 2) == 0) {
        long fbport, value;
        int anzparms;
        anzparms = sscanf(parameter, "%ld %ld", &fbport, &value);
        if (anzparms >= 2) {
            if (setorcheck == 1)
                rc = setFB(bus, fbport, value);
        }
    }

    /*SET 0 GM <send_to_id> <reply_to_id> <message_type> <message>*/
    else if (bus_has_devicegroup(bus, DG_GM)
        && strncasecmp(device, "GM", 2) == 0) {
        sessionid_t sendto, replyto;
        int result;
        char msg[MAXSRCPLINELEN];

        memset(msg, 0, sizeof(msg));
        /*TODO: scan also message type*/
        result = sscanf(parameter, "%lu %lu %990c", &sendto, &replyto, msg);
        if (result < 3)
            rc = SRCP_LISTTOOSHORT;
        else
            rc = setGM(sendto, replyto, msg);
    }

    else if (bus_has_devicegroup(bus, DG_SM)
        && strncasecmp(device, "SM", 2) == 0) {
        long addr, value1, value2, value3;
        int type;
        char *ctype;

        ctype = malloc(MAXSRCPLINELEN);
        if (ctype == NULL) {
            /* TODO: proper action if malloc returns NULL*/
        }
        sscanf(parameter, "%ld %s %ld %ld %ld", &addr, ctype, &value1,
               &value2, &value3);
        type = -1;
        if (strcasecmp(ctype, "REG") == 0)
            type = REGISTER;
        else if (strcasecmp(ctype, "CV") == 0)
            type = CV;
        else if (strcasecmp(ctype, "CVBIT") == 0)
            type = CV_BIT;
        else if (strcasecmp(ctype, "PAGE") == 0)
            type = PAGE;

        free(ctype);
        if (type == -1)
            rc = SRCP_WRONGVALUE;
        else {
            if (type == CV_BIT)
                rc = infoSM(bus, SET, type, addr, value1, value2, value3,
                            reply);
            else
                rc = infoSM(bus, SET, type, addr, value1, 0, value2,
                            reply);
        }
    }

    else if (bus_has_devicegroup(bus, DG_TIME)
        && strncasecmp(device, "TIME", 4) == 0) {
        long d, h, m, s, nelem;
        nelem = sscanf(parameter, "%ld %ld %ld %ld", &d, &h, &m, &s);
        if (nelem >= 4) {
            rc = SRCP_OK;
            if (setorcheck == 1)
                rc = setTIME(d, h, m, s);
        }
        else
            rc = SRCP_LISTTOOSHORT;
    }

    else if (bus_has_devicegroup(bus, DG_LOCK)
        && strncasecmp(device, "LOCK", 4) == 0) {
        long int addr, duration;
        char devgrp[MAXSRCPLINELEN];
        int nelem = -1;
        if (strlen(parameter) > 0) {
            nelem =
                sscanf(parameter, "%s %ld %ld", devgrp, &addr, &duration);
            syslog_bus(bus, DBG_INFO, "LOCK: %s", parameter);
        }
        if (nelem >= 3) {
            rc = SRCP_UNSUPPORTEDDEVICEGROUP;
            if (strncmp(devgrp, "GL", 2) == 0) {
                rc = SRCP_OK;
                if (setorcheck == 1)
                    rc = cacheLockGL(bus, addr, duration, sessionid);
            }
            else if (strncmp(devgrp, "GA", 2) == 0) {
                rc = SRCP_OK;
                if (setorcheck == 1)
                    rc = lockGA(bus, addr, duration, sessionid);
            }
        }
        else {
            rc = SRCP_LISTTOOSHORT;
        }
    }

    else if (bus_has_devicegroup(bus, DG_POWER)
        && strncasecmp(device, "POWER", 5) == 0) {
        int nelem;
        char state[5], msg[256];
        memset(msg, 0, sizeof(msg));
        nelem = sscanf(parameter, "%3s %100c", state, msg);
        if (nelem >= 1) {
            rc = SRCP_WRONGVALUE;
            if (strncasecmp(state, "OFF", 3) == 0) {
                rc = SRCP_OK;
                if (setorcheck == 1)
                    rc = setPower(bus, 0, msg);
            }
            else if (strncasecmp(state, "ON", 2) == 0) {
                rc = SRCP_OK;
                if (setorcheck == 1)
                    rc = setPower(bus, 1, msg);
            }
        }
        else {
            rc = SRCP_LISTTOOSHORT;
        }
    }

    gettimeofday(&time, NULL);
    srcp_fmt_msg(rc, reply, time);
    return rc;
}

/**
 * SET
 */
int handleSET(sessionid_t sessionid, bus_t bus, char *device, char *parameter,
              char *reply)
{
    return handle_setcheck(sessionid, bus, device, parameter, reply, 1);
}

/***
 * CHECK -- like SET but no command must be sent
 */
int handleCHECK(sessionid_t sessionid, bus_t bus, char *device, char *parameter,
                char *reply)
{
    return handle_setcheck(sessionid, bus, device, parameter, reply, 0);
}

/**
 * GET
 */
int handleGET(sessionid_t sessionid, bus_t bus, char *device, char *parameter,
              char *reply)
{
    struct timeval akt_time;
    int rc = SRCP_UNSUPPORTEDDEVICEGROUP;
    *reply = 0x00;
    gettimeofday(&akt_time, NULL);

    if (bus_has_devicegroup(bus, DG_FB)
        && strncasecmp(device, "FB", 2) == 0) {
        long int nelem, port;
        nelem = sscanf(parameter, "%ld", &port);
        if (nelem >= 1)
            rc = infoFB(bus, port, reply);
        else
            rc = srcp_fmt_msg(SRCP_LISTTOOSHORT, reply, akt_time);
    }

    else if (bus_has_devicegroup(bus, DG_GL)
        && strncasecmp(device, "GL", 2) == 0) {
        long nelem, addr;
        nelem = sscanf(parameter, "%ld", &addr);
        if (nelem >= 1)
            rc = cacheInfoGL(bus, addr, reply);
        else
            rc = srcp_fmt_msg(SRCP_LISTTOOSHORT, reply, akt_time);
    }

    else if (bus_has_devicegroup(bus, DG_GA)
        && strncasecmp(device, "GA", 2) == 0) {
        long addr, port, nelem;
        nelem = sscanf(parameter, "%ld %ld", &addr, &port);
        switch (nelem) {
            case 0:
            case 1:
                rc = srcp_fmt_msg(SRCP_LISTTOOSHORT, reply, akt_time);;
                break;
            case 2:
                rc = infoGA(bus, addr, port, reply);
                break;
            default:
                rc = srcp_fmt_msg(SRCP_LISTTOOLONG, reply, akt_time);;
        }
    }

    else if (bus_has_devicegroup(bus, DG_SM)
        && strncasecmp(device, "SM", 2) == 0) {
        long addr, value1, value2;
        int type;
        char *ctype;

        ctype = malloc(MAXSRCPLINELEN);
        if (ctype == NULL) {
            /* TODO: proper action if malloc returns NULL*/
        }
        sscanf(parameter, "%ld %s %ld %ld", &addr, ctype, &value1,
               &value2);
        type = CV;
        if (strcasecmp(ctype, "REG") == 0)
            type = REGISTER;
        else if (strcasecmp(ctype, "CVBIT") == 0)
            type = CV_BIT;
        else if (strcasecmp(ctype, "PAGE") == 0)
            type = PAGE;
        free(ctype);
        if (type != CV_BIT)
            value2 = 0;
        rc = infoSM(bus, GET, type, addr, value1, value2, 0, reply);
    }

    else if (bus_has_devicegroup(bus, DG_POWER)
        && strncasecmp(device, "POWER", 5) == 0) {
        rc = infoPower(bus, reply);
    }

    else if (bus_has_devicegroup(bus, DG_SERVER)
        && strncasecmp(device, "SERVER", 6) == 0) {
        rc = infoSERVER(reply);
    }

    else if (bus_has_devicegroup(bus, DG_TIME)
        && strncasecmp(device, "TIME", 4) == 0) {
        rc = infoTIME(reply);
        if (rc != SRCP_INFO) {
            rc = srcp_fmt_msg(SRCP_NODATA, reply, akt_time);
        }
    }

    else if (strncasecmp(device, "DESCRIPTION", 11) == 0) {

        /* there are two descriptions */
        long int addr;
        char devgrp[10];
        int nelem = 0;
        if (strlen(parameter) > 0)
            nelem = sscanf(parameter, "%10s %ld", devgrp, &addr);
        if (nelem <= 0) {
            rc = describeBus(bus, reply);
        }
        else {
            if (bus_has_devicegroup(bus, DG_DESCRIPTION)) {
                syslog_bus(bus, DBG_INFO, "DESCRIPTION: devgrp=%s addr=%ld",
                    devgrp, addr);
                if (strncmp(devgrp, "GL", 2) == 0)
                    rc = cacheDescribeGL(bus, addr, reply);
                else if (strncmp(devgrp, "GA", 2) == 0)
                    rc = describeGA(bus, addr, reply);
                else if (strncmp(devgrp, "FB", 2) == 0)
                    rc = describeFB(bus, addr, reply);
                else if (strncmp(devgrp, "SESSION", 7) == 0)
                    rc = describeSESSION(bus, addr, reply);
                else if (strncmp(devgrp, "TIME", 4) == 0)
                    rc = describeTIME(reply);
                else if (strncmp(devgrp, "SERVER", 6) == 0)
                    rc = describeSERVER(bus, addr, reply);
            }
            else {
                rc = srcp_fmt_msg(SRCP_UNSUPPORTEDDEVICEGROUP, reply,
                                  akt_time);
            }
        }
    }

    else if (bus_has_devicegroup(bus, DG_LOCK)
        && (strncasecmp(device, "LOCK", 4) == 0)) {
        long int addr;
        char devgrp[10];
        int nelem = -1;

        if (strlen(parameter) > 0)
            nelem = sscanf(parameter, "%s %ld", devgrp, &addr);
        if (nelem <= 1) {
            rc = srcp_fmt_msg(SRCP_LISTTOOSHORT, reply, akt_time);
        }
        else {
            rc = SRCP_UNSUPPORTEDDEVICEGROUP;
            if (strncmp(devgrp, "GL", 2) == 0)
                rc = describeLOCKGL(bus, addr, reply);
            else if (strncmp(devgrp, "GA", 2) == 0)
                rc = describeLOCKGA(bus, addr, reply);
        }
    }

    if (reply[0] == 0x00)
        rc = srcp_fmt_msg(rc, reply, akt_time);
    return rc;
}


/**
 * WAIT
 */
int handleWAIT(sessionid_t sessionid, bus_t bus, char *device, char *parameter,
               char *reply)
{
    struct timeval time;
    int rc = SRCP_UNSUPPORTEDDEVICEGROUP;
    *reply = 0x00;
    gettimeofday(&time, NULL);

    /* check, if bus has FB's */
    if (bus_has_devicegroup(bus, DG_FB)
        && strncasecmp(device, "FB", 2) == 0) {
        long int port, timeout, nelem;
        int value, waitvalue;
        nelem =
            sscanf(parameter, "%ld %d %ld", &port, &waitvalue, &timeout);
        syslog_bus(bus, DBG_INFO, "wait: %d %d %d", port, waitvalue, timeout);
        if (nelem >= 3) {
            if (getFB(bus, port, &time, &value) == SRCP_OK
                && value == waitvalue) {
                rc = infoFB(bus, port, reply);
            }
            else {
                /* we exactly wait for 1/20 seconds */
                timeout *= 20;
                do {
                    usleep(50000);
                    getFB(bus, port, &time, &value);
                    timeout--;
                }
                while ((timeout >= 0) && (value != waitvalue));

                if (timeout < 0) {
                    gettimeofday(&time, NULL);
                    rc = srcp_fmt_msg(SRCP_TIMEOUT, reply, time);
                }
                else {
                    rc = infoFB(bus, port, reply);
                }
            }
        }
        else {
            rc = srcp_fmt_msg(SRCP_LISTTOOSHORT, reply, time);
        }
    }

    else if (bus_has_devicegroup(bus, DG_TIME)
        && strncasecmp(device, "TIME", 4) == 0) {
        long d, h, m, s;
        int nelem;
        nelem = sscanf(parameter, "%ld %ld %ld %ld", &d, &h, &m, &s);
        if (vtime.ratio_x != 0 && vtime.ratio_y != 0) {
            if (nelem >= 4) {
                /* no calculation, der Zeitfluss ist nicht gleichmaessig! */
                while ((((d * 24 + h) * 60 + m) * 60 + s) >=
                       (((vtime.day * 24 + vtime.hour) * 60 +
                         vtime.min) * 60 + vtime.sec)) {
                    usleep(10000);      /* we wait 10ms real time.. */
                }
                rc = infoTIME(reply);
            }
            else {
                rc = srcp_fmt_msg(SRCP_LISTTOOSHORT, reply, time);
            }
        }
        else {
            rc = srcp_fmt_msg(SRCP_NODATA, reply, time);
        }
    }
    return rc;
}

/**
 * VERIFY
 */
int handleVERIFY(sessionid_t sessionid, bus_t bus, char *device,
                 char *parameter, char *reply)
{
    int rc = SRCP_UNSUPPORTEDOPERATION;
    struct timeval time;
    gettimeofday(&time, NULL);
    srcp_fmt_msg(rc, reply, time);
    return rc;
}

/**
 * TERM
 * negative return code (rc) will terminate current session! */
int handleTERM(sessionid_t sessionid, bus_t bus, char *device, char *parameter,
               char *reply)
{
    struct timeval akt_time;
    int rc = SRCP_UNSUPPORTEDDEVICEGROUP;
    *reply = 0x00;

    if (bus_has_devicegroup(bus, DG_GL)
        && strncasecmp(device, "GL", 2) == 0) {
        long int addr = 0;
        int nelem = 0;
        if (strlen(parameter) > 0)
            nelem = sscanf(parameter, "%ld", &addr);
        if (nelem == 1) {
            sessionid_t lockid;
            cacheGetLockGL(bus, addr, &lockid);
            if (lockid == 0 || lockid == sessionid) {
                rc = cacheUnlockGL(bus, addr, sessionid);
                rc = cacheTermGL(bus, addr);
            }
            else {
                rc = SRCP_DEVICELOCKED;
            }
        }
    }

    else if (bus_has_devicegroup(bus, DG_LOCK)
        && strncasecmp(device, "LOCK", 4) == 0) {
        long int addr;
        char devgrp[10];
        int nelem = -1;
        if (strlen(parameter) > 0)
            nelem = sscanf(parameter, "%s %ld", devgrp, &addr);

        if (nelem <= 1) {
            rc = SRCP_LISTTOOSHORT;
        }
        else {
            rc = SRCP_UNSUPPORTEDDEVICE;
            if (strncmp(devgrp, "GL", 2) == 0) {
                rc = cacheUnlockGL(bus, addr, sessionid);
            }
            else if (strncmp(devgrp, "GA", 2) == 0) {
                rc = unlockGA(bus, addr, sessionid);
            }
        }
    }

    else if (bus_has_devicegroup(bus, DG_SERVER)
        && strncasecmp(device, "SERVER", 6) == 0) {
        rc = SRCP_OK;
        server_shutdown();
    }

    else if (bus_has_devicegroup(bus, DG_SESSION)
        && strncasecmp(device, "SESSION", 7) == 0) {
        sessionid_t termsession = 0;
        int nelem = 0;
        if (strlen(parameter) > 0)
            nelem = sscanf(parameter, "%ld", &termsession);
        if (nelem <= 0)
            termsession = sessionid;
        rc = termSESSION(bus, sessionid, termsession, reply);
    }

    else if (bus_has_devicegroup(bus, DG_SM)
        && strncasecmp(device, "SM", 2) == 0) {
        rc = infoSM(bus, TERM, 0, -1, 0, 0, 0, reply);
    }

    gettimeofday(&akt_time, NULL);
    srcp_fmt_msg(abs(rc), reply, akt_time);
    return rc;
}

/**
 * INIT
 */
int handleINIT(sessionid_t sessionid, bus_t bus, char *device, char *parameter,
               char *reply)
{
    struct timeval time;
    int rc = SRCP_UNSUPPORTEDDEVICEGROUP;

    if (bus_has_devicegroup(bus, DG_GL)
        && strncasecmp(device, "GL", 2) == 0) {
        long addr, protversion, n_fs, n_func, nelem;
        char prot;
        nelem =
            sscanf(parameter, "%ld %c %ld %ld %ld", &addr, &prot,
                   &protversion, &n_fs, &n_func);
        if (nelem >= 5) {
            rc = cacheInitGL(bus, addr, prot, protversion, n_fs, n_func);
        }
        else {
            rc = SRCP_LISTTOOSHORT;
        }
    }

    else if (bus_has_devicegroup(bus, DG_GA)
        && strncasecmp(device, "GA", 2) == 0) {
        long addr, nelem;
        char prot;
        nelem = sscanf(parameter, "%ld %c", &addr, &prot);
        if (nelem >= 2) {
            rc = initGA(bus, addr, prot);
        }
        else {
            rc = SRCP_LISTTOOSHORT;
        }
    }

    else if (bus_has_devicegroup(bus, DG_FB)
        && strncasecmp(device, "FB", 2) == 0) {
        long addr, index, nelem;
        char prot;
        nelem = sscanf(parameter, "%ld %c %ld", &addr, &prot, &index);
        if (nelem >= 3) {
            rc = initFB(bus, addr, prot, index);
        }
        else {
            rc = SRCP_LISTTOOSHORT;
        }
    }

    else if (bus_has_devicegroup(bus, DG_TIME)
        && strncasecmp(device, "TIME", 4) == 0) {
        long rx, ry, nelem;
        nelem = sscanf(parameter, "%ld %ld", &rx, &ry);
        if (nelem >= 2) {
            rc = initTIME(rx, ry);      /* checks also values! */
        }
        else {
            rc = SRCP_LISTTOOSHORT;
        }
    }

    else if (bus_has_devicegroup(bus, DG_SM)
        && strncasecmp(device, "SM", 2) == 0) {
        rc = infoSM(bus, INIT, 0, -1, 0, 0, 0, reply);
    }

    gettimeofday(&time, NULL);
    srcp_fmt_msg(rc, reply, time);
    return rc;
}

/**
 * RESET
 */
int handleRESET(sessionid_t sessionid, bus_t bus, char *device, char *parameter,
                char *reply)
{
    struct timeval time;
    int rc = SRCP_UNSUPPORTEDOPERATION;

    gettimeofday(&time, NULL);
    srcp_fmt_msg(rc, reply, time);
    return rc;
}

/*
 * Command mode network thread routine
 */
int doCmdClient(session_node_t* sn)
{
    char line[MAXSRCPLINELEN], reply[MAXSRCPLINELEN];
    char cbus[MAXSRCPLINELEN], command[MAXSRCPLINELEN],
        devicegroup[MAXSRCPLINELEN], parameter[MAXSRCPLINELEN];
    bus_t bus;
    long int rc, nelem;
    struct timeval akt_time;

    syslog_session(sn->session, DBG_INFO, "Command mode starting.");

    while (1) {
        pthread_testcancel();
        memset(line, 0, sizeof(line));
        if (socket_readline(sn->socket, line, sizeof(line) - 1) < 0) {
            return -1;
        }
        memset(command, 0, sizeof(command));
        memset(devicegroup, 0, sizeof(devicegroup));
        memset(parameter, 0, sizeof(parameter));
        memset(reply, 0, sizeof(reply));
        nelem = sscanf(line, "%s %s %s %1000c", command, cbus,
                devicegroup, parameter);
        bus = atoi(cbus);
        reply[0] = 0x00;

        if (nelem >= 3) {
            if (bus <= num_buses) {
                rc = SRCP_UNKNOWNCOMMAND;
                if (strncasecmp(command, "SET", 3) == 0) {
                    rc = handleSET(sn->session, bus, devicegroup,
                            parameter, reply);
                }
                else if (strncasecmp(command, "GET", 3) == 0) {
                    rc = handleGET(sn->session, bus, devicegroup,
                            parameter, reply);
                }
                else if (strncasecmp(command, "WAIT", 4) == 0) {
                    rc = handleWAIT(sn->session, bus, devicegroup,
                            parameter, reply);
                }
                else if (strncasecmp(command, "INIT", 4) == 0) {
                    rc = handleINIT(sn->session, bus, devicegroup,
                            parameter, reply);
                }
                else if (strncasecmp(command, "TERM", 4) == 0) {
                    rc = handleTERM(sn->session, bus, devicegroup,
                            parameter, reply);
                    /*special option for session termination (?)*/
                    if (rc < 0) {
                        if (socket_writereply(sn->socket, reply) < 0) {
                            break;
                        }
                        break;
                    }
                    rc = abs(rc);
                }
                else if (strncasecmp(command, "VERIFY", 6) == 0) {
                    rc = handleVERIFY(sn->session, bus, devicegroup,
                                      parameter, reply);
                }
                else if (strncasecmp(command, "RESET", 5) == 0) {
                    rc = handleRESET(sn->session, bus, devicegroup,
                                     parameter, reply);
                }
            }
            /* bus > num_buses */
            else {
                rc = SRCP_WRONGVALUE;
                gettimeofday(&akt_time, NULL);
                srcp_fmt_msg(rc, reply, akt_time);
            }
        }
        /* nelem < 3 */
        else {
            syslog_bus(0, DBG_DEBUG, "list too short in session %ld: %d",
                sn->session, nelem);
            rc = SRCP_LISTTOOSHORT;
            gettimeofday(&akt_time, NULL);
            srcp_fmt_msg(rc, reply, akt_time);
        }

        if (socket_writereply(sn->socket, reply) < 0) {
            break;
        }
    }
    return 0;
}
