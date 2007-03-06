
/*
 * Vorliegende Software unterliegt der General Public License,
 * Version 2, 1991. (c) Matthias Trute, 2000-2001.
 *
 *
 * 2004-03-02 Frank Schmischke
 *            - added some handling for ServiceMode
 *
 * 2002-12-29 Manuel Borchers
 *            handleSET():
 *            - getlockGL changed to getlockGA in the GA-command-processing
 *            - if the device is locked, return code is now set to
 *              SRCP_DEVICELOCKED
 */

#include "stdincludes.h"

#include "config-srcpd.h"
#include "srcp-session.h"
#include "io.h"
#include "srcp-error.h"
#include "srcp-fb.h"
#include "srcp-time.h"
#include "srcp-ga.h"
#include "srcp-gl.h"
#include "srcp-gm.h"
#include "srcp-power.h"
#include "srcp-sm.h"
#include "srcp-srv.h"

#include "srcp-descr.h"
#include "srcp-time.h"

#include "srcp-info.h"
#include "netserver.h"
#include "threads.h"

#define COMMAND 1
#define INFO    2

extern struct _VTIME vtime;
extern char *WELCOME_MSG;


/**
 * Shakehand phase.
 *
 */


void* thr_doClient(void *v)
{
    int Socket = (int) v;
    char line[MAXSRCPLINELEN], cmd[MAXSRCPLINELEN],
        parameter[MAXSRCPLINELEN], reply[MAXSRCPLINELEN];
    int mode = COMMAND;
    sessionid_t sessionid;
    int rc, nelem;
    struct timeval time;
    /* drop root permission for this thread */
    change_privileges(0);

    if (socket_writereply(Socket, WELCOME_MSG) < 0) {
        shutdown(Socket, SHUT_RDWR);
        close(Socket);
        return NULL;
    }

    while (1) {
        rc = SRCP_HS_NODATA;
        reply[0] = 0x00;
        memset(line, 0, sizeof(line));

        if (socket_readline(Socket, line, sizeof(line) - 1) < 0) {
            shutdown(Socket, SHUT_RDWR);
            close(Socket);
            return NULL;
        }

        memset(cmd, 0, sizeof(cmd));
        memset(parameter, 0, sizeof(parameter));
        nelem = sscanf(line, "%s %1000c", cmd, parameter);

        if (nelem > 0) {
            rc = SRCP_UNKNOWNCOMMAND;

            if (strncasecmp(cmd, "GO", 2) == 0) {
		sessionid = session_getnextID();
                gettimeofday(&time, NULL);
                sprintf(reply, "%lu.%.3lu 200 OK GO %ld\n", time.tv_sec,
                        time.tv_usec / 1000, sessionid);
                if (socket_writereply(Socket, reply) < 0) {
                    shutdown(Socket, SHUT_RDWR);
                    close(Socket);
                    return NULL;
                }
                start_session(sessionid, mode);
                switch (mode) {
                    case COMMAND:
                        rc = doCmdClient(Socket, sessionid);
                        break;
                    case INFO:
                        rc = doInfoClient(Socket, sessionid);
                        break;
                }
                stop_session(sessionid);
                return NULL;
            }

            else if (strncasecmp(cmd, "SET", 3) == 0) {
                char p[MAXSRCPLINELEN], setcmd[MAXSRCPLINELEN];
                int n = sscanf(parameter, "%s %1000c", setcmd, p);

                if (n == 2
                    && strncasecmp(setcmd, "CONNECTIONMODE", 14) == 0) {
                    if (strncasecmp(p, "SRCP INFO", 9) == 0) {
                        mode = INFO;
                        rc = SRCP_OK_CONNMODE;
                    }
                    else if (strncasecmp(p, "SRCP COMMAND", 12) == 0) {
                        mode = COMMAND;
                        rc = SRCP_OK_CONNMODE;
                    }
                    else
                        rc = SRCP_HS_WRONGCONNMODE;
                }

                if (nelem == 2 && strncasecmp(setcmd, "PROTOCOL", 3) == 0) {
                    if (strncasecmp(p, "SRCP 0.8", 8) == 0)
                        rc = SRCP_OK_PROTOCOL;
                    else
                        rc = SRCP_HS_WRONGPROTOCOL;
                }
            }
        }

        gettimeofday(&time, NULL);
        srcp_fmt_msg(rc, reply, time);

        if (socket_writereply(Socket, reply) < 0) {
            shutdown(Socket, SHUT_RDWR);
            close(Socket);
            return NULL;
        }
    }
}

