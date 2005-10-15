/* cvs: $Id$             */

/* 
 * Vorliegende Software unterliegt der General Public License, 
 * Version 2, 1991. (c) Matthias Trute, 2000-2001.
 *
 */

#include "stdincludes.h"

#include "threads.h"
#include "config-srcpd.h"
#include "srcp-srv.h"

void change_privileges(int bus)
{
  struct group *group;
  struct passwd *passwd;
  char * grp =  ((SERVER_DATA *) busses[0].driverdata)->groupname;
  char * uid =  ((SERVER_DATA *) busses[0].driverdata)->username;


  if (grp != NULL)
  {
    if ((group = getgrnam(grp)) != NULL ||
        (group = getgrgid((gid_t)atoi(grp))) != NULL)
    {
      if (setegid(group->gr_gid) != 0)
      {
            DBG(0, DBG_WARN, _("could not change to group %s: %s"), group->gr_name, strerror(errno));
      }
      else
      {
            DBG(0, DBG_INFO, _("changed to group %s"), group->gr_name);
      }
    }
    else
    {
          DBG(0,  DBG_WARN, _("could not change to group %s"), grp);
    }
  }

  if (uid != NULL)
  {
    if ((passwd = getpwnam(uid)) != NULL ||
      (passwd = getpwuid((uid_t)atoi(uid))) != NULL)
    {
      if (seteuid(passwd->pw_uid) != 0)
      {
            DBG(0, DBG_INFO, _("could not change to user %s: %s"), passwd->pw_name, strerror(errno));
      }
      else
      {
        DBG(0, DBG_INFO, _("changed to user %s"), passwd->pw_name);
      }
    }
    else
    {
      DBG(0, DBG_INFO, _("could not change to user %s"), uid);
    }
  }
}
int ipv6_supported()
{
#if defined (ENABLE_IPV6)
  int s;

  s = socket (AF_INET6, SOCK_STREAM, 0);
  if (s != -1) {
    (void) close (s);
    return 1;
  }
#endif
  return 0;
}

void* thr_handlePort(void *v)
{
  pthread_t ttid;
  struct _THREADS ti = *((THREADS *)v);
/* ******** */
 int lsock, newsock; /* The listen socket */

#if defined (ENABLE_IPV6)
  struct sockaddr_in6 sin6;
  struct sockaddr_in6 fsin6;
#endif
  struct sockaddr_in sin;
  struct sockaddr_in fsin;
  struct sockaddr *saddr, *fsaddr;
  int socklen, fsocklen;
  int sock_opt;

#ifdef ENABLE_IPV6
  if (ipv6_supported()) { /* Runtime check for IPv6 */
    memset(&sin6, 0, sizeof (sin6));

    sin6.sin6_family = AF_INET6; /* Address family is AF_INET6 */

    sin6.sin6_port = htons(ti.socket);
    sin6.sin6_addr = in6addr_any; /* Specify any address */
                                  /* Addresses of IPv4 nodes would be specified
                                     as IPv4-mapped addresses */

    /* Try to create a socket for listening */
    lsock = socket (AF_INET6, SOCK_STREAM, 0);
    if (lsock == -1) {
       DBG (0, DBG_ERROR, _("Error while creating a socket\n"));
       exit(1);
    }
    saddr = (struct sockaddr *)&sin6; /* This should work since we are assigning the pointer */
    fsaddr = (struct sockaddr *)&fsin6; /* This should work since we are assigning the pointer */
    socklen = sizeof(sin6);
  }
  else
#endif
  {
    /* Here would be the original IPv4 code as usual */
    memset (&sin, 0, sizeof (sin));
    sin.sin_family = AF_INET; /* IPv4 address family */
    sin.sin_port = htons(ti.socket);
    sin.sin_addr.s_addr = INADDR_ANY; 

     /* Create the socket */
    lsock = socket (AF_INET, SOCK_STREAM, 0); /* Create an AF_INET socket */
    if (lsock == -1) {
      DBG(0, DBG_ERROR, _("Error while creating a socket\n"));
      exit(1);
    }
    saddr = (struct sockaddr *)&sin;
    fsaddr = (struct sockaddr *)&fsin;
    socklen = sizeof(sin);
  }
  if(getuid() == 0)
  {
    change_privileges(0);
  }

  sock_opt = 1;
  if (setsockopt (lsock, SOL_SOCKET, SO_REUSEADDR, &sock_opt, sizeof(sock_opt)) == -1) {
    DBG(0, DBG_ERROR, _("Error while setting socket option SO_REUSEADDR\n"));
    close (lsock);
    exit(1);
  }

  /* saddr=(sockaddr_in) if lsock is of type AF_INET else its (sockaddr_in6) */
  if (bind(lsock, (struct sockaddr*)saddr, socklen) == -1) {
    DBG(0, DBG_ERROR, _("Unable to bind\n"));
    close (lsock);
    exit(1);
  }

  if (listen (lsock, 1) == -1) {
    DBG(0, DBG_ERROR, _("Error in listen()\n"));
    close (lsock);
    exit(1);
  }

  /* Wait for a connection request */
  for (;;) {
    fsocklen = socklen;
    newsock = accept(lsock, (struct sockaddr*)fsaddr, &fsocklen);

    if (newsock == -1) {
      /* Possibly the connection got aborted */
      DBG (0, DBG_WARN, _("Unable to accept the new connection\n"));
      continue;
    }

    DBG (0, DBG_INFO, _("Received a new connection\n"));
    /* Now process the connection as per the protocol */
#ifdef ENABLE_IPV6
    if (ipv6_supported()) {
      /* This casting would work since we have take care of the appropriate
       * data structures
       */
      struct sockaddr_in6 *sin6_ptr = (struct sockaddr_in6 *)fsaddr;
      char addrbuf[INET6_ADDRSTRLEN];
      if (IN6_IS_ADDR_V4MAPPED(&(sin6_ptr->sin6_addr))) {
        DBG(0, DBG_INFO, _("Connection from an IPv4 client\n"));
      }

      DBG(0, DBG_INFO, _("Connection from %s/%d\n"),
                     inet_ntop(AF_INET6, (void *)&(sin6_ptr->sin6_addr),
                     addrbuf, sizeof (addrbuf)),
                     ntohs(sin6_ptr->sin6_port));
     } 
     else
#endif
     {
        struct sockaddr_in *sin_ptr = (struct sockaddr_in *)fsaddr;
        DBG (0, DBG_INFO, _("Connection from %s/%d\n"), inet_ntoa(sin_ptr->sin_addr), ntohs(sin_ptr->sin_port));
     }
      sock_opt = 1;
      setsockopt(newsock, SOL_SOCKET, SO_KEEPALIVE, &sock_opt, sizeof(sock_opt));

      if(pthread_create(&ttid, NULL, ti.func, (void*)newsock))
      {
        perror(_("Cannot create thread to handle client. Abort!"));
        exit(1);
      }
      pthread_detach(ttid);
    }
}
