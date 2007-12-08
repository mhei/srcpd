/* cvs: $Id$             */

/*
 * Vorliegende Software unterliegt der General Public License,
 * Version 2, 1991. (c) Matthias Trute, 2000-2001.
 *
 */

#include <arpa/inet.h>
#include <grp.h>
#include <pwd.h>
#include <sys/socket.h>

#include "stdincludes.h"
#include "netservice.h"
#include "clientservice.h"
#include "srcp-server.h"
#include "syslogmessage.h"


void change_privileges(bus_t bus)
{
    struct group *group;
    struct passwd *passwd;
    char *grp = ((SERVER_DATA *) buses[0].driverdata)->groupname;
    char *uid = ((SERVER_DATA *) buses[0].driverdata)->username;


    if (grp != NULL) {
        if ((group = getgrnam(grp)) != NULL ||
            (group = getgrgid((gid_t) atoi(grp))) != NULL) {
            if (setegid(group->gr_gid) != 0) {
                syslog_bus(0, DBG_WARN, "could not change to group %s: %s",
                    group->gr_name, strerror(errno));
            }
            else {
                syslog_bus(0, DBG_INFO, "changed to group %s", group->gr_name);
            }
        }
        else {
            syslog_bus(0, DBG_WARN, "could not change to group %s", grp);
        }
    }

    if (uid != NULL) {
        if ((passwd = getpwnam(uid)) != NULL ||
            (passwd = getpwuid((uid_t) atoi(uid))) != NULL) {
            if (seteuid(passwd->pw_uid) != 0) {
                syslog_bus(0, DBG_INFO, "could not change to user %s: %s",
                    passwd->pw_name, strerror(errno));
            }
            else {
                syslog_bus(0, DBG_INFO, "changed to user %s", passwd->pw_name);
            }
        }
        else {
            syslog_bus(0, DBG_INFO, "could not change to user %s", uid);
        }
    }
}

/*runtime check for ipv6 support */
int ipv6_supported()
{
#if defined (ENABLE_IPV6)
    int s;

    s = socket(AF_INET6, SOCK_STREAM, 0);
    if (s != -1) {
        close(s);
        return 1;
    }
#endif
    return 0;
}

/*cleanup routine for network syn request thread*/
void end_netrequest_thread(net_thread_t *nt_data)
{
    if (nt_data->socket != -1) {
        close(nt_data->socket);
        nt_data->socket = -1;
    }
}

/*handle incoming network syn requests*/
void *thr_handlePort(void *v)
{
    int last_cancel_state, last_cancel_type;
    pthread_t ttid;
    net_thread_t ntd = *((net_thread_t *) v);
    int newsock;
    int result;

    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &last_cancel_state);
    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, &last_cancel_type);

    /*register cleanup routine*/
    pthread_cleanup_push((void *) end_netrequest_thread, (void *) &ntd);

#if defined (ENABLE_IPV6)
    struct sockaddr_in6 sin6;
    struct sockaddr_in6 fsin6;
#endif
    struct sockaddr_in sin;
    struct sockaddr_in fsin;
    struct sockaddr *saddr, *fsaddr;
    socklen_t socklen, fsocklen;
    int sock_opt;

#ifdef ENABLE_IPV6
    if (ipv6_supported()) {
        memset(&sin6, 0, sizeof(sin6));
        sin6.sin6_family = AF_INET6;

        sin6.sin6_port = htons(ntd.port);
        sin6.sin6_addr = in6addr_any;

        /* create a socket for listening */
        ntd.socket = socket(AF_INET6, SOCK_STREAM, 0);
        if (ntd.socket == -1) {
            syslog_bus(0, DBG_ERROR, "Socket creation failed: %s (errno = %d). "
                    "Terminating...\n", strerror(errno), errno);
            exit(1);
        }
        saddr = (struct sockaddr *) &sin6;
        fsaddr = (struct sockaddr *) &fsin6;
        socklen = sizeof(sin6);
    }
    else