/**
 * Core SRCP Commands
 * handle all aspects of the command for all commands
 */

static int handle_setcheck(int sessionid, bus_t bus, char *device,
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
        anzparms =
            sscanf(parameter,
                   "%ld %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld",
                   &laddr, &direction, &speed, &maxspeed, &f[0], &f[1],
                   &f[2], &f[3], &f[4], &f[5], &f[6], &f[7], &f[8], &f[9],
                   &f[10], &f[11], &f[12]);
        for (i = 0; i < anzparms - 4; i++) {
            func += (f[i] ? 1 : 0) << i;
        }
        if (anzparms >= 4) {
            sessionid_t lockid;
            /* Only if not locked or emergency stop !! */
            getlockGL(bus, laddr, &lockid);
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
            /* Port 0,1; Aktion 0,1 */
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

    else if (bus_has_devicegroup(bus, DG_GM)
        && strncasecmp(device, "GM", 2) == 0) {
	
        char  msg[MAXSRCPLINELEN];
        memset(msg, 0, sizeof(msg));
        sscanf(parameter, "%990c",  msg);
        rc = setGM(bus, msg);
    }

    else if (bus_has_devicegroup(bus, DG_SM)
        && strncasecmp(device, "SM", 2) == 0) {
        long addr, value1, value2, value3;
        int type;
        char *ctype;

        ctype = malloc(MAXSRCPLINELEN);
        /* TODO: check malloc returns NULL*/
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
            DBG(bus, DBG_INFO, "LOCK: %s", parameter);
        }
        if (nelem >= 3) {
            rc = SRCP_UNSUPPORTEDDEVICEGROUP;
            if (strncmp(devgrp, "GL", 2) == 0) {
                rc = SRCP_OK;
                if (setorcheck == 1)
                    rc = lockGL(bus, addr, duration, sessionid);
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
int handleSET(int sessionid, bus_t bus, char *device, char *parameter,
              char *reply)
{
    return handle_setcheck(sessionid, bus, device, parameter, reply, 1);
}

/***
 * CHECK -- like SET but no command must be sent
 */
int handleCHECK(int sessionid, bus_t bus, char *device, char *parameter,
                char *reply)
{
    return handle_setcheck(sessionid, bus, device, parameter, reply, 0);
}

/**
 * GET
 */
int handleGET(int sessionid, bus_t bus, char *device, char *parameter,
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
            rc = infoGL(bus, addr, reply);
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
        /* TODO: check malloc returns NULL*/
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

        /* Beschreibungen gibt es deren 2 */
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
                DBG(bus, DBG_INFO, "DESCRIPTION: devgrp=%s addr=%ld",
                    devgrp, addr);
                if (strncmp(devgrp, "GL", 2) == 0)
                    rc = describeGL(bus, addr, reply);
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
int handleWAIT(int sessionid, bus_t bus, char *device, char *parameter,
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
        DBG(bus, DBG_INFO, "wait: %d %d %d", port, waitvalue, timeout);
        if (nelem >= 3) {
            if (getFB(bus, port, &time, &value) == SRCP_OK
                && value == waitvalue) {
                rc = infoFB(bus, port, reply);
            }
            else {
                /* wir warten 1/20 Sekunden genau */
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
                /* es wird nicht gerechnet!, der Zeitflu�ist nicht gleichm�ig! */
                while ((((d * 24 + h) * 60 + m) * 60 + s) >=
                       (((vtime.day * 24 + vtime.hour) * 60 +
                         vtime.min) * 60 + vtime.sec)) {
                    usleep(10000);      /* wir warten 10ms realzeit.. */
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
int handleVERIFY(int sessionid, bus_t bus, char *device,
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
 * negative return code will terminate current session! */
int handleTERM(int sessionid, bus_t bus, char *device, char *parameter,
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
            getlockGL(bus, addr, &lockid);
            if (lockid == 0 || lockid == sessionid) {
                rc = unlockGL(bus, addr, sessionid);
                rc = termGL(bus, addr);
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
                rc = unlockGL(bus, addr, sessionid);
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
            termsession = 0;
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
int handleINIT(int sessionid, bus_t bus, char *device, char *parameter,
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
            rc = initGL(bus, addr, prot, protversion, n_fs, n_func);
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
            rc = initTIME(rx, ry);      /* prft auch die Werte! */
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
int handleRESET(int sessionid, bus_t bus, char *device, char *parameter,
                char *reply)
{
    struct timeval time;
    int rc = SRCP_UNSUPPORTEDOPERATION;

    gettimeofday(&time, NULL);
    srcp_fmt_msg(rc, reply, time);
    return rc;
}



/*
 * Command mode
 */

int doCmdClient(int Socket, sessionid_t sessionid)
{
    char line[MAXSRCPLINELEN], reply[MAXSRCPLINELEN];
    char cbus[MAXSRCPLINELEN], command[MAXSRCPLINELEN],
        devicegroup[MAXSRCPLINELEN], parameter[MAXSRCPLINELEN];
    bus_t bus;
    long int rc, nelem;
    struct timeval akt_time;

    DBG(0, DBG_INFO, "Command mode just starting for session %ld", sessionid);
    while (1) {
        memset(line, 0, sizeof(line));
        if (socket_readline(Socket, line, sizeof(line) - 1) < 0) {
            shutdown(Socket, 0);
            close(Socket);
            return -1;
        }
        memset(command, 0, sizeof(command));
        memset(devicegroup, 0, sizeof(devicegroup));
        memset(parameter, 0, sizeof(parameter));
        memset(reply, 0, sizeof(reply));
        nelem =
            sscanf(line, "%s %s %s %1000c", command, cbus, devicegroup,
                   parameter);
        bus = atoi(cbus);
        reply[0] = 0x00;
        if (nelem >= 3) {
            if ((bus >= 0) && (bus <= num_busses)) {
                rc = SRCP_UNKNOWNCOMMAND;
                if (strncasecmp(command, "SET", 3) == 0) {
                    rc = handleSET(sessionid, bus, devicegroup, parameter,
                                   reply);
                }
                else if (strncasecmp(command, "GET", 3) == 0) {
                    rc = handleGET(sessionid, bus, devicegroup, parameter,
                                   reply);
                }
                else if (strncasecmp(command, "WAIT", 4) == 0) {
                    rc = handleWAIT(sessionid, bus, devicegroup, parameter,
                                    reply);
                }
                else if (strncasecmp(command, "INIT", 4) == 0) {
                    rc = handleINIT(sessionid, bus, devicegroup, parameter,
                                    reply);
                }
                else if (strncasecmp(command, "TERM", 4) == 0) {
                    rc = handleTERM(sessionid, bus, devicegroup, parameter,
                                    reply);
                    if (rc < 0) {
                        if (socket_writereply(Socket, reply) < 0) {
                            break;
                        }
                        break;
                    }
                    rc = abs(rc);
                }
                else if (strncasecmp(command, "VERIFY", 6) == 0) {
                    rc = handleVERIFY(sessionid, bus, devicegroup,
                                      parameter, reply);
                }
                else if (strncasecmp(command, "RESET", 5) == 0) {
                    rc = handleRESET(sessionid, bus, devicegroup,
                                     parameter, reply);
                }
            }
            else {
                rc = SRCP_WRONGVALUE;
                gettimeofday(&akt_time, NULL);
                srcp_fmt_msg(rc, reply, akt_time);
            }
        }
        else {
            DBG(0, DBG_DEBUG, "list too short in session %ld: %d",
                sessionid, nelem);
            rc = SRCP_LISTTOOSHORT;
            gettimeofday(&akt_time, NULL);
            srcp_fmt_msg(rc, reply, akt_time);
        }

        if (socket_writereply(Socket, reply) < 0) {
            break;
        }
    }
    shutdown(Socket, 2);
    close(Socket);
    return 0;
}

