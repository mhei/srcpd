/***************************************************************************
                          srcpd.c  -  description
                             -------------------
    begin                : Wed Jul 4 2001
    copyright            : (C) 2001,2002 by the srcpd team
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#include "stdincludes.h"

#include "config-srcpd.h"
#include "io.h"
#include "netserver.h"
#include "srcp-descr.h"
#include "srcp-fb.h"
#include "srcp-ga.h"
#include "srcp-gl.h"
#include "srcp-lock.h"
#include "srcp-power.h"
#include "srcp-session.h"
#include "srcp-srv.h"
#include "srcp-time.h"
#include "srcp-info.h"
#include "threads.h"

void hup_handler(int);
void term_handler(int);
void install_signal_handler(void);

extern int server_shutdown_state;
extern int server_reset_state;
extern const char *WELCOME_MSG;

void CreatePIDFile(int pid)
{
  FILE *f;
  f=fopen(((SERVER_DATA *) busses[0].driverdata)->PIDFILE,"wb");
  if (!f)
    syslog(LOG_INFO,"   cannot open %s. Ignoring.", ((SERVER_DATA *) busses[0].driverdata)->PIDFILE);
  else
  {
    fprintf(f,"%d", pid);
    fflush(f);
    fclose(f);
  }
}

void DeletePIDFile()
{
  unlink(((SERVER_DATA *) busses[0].driverdata)->PIDFILE);
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
  int sleep_ctr;
  pid_t pid;
  char c, conffile[MAXPATHLEN];
  pthread_t ttid_cmd, ttid_clock, ttid_pid;
  struct _THREADS cmds;
  install_signal_handler();

  sprintf(conffile, "%s/srcpd.conf", PREFIX);

  /* Parameter auswerten */
  opterr=0;
  while((c=getopt(argc, argv, "f:hv")) != EOF)
  {
    switch(c)
    {
      case 'f':
        if(strlen(optarg)<MAXPATHLEN-1)
		strcpy(conffile, optarg);
        break;
      case 'v':
        printf("srcpd version 2.0, speaks SRCP between 0.7 and 0.8, do not use!\n");
        exit(1);
        break;
      case 'h':
        printf("srcpd -f <conffile> -v -h\n");
        printf("v           -  prints program version and exits\n");
        printf("f            -  use another config file (default %s)\n", conffile);
        printf("h           -  prints this text and exits\n");
        exit(1);
        break;
      default:
        printf("unknown Parameter\n");
        printf("use: \"srcpd -h\" for help, exiting\n");
        exit(1);
        break;
    }
  }

  openlog("srcpd", LOG_CONS, LOG_USER);
  syslog(LOG_INFO, "%s", WELCOME_MSG);

  startup_GL();
  startup_GA();
  startup_FB();

    // zuerst die Konfigurationsdatei lesen
  printf("conffile = \"%s\"\n", conffile);

  readConfig(conffile);
  // check for resolv all needed malloc's
  for(i=0;i<MAX_BUSSES;i++)
  {
    if (busses[i].number > -1)
    {
      if (busses[i].driverdata == NULL)
      {
        printf("Sorry, there is an error in your srcpd.conf for bus %i !\n", i);
        exit(1);
      }
    }
  }
  cmds.socket = ((SERVER_DATA *) busses[0].driverdata)->TCPPORT;
  cmds.func = thr_doClient;

  /* Now we have to initialize all busses */
  /* this function should open the device */
  for(i=0; i<=num_busses; i++)
  {
    if(busses[i].init_func)
    {
      if ((*busses[i].init_func)(i) != 0)
        exit(1);           // error while initialize
    }
  }

  // a little help for debugging the threads
  if (busses[0].debuglevel < DBG_DEBUG)
  {
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
  }

  CreatePIDFile(getpid());
  /* First: Init the device data used internally*/
  startup_INFO();
  startup_LOCK();
  startup_DESCRIPTION();
  startup_TIME();
  startup_SERVER();
  startup_SESSION();

  /* Netzwerkverbindungen */
  error = pthread_create(&ttid_cmd, NULL, thr_handlePort, &cmds);
  if(error)
  {
    syslog(LOG_INFO, "cannot start Command Thread #%d: %d", i, error);
    exit(1);
  }
  pthread_detach(ttid_cmd);
  
  /* Modellzeitgeber starten, der ist aber zunächst idle */
  error = pthread_create(&ttid_clock, NULL, thr_clock, NULL);
  if(error)
  {
    syslog(LOG_INFO, "cannot start Clock Thread!");
  }
  pthread_detach(ttid_clock);
  /* und jetzt die Bustreiber selbst starten. Das Device ist offen, die Datenstrukturen
     initialisiert */
  syslog(LOG_INFO, "Going to start %d Interface Threads for the busses", num_busses);
  /* Jetzt die Threads für die Busse */
  for (i=1; i<=num_busses; i++)
  {
    syslog(LOG_INFO, "going to start Interface Thread  %d type(%d)", i, busses[i].type);
    error = pthread_create(&ttid_pid, NULL, busses[i].thr_func, (void *)i);
    if(error)
    {
      syslog(LOG_INFO, "cannot start Interface Thread # %d", i);
      exit(1);
    }
    pthread_detach(ttid_pid);
    busses[i].pid = ttid_pid;
    syslog(LOG_INFO, "Interface Thread %d started successfully type(%d): pid %ld", i, busses[i].type, (long)(busses[i].pid));
    if (( (busses[i].flags & AUTO_POWER_ON) == AUTO_POWER_ON) ) {
      setPower(i, 1, "AUTO POWER ON");
    } else {
      setPower(i, 0, "AUTO POWER OFF");
    }
  }
  syslog(LOG_INFO, "All Threads started");
  server_shutdown_state = 0;
  sleep_ctr = 10;
  /* And now: Wait for _real_ tasks: shutdown, reset, watch for hanging processes */
  while(1)
  {
    if(server_shutdown_state == 1)
    {
      sleep(2); /* Protokollforderung */
      break; /* leave the while() loop */
    }
    usleep(100000);
  
    /* test for feedbacks changed back to "0" */
    check_reset_fb();

    sleep_ctr--;
    if (sleep_ctr == 0)
    {
       /* LOCKs aufraeumen */
       unlock_gl_bytime();
       unlock_ga_bytime();
      /* Jetzt Wachhund spielen, falls gewünscht */
      for(i=1; i<=num_busses; i++)
      {
        if(busses[i].watchdog == 0 && (busses[i].flags & USE_WATCHDOG))
        {
          syslog(LOG_INFO, "Oops: Interface Thread %d hangs, restarting it: (old pid: %ld, %d)", i, (long) busses[i].pid, busses[i].watchdog);
          pthread_cancel(busses[i].pid);
          waitpid((long) busses[i].pid, NULL, 0);
          error = pthread_create(&ttid_pid, NULL, busses[i].thr_func, (void *)i);
          if(error)
          {
            perror("cannot restart Interface Thread!");
            break; /* ermöglicht aufräumen am Ende */
          }
          busses[i].pid = ttid_pid;
          pthread_detach(busses[i].pid);
        }
        busses[i].watchdog = 0;
      }
      sleep_ctr = 10;
    }
  }
  syslog(LOG_INFO, "Shutting down server...");
  /* hierher kommen wir nur nach einem break */
  pthread_cancel(ttid_cmd);
  pthread_cancel(ttid_clock);
  /* und jetzt die ganzen Busse */
  for(i=1; i<=num_busses;i++)
  {
    pthread_cancel(busses[i].pid);
  }
  DeletePIDFile();
  for(i=num_busses; i>=0; i--) // der Server als letzter
  {
    (*busses[i].term_func)(i);
  }
  wait(0);
  syslog(LOG_INFO, "und tschüß.. ;=)");
  exit(0);
}