#endif
    {
        /* Here would be the original IPv4 code as usual */
        memset(&sin, 0, sizeof(sin));
        sin.sin_family = AF_INET;       /* IPv4 address family */
        sin.sin_port = htons(ntd.port);
        sin.sin_addr.s_addr = INADDR_ANY;

        /* Create the socket */
        ntd.socket = socket(AF_INET, SOCK_STREAM, 0);
        if (ntd.socket == -1) {
            syslog_bus(0, DBG_ERROR, "Socket creation failed: %s (errno = %d). "
                    "Terminating...\n", strerror(errno), errno);
            exit(1);
        }
        saddr = (struct sockaddr *) &sin;
        fsaddr = (struct sockaddr *) &fsin;
        socklen = sizeof(sin);
    }
    if (getuid() == 0) {
        change_privileges(0);
    }

    sock_opt = 1;
    if (setsockopt(ntd.socket, SOL_SOCKET, SO_REUSEADDR, &sock_opt,
         sizeof(sock_opt)) == -1) {
        syslog_bus(0, DBG_ERROR, "Setsockopt failed: %s (errno = %d). "
                "Terminating...\n", strerror(errno), errno);
        close(ntd.socket);
        exit(1);
    }

    /* saddr=(sockaddr_in) if ntd.socket is of type AF_INET else its (sockaddr_in6) */
    if (bind(ntd.socket, (struct sockaddr *) saddr, socklen) == -1) {
        syslog_bus(0, DBG_ERROR, "Bind failed: %s (errno = %d). "
                "Terminating...\n", strerror(errno), errno);
        close(ntd.socket);
        exit(1);
    }

    if (listen(ntd.socket, 1) == -1) {
        syslog_bus(0, DBG_ERROR, "Listen failed: %s (errno = %d). "
                "Terminating...\n", strerror(errno), errno);
        close(ntd.socket);
        exit(1);
    }

    /* Wait for connection requests */
    for (;;) {
        pthread_testcancel();
        fsocklen = socklen;
        newsock = accept(ntd.socket, (struct sockaddr *) fsaddr, &fsocklen);

        if (newsock == -1) {
            /* Possibly the connection got aborted */
            syslog_bus(0, DBG_WARN, "Accept failed: %s (errno = %d)\n",
                    strerror(errno), errno);
            continue;
        }

        syslog_bus(0, DBG_INFO, "New connection received.\n");
        /* Now process the connection as per the protocol */
#ifdef ENABLE_IPV6
        if (ipv6_supported()) {
            /* This casting would work since we have take care of the
             * appropriate data structures
             */
            struct sockaddr_in6 *sin6_ptr = (struct sockaddr_in6 *) fsaddr;
            char addrbuf[INET6_ADDRSTRLEN];

            if (IN6_IS_ADDR_V4MAPPED(&(sin6_ptr->sin6_addr))) {
                syslog_bus(0, DBG_INFO, "Connection from an IPv4 client\n");
            }

            syslog_bus(0, DBG_INFO, "Connection from %s/%d\n",
                inet_ntop(AF_INET6, (void *) &(sin6_ptr->sin6_addr),
                          addrbuf, sizeof(addrbuf)),
                ntohs(sin6_ptr->sin6_port));
        }
        else
#endif
        {
            struct sockaddr_in *sin_ptr = (struct sockaddr_in *) fsaddr;
            syslog_bus(0, DBG_INFO, "Connection from %s/%d\n",
                inet_ntoa(sin_ptr->sin_addr), ntohs(sin_ptr->sin_port));
        }
        sock_opt = 1;
        if (setsockopt(newsock, SOL_SOCKET, SO_KEEPALIVE, &sock_opt,
                    sizeof(sock_opt)) == -1) {
            syslog_bus(0, DBG_ERROR, "Setsockopt failed: %s (errno = %d)\n",
                    strerror(errno), errno);
            close(newsock);
            continue;
        }

        /* hand over client service to "thr_doClient()" from netserver.c */
        result = pthread_create(&ttid, NULL, ntd.client_handler,
                (void *) newsock);
        if (result != 0) {
            syslog_bus(0, DBG_ERROR, "Create thread for network client "
                    "failed: %s (errno = %d). Terminating...\n",
                    strerror(result), result);
            exit(1);
        }
        pthread_detach(ttid);
    }
    /*run the cleanup routine*/
    pthread_cleanup_pop(1);
}
