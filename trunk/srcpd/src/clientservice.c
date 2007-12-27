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

#include "clientservice.h"
#include "io.h"
#include "srcp-error.h"
#include "srcp-command.h"
#include "srcp-info.h"
#include "srcp-server.h"
#include "syslogmessage.h"
#include "queue.h"

#include <errno.h>
#include <sys/socket.h>


/* SRCP server welcome message */
const char *WELCOME_MSG =
    "srcpd V" VERSION "; SRCP 0.8.3; SRCPOTHER 0.8.4-wip\n";


/* Cleanup routine for network client thread. Info threads must release
 * the queue mutex when they are cancelled in pthread_cond_wait(), not
 * to block themselves trying to queue their termination message.
 */
void end_client_thread(session_node_t *sn)
{
    int result;
    sessionid_t session;

    session = sn->session;
    syslog_session(session, DBG_INFO,
            "Session entered cancel state (mode = %d).", sn->mode);

    if (sn->mode == smInfo) {
        result = pthread_mutex_unlock(&sn->queue_mutex);
        if (result != 0) {
            syslog_session(session, DBG_ERROR,
                    "pthread_mutex_unlock() failed: %s (errno = %d).",
                    strerror(result), result);
        }
    }

    /*prevent receiving new messages*/
    sn->mode = smUndefined;
    
    if (sn->socket != -1)
        close(sn->socket);
    
    result = pthread_cond_destroy(&sn->queue_cond);
    if (result != 0) {
        syslog_session(session, DBG_WARN,
                "pthread_cond_destroy() failed: %s (errno = %d).",
                strerror(result), result);
    }
    
    result = pthread_mutex_destroy(&sn->queue_mutex);
    if (result != 0) {
        syslog_session(session, DBG_WARN,
                "pthread_mutex_destroy() failed: %s (errno = %d).",
                strerror(result), result);
    }

    clear_queue(&sn->queue);
    destroy_session(session);

    /*at last tell all remaining sessions about this one to leave*/
    stop_session(session);
    syslog_session(session, DBG_INFO, "Session sucessfully cancelled.");
}

/* handle connected SRCP clients, start with shake hand phase. */
void* thr_doClient(void* v)
{
    char line[MAXSRCPLINELEN], cmd[MAXSRCPLINELEN],
        parameter[MAXSRCPLINELEN], reply[MAXSRCPLINELEN];
    int rc, nelem;
    struct timeval time;
    int last_cancel_state, last_cancel_type;
    int result;
    ssize_t sresult;

    session_node_t* sn = (session_node_t*) v;
    sn->thread = pthread_self();

    result = pthread_detach(sn->thread);
    if (result != 0) {
        syslog_session(sn->session, DBG_WARN,
                "pthread_detach() failed: %s (errno = %d).",
                strerror(result), result);
    }

    sn->mode = smCommand;
    initialize_queue(&sn->queue);

    result = pthread_mutex_init(&sn->queue_mutex, NULL);
    if (result != 0) {
        syslog_session(sn->session, DBG_WARN,
                "pthread_mutex_init() failed: %s (errno = %d).",
                strerror(result), result);
    }
    result = pthread_cond_init(&sn->queue_cond, NULL);
    if (result != 0) {
        syslog_session(sn->session, DBG_WARN,
                "pthread_cond_init() failed: %s (errno = %d).",
                strerror(result), result);
    }
    result = pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &last_cancel_state);
    if (result != 0) {
        syslog_session(sn->session, DBG_WARN,
                "pthread_setcancelstate() failed: %s (errno = %d).",
                strerror(result), result);
    }
    result = pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, &last_cancel_type);
    if (result != 0) {
        syslog_session(sn->session, DBG_WARN,
                "pthread_setcanceltype() failed: %s (errno = %d).",
                strerror(result), result);
    }

    /*register cleanup routine*/
    pthread_cleanup_push((void *) end_client_thread, (void *) sn);

    sresult = writen_amlb(sn->socket, WELCOME_MSG);
    if (-1 == sresult) {
        syslog_session(sn->session, DBG_ERROR,
                "Socket write failed: %s (errno = %d)\n",
                strerror(errno), errno);
        pthread_exit((void*) 1);
    }
    else if (0 == sresult) {
        syslog_session(sn->session, DBG_WARN,
                "Socket write failed (connection terminated by client).\n");
        shutdown(sn->socket, SHUT_RDWR);
        pthread_exit((void*) 1);
    }

    while (1) {
        pthread_testcancel();
        rc = SRCP_HS_NODATA;
        reply[0] = 0x00;
        memset(line, 0, sizeof(line));

        sresult = socket_readline(sn->socket, line, sizeof(line) - 1);

        /* client terminated connection */
        /* TODO:
        if (0 == sresult) {
            shutdown(sn->socket, SHUT_RDWR);
            break;
        }*/

        /* read errror */
        /*else*/ if (-1 == sresult) {
            syslog_session(sn->session, DBG_ERROR,
                    "Socket read failed: %s (errno = %d)\n",
                    strerror(errno), errno);
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

                sresult = writen_amlb(sn->socket, reply);
                if (-1 == sresult) {
                    syslog_session(sn->session, DBG_ERROR,
                            "Socket write failed: %s (errno = %d)\n",
                            strerror(errno), errno);
                    pthread_exit((void*) 1);
                }
                else if (0 == sresult) {
                    syslog_session(sn->session, DBG_WARN,
                            "Socket write failed (connection terminated "
                            "by client).\n");
                    shutdown(sn->socket, SHUT_RDWR);
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
                pthread_exit((void*) 0);
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

        sresult = writen_amlb(sn->socket, reply);
        if (-1 == sresult) {
            syslog_session(sn->session, DBG_ERROR,
                    "Socket write failed: %s (errno = %d)\n",
                    strerror(errno), errno);
            break;
        }
        else if (0 == sresult) {
            syslog_session(sn->session, DBG_WARN,
                    "Socket write failed (connection terminated "
                    "by client).\n");
            shutdown(sn->socket, SHUT_RDWR);
            break;
        }
    }

    /*run the cleanup routine*/
    pthread_cleanup_pop(1);
    return NULL;
}

