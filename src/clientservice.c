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

/* SRCP server welcome message */
const char *WELCOME_MSG =
    "srcpd V" VERSION "; SRCP 0.8.3; SRCPOTHER 0.8.4-wip\n";


/* Cleanup routine for network client thread. Info threads must release
 * the queue mutex when they are cancelled in pthread_cond_wait(), not
 * to block themselves trying to queue their termination message.
 */
void end_client_thread(session_node_t *sn)
{
    syslog_session(sn->session, DBG_INFO, "Session cancelled (mode = %d).",
            sn->mode);

    if (sn->mode == smInfo)
        unlock_info_queue_mutex();
    stop_session(sn->session);
    if (sn->socket != -1)
        close(sn->socket);

    destroy_session(sn->session);
}

/* handle connected SRCP clients, start with shake hand phase. */
void* thr_doClient(void* v)
{
    session_node_t* sn = (session_node_t*) v;
    sn->thread = pthread_self();
    pthread_detach(sn->thread);
    sn->mode = smCommand;

    char line[MAXSRCPLINELEN], cmd[MAXSRCPLINELEN],
        parameter[MAXSRCPLINELEN], reply[MAXSRCPLINELEN];
    int rc, nelem;
    struct timeval time;
    int last_cancel_state, last_cancel_type;

    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &last_cancel_state);
    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, &last_cancel_type);

    /*register cleanup routine*/
    pthread_cleanup_push((void *) end_client_thread, (void *) sn);

    if (socket_writereply(sn->socket, WELCOME_MSG) < 0) {
        pthread_exit((void*) 1);
    }

    while (1) {
        pthread_testcancel();
        rc = SRCP_HS_NODATA;
        reply[0] = 0x00;
        memset(line, 0, sizeof(line));

        if (socket_readline(sn->socket, line, sizeof(line) - 1) < 0) {
            break;
        }

        memset(cmd, 0, sizeof(cmd));
        memset(parameter, 0, sizeof(parameter));
        nelem = sscanf(line, "%s %1000c", cmd, parameter);

        if (nelem > 0) {
            rc = SRCP_UNKNOWNCOMMAND;

            if (strncasecmp(cmd, "GO", 2) == 0) {
                /*get a session-id for this no longer anonymous session*/
                register_session(sn);

                gettimeofday(&time, NULL);
                sprintf(reply, "%lu.%.3lu 200 OK GO %ld\n", time.tv_sec,
                        time.tv_usec / 1000, sn->session);
                if (socket_writereply(sn->socket, reply) < 0) {
                    pthread_exit((void*) 1);
                }
                start_session(sn);

                switch (sn->mode) {
                    case smCommand:
                        rc = doCmdClient(sn);
                        break;
                    case smInfo:
                        rc = doInfoClient(sn);
                        break;
                }
                /*exit while loop*/
                break;
            }

            else if (strncasecmp(cmd, "SET", 3) == 0) {
                char p[MAXSRCPLINELEN], setcmd[MAXSRCPLINELEN];
                int n = sscanf(parameter, "%s %1000c", setcmd, p);

                if (n == 2
                    && strncasecmp(setcmd, "CONNECTIONMODE", 14) == 0) {
                    if (strncasecmp(p, "SRCP INFO", 9) == 0) {
                        sn->mode = smInfo;
                        rc = SRCP_OK_CONNMODE;
                    }
                    else if (strncasecmp(p, "SRCP COMMAND", 12) == 0) {
                        sn->mode = smCommand;
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

        if (socket_writereply(sn->socket, reply) < 0) {
            break;
        }
    }

    /*run the cleanup routine*/
    pthread_cleanup_pop(1);
    return NULL;
}

