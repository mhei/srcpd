/***************************************************************************
                          srcpd.c  -  description
                             -------------------
    begin                : Wed Jul 4 2001
    copyright            : (C) 2001,2002 by the srcpd team
    $Id$
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <unistd.h>

#include "config-srcpd.h"
#include "io.h"

#include "ib.h"
#include "m605x.h"
#include "loopback.h"

#include "netserver.h"
#include "srcp-fb.h"
#include "srcp-fb-i8255.h"
#include "srcp-ga.h"
#include "srcp-gl.h"
#include "srcp-power.h"
#include "srcp-time.h"
#include "threads.h"

void hup_handler(int);
void term_handler(int);
void install_signal_handler(void);

extern int TCPPORT;

extern int server_shutdown_state;
extern int server_reset_state;
extern const char *WELCOME_MSG;

void CreatePIDFile(int pid) {
    FILE *f;
    f=fopen(PIDFILE,"wb");
    if (!f)  
       syslog(LOG_INFO,"   cannot open %s. Ignoring.", PIDFILE); 
    else {
       fprintf(f,"%d", pid);
       fflush(f);
       fclose(f);
    }
}

void DeletePIDFile() {
    unlink(PIDFILE);
}

void hup_handler(int s)
{
  /* signal SIGHUP(1) caught */
    server_reset_state=1;
    syslog(LOG_INFO,"SIGHUP(1) received! Reset done. Working in initial state.");

    signal(s, hup_handler);
    
    return;
}

void term_handler(int s)
{
  // signal SIGTERM(15) caught
  syslog(LOG_INFO,"SIGTERM(15) received! Terminating ...");
  server_shutdown_state = 1;
  DeletePIDFile();
}

void install_signal_handler()
{
  signal(SIGTERM, term_handler);
  signal(SIGABRT, term_handler);
  signal(SIGINT, term_handler);
  signal(SIGHUP, hup_handler);
  signal(SIGPIPE, SIG_IGN);    /* important, because write() on sockets */
                               /* should return errors                */
}


int main(int argc, char **argv)
{
  int error, i;
  pid_t pid;
  char c;
  pthread_t ttid_cmd, ttid_clock, ttid_pid;
  struct _THREADS cmds = {TCPPORT,      thr_doClient};

  install_signal_handler();

  // zuerst die Konfigurationsdatei lesen
  readConfig();

  cmds.socket = TCPPORT;

  /* Parameter auswerten */
  opterr=0;
  while((c=getopt(argc, argv, "p:htv")) != EOF)
  {
    switch(c)
    {
      case 'p':
        cmds.socket = atoi(optarg);
        break;
      case 'v':
        printf("srcpd version 2.0, speaks SRCP between 0.7 and 0.8, do not use!\n");
	exit(1);
        break;
      case 'h':
        printf("srcpd -p <Portnumber> -t -v\n");
        printf("Portnumber  -  first Communicationport via TCP/IP (default: 12345)\n");
        printf("t           -  start server in testmode (without connection to real interface)\n");
	printf("v           -  prints program version and exits\n");
        exit(1);
        break;
      default:
	printf("unknown Parameter\n");
	printf("use: \"srcpd -h\" for help\n");
	exit(1);
	break;
    }
  }

  /* forken */
  if((pid=fork())<0)
  {
    return -1;
  }
  else
  {
    if(pid != 0 )
    {
      exit(0);
    }
  }
  // ab hier keine Konsole mehr... Wir sind ein Dämon geworden!
  chdir("/");

  openlog("srcpd", LOG_CONS, LOG_USER);
  syslog(LOG_INFO, "%s", WELCOME_MSG);

  CreatePIDFile(getpid());

  /* Now we have to initialize all busses */
  for(i=1; i<=num_busses; i++) {
      (*busses[i].init_func)(i);
  }
  /* die Threads starten */
  error = pthread_create(&ttid_cmd, NULL, thr_handlePort, &cmds);
  if(error)
  {
    syslog(LOG_INFO, "cannot start Command Thread #%d: %d", i, error);
    exit(1);
  }
  pthread_detach(ttid_cmd);

  error = pthread_create(&ttid_clock, NULL, thr_clock, NULL);
  if(error)
  {
    syslog(LOG_INFO, "cannot start Clock Thread!");
  }
  pthread_detach(ttid_clock);

  syslog(LOG_INFO, "Going to start %d Interface Threads for the busses", num_busses);
  /* Jetzt die Threads für die Busse */
  for (i=1; i<=num_busses; i++) {
      syslog(LOG_INFO, "going to start Interface Thread  %d type(%d)", i, busses[i].type);
      error = pthread_create(&ttid_pid, NULL, busses[i].thr_func, (void *)i);
      if(error)
      { 
        syslog(LOG_INFO, "cannot start Interface Thread # %d", i);
        exit(1);
      }
      pthread_detach(ttid_pid);
      busses[i].pid = ttid_pid;
      syslog(LOG_INFO, "Interface Thread %d started successfully type(%d): pid %d", i, busses[i].type, busses[i].pid);
  }
  syslog(LOG_INFO, "All Threads started");
  server_shutdown_state = 0;

  while(1)  {
    if(server_shutdown_state == 1) {
      sleep(2); /* Protokollforderung */
      pthread_cancel(ttid_cmd);
      pthread_cancel(ttid_clock);
      /* und jetzt die ganzen Busse */
      for(i=1; i<=num_busses;i++) {
        pthread_cancel(busses[i].pid);
      }
      break;
    }
    sleep(1);
    /* Jetzt Wachhund spielen, falls gewünscht */
    for(i=1; i<=num_busses; i++) {
      if(busses[i].watchdog == 0 && (busses[i].flags && USE_WATCHDOG))  {
	  syslog(LOG_INFO, "Oops: Interface Thread %d hangs, restarting it: (old pid: %d, %d)", i, busses[i].pid, busses[i].watchdog);
	  pthread_cancel(busses[i].pid);
	  sleep(1);
	  error = pthread_create(&ttid_pid, NULL, busses[i].thr_func, (void *)i);
	  if(error)
	      {
		  perror("cannot restart Interface Thread!");
		  exit(1);
	      }
	  busses[i].pid = ttid_pid;
	  pthread_detach(busses[i].pid);
      }
      busses[i].watchdog = 0;
    }
  }
  syslog(LOG_INFO, "Shutting down server...");
  /* hierher kommen wir nur nach einem break */
  for(i=1; i<=num_busses; i++) {
      (*busses[i].term_func)(i);
  }
  DeletePIDFile();
  syslog(LOG_INFO, "und tschüß.. ;=)");
  exit(0);
}
