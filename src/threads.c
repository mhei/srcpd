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
            DBG(0, DBG_WARN, "could not change to group %s: %s",group->gr_name,strerror(errno));
      }
      else
      {
            DBG(0, DBG_INFO, "changed to group %s",group->gr_name);
      }
    }
    else
    {
          DBG(0,  DBG_WARN, "could not change to group %s: invalid group or ENOMEM",grp);
    }
  }

  if (uid != NULL)
  {
    if ((passwd = getpwnam(uid)) != NULL ||
      (passwd = getpwuid((uid_t)atoi(uid))) != NULL)
    {
      if (seteuid(passwd->pw_uid) != 0)
      {
            DBG(0, DBG_WARN,"could not change to user %s: %s",passwd->pw_name,strerror(errno));
      }
      else
      {
        DBG(0, DBG_INFO, "changed to user %s",passwd->pw_name);
      }
    }
    else
    {
      DBG(0, DBG_WARN,"could not change to user %s: invalid user or ENOMEM", uid);
    }
  }
}

/*******************************************************************
 * Die Metathread, die an den tcp/ip Ports lauschen und
 * die dann einen eigen Thread für jeden Request starten
 *******************************************************************
 *
 * Ein Thread hat einen Port und eine zugehörige Funktion, die
 * bei Aktivieren startet.
 */
void*
thr_handlePort(void *v)
{
  pthread_t ttid;
  int       error, val;
  int       sckt;
  int        boundSocket;
  struct     sockaddr_in socketAddr;
  int        addrlen=0;
  struct _THREADS ti = *((THREADS *)v);

  if((boundSocket = socket(AF_INET, SOCK_STREAM, 0 )) == -1)
  {
    perror("socket()");
    exit(1);
  }
  
  val = 1;
  setsockopt(boundSocket, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));


  bzero(&socketAddr,sizeof(socketAddr));
  socketAddr.sin_family = AF_INET;
  if(((SERVER_DATA *) busses[0].driverdata)->listenip == NULL){
	socketAddr.sin_addr.s_addr = htonl(INADDR_ANY);
  }else	{
	socketAddr.sin_addr.s_addr = inet_addr(((SERVER_DATA *) busses[0].driverdata)->listenip);
  }

  socketAddr.sin_port = htons(ti.socket);

  if(bind(boundSocket, (struct sockaddr *) &socketAddr, sizeof(socketAddr)) < 0)
  {
    perror("bind()");
    exit(1);
  }
  if(listen(boundSocket, 10)  == -1)
  {
    perror("listen()");
    exit(1);
  }

  if(getuid() == 0)
  {
    change_privileges(0);
  }

  while (1)
  {
    if((sckt=accept(boundSocket,(struct sockaddr*)&socketAddr,&addrlen)) < 0)
    {
      perror("accept()\n");
    }
    if(sckt)
    {
      val = 1;
      setsockopt(sckt, SOL_SOCKET, SO_KEEPALIVE, &val, sizeof(val));

      if(busses[0].debuglevel)
        DBG(0, DBG_INFO, "New Client at Port %d from %s:%d", ti.socket,
     inet_ntoa(socketAddr.sin_addr), ntohs(socketAddr.sin_port));
     error = pthread_create(&ttid, NULL, ti.func, (void*)sckt);
      if(error)
      {
        perror("cannot create thread to handle client. Abort!");
        exit(1);
      }
      pthread_detach(ttid);
    }
  }
}
