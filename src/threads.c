/* cvs: $Id$             */

/* 
 * Vorliegende Software unterliegt der General Public License, 
 * Version 2, 1991. (c) Matthias Trute, 2000-2001.
 *
 */


#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <signal.h>
#include <syslog.h>

#include "threads.h"

/*******************************************************************
 * Die Metathread, die an den tcp/ip Ports lauschen und
 * die dann einen eigen Thread für jeden Request starten
 *******************************************************************
 *
 * Ein Thread hat einen Port und eine zugehörige Funktion, die
 * bei Aktivieren startet.
 */
void* thr_handlePort(void *v)
{
  pthread_t ttid;
  int       error;
  int       sckt, val;
  int        boundSocket;
  struct     sockaddr_in socketAddr;
  int        addrlen=sizeof(socketAddr);
  struct _THREADS ti = *((THREADS *)v);

  syslog(LOG_INFO,"tcp-port: going to start");

  if((boundSocket = socket(AF_INET, SOCK_STREAM, 0 )) == -1)
  {
    syslog(LOG_INFO,"error: socket()");
    perror("socket()");
    exit(1);
  }
  
  val = 1;
  setsockopt(boundSocket, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
  
  memset(&socketAddr, 0, sizeof(socketAddr));
  socketAddr.sin_family = AF_INET;
  socketAddr.sin_addr.s_addr = INADDR_ANY;
  socketAddr.sin_port = htons(ti.socket);

  if(bind(boundSocket, (struct sockaddr *) &socketAddr, sizeof(socketAddr)) < 0)
  {
    syslog(LOG_INFO,"error: bind() %d", htons(ti.socket));
    perror("bind()");
    exit(1);
  }
  if(listen(boundSocket, 10)  == -1)
  {
    syslog(LOG_INFO,"listen() %d", htons(ti.socket));
    perror("listen()");
    exit(1);
  }

  while (1)
  {
    if((sckt=accept(boundSocket,(struct sockaddr*)&socketAddr,&addrlen)) < 0)
    {
      perror("accept()\n");
      syslog(LOG_INFO,"error: accept() %d", htons(ti.socket));

    }
    if(sckt)
    {
      syslog(LOG_INFO, "New Client: IP: %s, port %hd", inet_ntoa(socketAddr.sin_addr), ntohs(socketAddr.sin_port));
      error = pthread_create(&ttid, NULL, ti.func, (void*)sckt);
      if(error)
      {
        perror("cannot create thread to handle client. Abort!");
	syslog(LOG_INFO,"thread() %d", htons(ti.socket));
      }
      pthread_detach(ttid);
    }
  }
}
