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
 *            - cacheGetLockGL changed to getlockGA in the GA-command-processing
 *            - if the device is locked, return code is now set to
 *              SRCP_DEVICELOCKED
 */

#include "stdincludes.h"

#include "clientservice.h"
#include "io.h"
#include "srcp-error.h"
#include "srcp-command.h"
#include "srcp-info.h"
#include "srcp-server.h"
#include "syslogmessage.h"

#define COMMAND 1
#define INFO    2

/* SRCP server welcome message */
const char *WELCOME_MSG =
    "srcpd V" VERSION "; SRCP 0.8.3; SRCPOTHER 0.8.4-wip\n";


/* Cleanup routine for network client thread. Info threads must release
 * the queue mutex when they are cancelled in pthread_cond_wait(), not
 * to block themselves trying to queue their termination message.
 */
void end_client_thread(client_thread_t *ctd)
{
    if (ctd->session != 0) {
        if (ctd->mode == INFO)
            unlock_info_queue_mutex();
        stop_session(ctd->session);
        session_destroy(ctd->session);
    }

    if (ctd->socket != -1) {
        close(ctd->socket);
    }

    free(ctd);
}

/* handle connected SRCP clients, start with shake hand phase. */
void* thr_doClient(void *v)
{
    client_thread_t* ctd = (client_thread_t*) malloc(sizeof(client_thread_t));
    if (ctd == NULL)
        return NULL;
    ctd->socket = (int) v;
    ctd->mode = COMMAND;
    ctd->session = 0;

    char line[MAXSRCPLINELEN], cmd[MAXSRCPLINELEN],
        parameter[MAXSRCPLINELEN], reply[MAXSRCPLINELEN];
    int rc, nelem;
    struct timeval time;
    int last_cancel_state, last_cancel_type;

    /* drop root permission for this thread (doubled)*/
    /*change_privileges(0);*/

    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &last_cancel_state);
    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, &last_cancel_type);

    /*register cleanup routine*/
    pthread_cleanup_push((void *) end_client_thread, (void *) ctd);

    if (socket_writereply(ctd->socket, WELCOME_MSG) < 0) {
        pthread_exit((void*) 1);
    }

    while (1) {
        pthread_testcancel();
        rc = SRCP_HS_NODATA;
        reply[0] = 0x00;
        memset(line, 0, sizeof(line));

        if (socket_readline(ctd->socket, line, sizeof(line) - 1) < 0) {
            break;
        }

        memset(cmd, 0, sizeof(cmd));
        memset(parameter, 0, sizeof(parameter));
        nelem = sscanf(line, "%s %1000c", cmd, parameter);

        if (nelem > 0) {
            rc = SRCP_UNKNOWNCOMMAND;

            if (strncasecmp(cmd, "GO", 2) == 0) {
                ctd->session = session_create(pthread_self());
                if (0 == ctd->session) {
                    syslog_bus(0, DBG_ERROR, "Session create failed!");
                    pthread_exit((void*) 1);
                }

                gettimeofday(&time, NULL);
                sprintf(reply, "%lu.%.3lu 200 OK GO %ld\n", time.tv_sec,
                        time.tv_usec / 1000, ctd->session);
                if (socket_writereply(ctd->socket, reply) < 0) {
                    pthread_exit((void*) 1);
                }
                start_session(ctd->session, ctd->mode);
                switch (ctd->mode) {
                    case COMMAND:
                        rc = doCmdClient(ctd);
                        break;
                    case INFO:
                        rc = doInfoClient(ctd);
                        break;
                }
                pthread_exit((void*) 0);
            }

            else if (strncasecmp(cmd, "SET", 3) == 0) {
                char p[MAXSRCPLINELEN], setcmd[MAXSRCPLINELEN];
                int n = sscanf(parameter, "%s %1000c", setcmd, p);

                if (n == 2
                    && strncasecmp(setcmd, "CONNECTIONMODE", 14) == 0) {
                    if (strncasecmp(p, "SRCP INFO", 9) == 0) {
                        ctd->mode = INFO;
                        rc = SRCP_OK_CONNMODE;
                    }
                    else if (strncasecmp(p, "SRCP COMMAND", 12) == 0) {
                        ctd->mode = COMMAND;
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

        if (socket_writereply(ctd->socket, reply) < 0) {
            break;
        }
    }

    /*run the cleanup routine*/
    pthread_cleanup_pop(1);
    pthread_exit(NULL);
}
