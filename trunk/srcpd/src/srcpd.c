/***************************************************************************
                          srcpd.c  -  description
                             -------------------
    begin                : Wed Jul 4 2001
    copyright            : (C) 2001 by Dipl.-Ing. Frank Schmischke
    email                : frank.schmischke@t-online.de
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
#include "ib.h"
#include "io.h"
#include "m605x.h"
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

int file_descriptor[NUMBER_SERVERS];

extern int CMDPORT;
extern int FEEDBACKPORT;
extern int INFOPORT;
extern int M6020MODE;
extern int NUMBER_FB;

extern int server_shutdown_state;
extern int server_reset_state;
extern int io_thread_running;
extern int working_server;
extern int use_i8255;
extern int use_watchdog;
extern int restore_com_parms;
#ifdef TESTMODE
extern int testmode;
#endif

extern char *DEV_COMPORT;
extern char *DEV_I8255;
extern const char *WELCOME_MSG;

void hup_handler(int s)
{
  /* signal SIGHUP(1) caught */

  syslog(LOG_INFO,"SIGHUP(1) received! Reset done. Working in initial state.");

  signal(s, hup_handler);

  return;
}

void term_handler(int s)
{
  // signal SIGTERM(15) caught
  syslog(LOG_INFO,"SIGTERM(15) received! Terminating ...");
  server_shutdown_state = 1;
}

void install_signal_handler()
{
  if(restore_com_parms) {
    signal(SIGTERM, term_handler);
    signal(SIGABRT, term_handler);
    signal(SIGINT, term_handler);
    signal(SIGHUP, hup_handler);
  }
  signal(SIGPIPE, SIG_IGN);    /* important, because write() on sockets */
                                        /* should return errors                */
}

int main(int argc, char **argv)
{
  int error, tmp_state;
  pid_t pid;
  char c;
  pthread_t ttid_iface, ttid_cmd, ttid_fb, ttid_info, ttid_clock, ttid_i8255;
  struct _THREADS cmds = {CMDPORT,      thr_doCmdClient};
  struct _THREADS fbs  = {FEEDBACKPORT, thr_doFBClient};
  struct _THREADS infos= {INFOPORT,     thr_doInfoClient};

  void* server_fkt[NUMBER_SERVERS] =
  {
    NULL,
    thr_sendrec6051,
    thr_sendrecintellibox,
    NULL
  };

  install_signal_handler();

  // zuerst die Konfigurationsdatei lesen
  readConfig();

  /* Parameter auswerten */
  opterr=0;
#ifndef TESTMODE
  while((c=getopt(argc, argv, "f:p:d:hoS:")) != EOF)
#else
  while((c=getopt(argc, argv, "f:p:d:hotS:")) != EOF)
#endif
  {
    switch(c)
    {
      case 'p':
        cmds.socket = atoi(optarg);
        fbs.socket  = cmds.socket+1;
        infos.socket= cmds.socket+2;
        break;
      case 'd':
        DEV_COMPORT = optarg;
        break;
      case 'S':
        DEV_I8255 = optarg;
        break;
      case 'o':
        M6020MODE = 1;
        break;
      case 'f':
        NUMBER_FB = atoi(optarg);
        break;
#ifdef TESTMODE
      case 't':
        testmode = 1;
        break;
#endif
      case 'h':
#ifndef TESTMODE
        printf("srcpd -f <nr> -p <Portnummer> -d <Devicepath> -S <I8255-dev> -o\n");
#else
        printf("srcpd -f <nr> -p <Portnummer> -d <Devicepath>  -S <I8255-dev> -o -t\n");
#endif
        printf("nr          -  Number of feedback-modules\n");
        printf("Portnumber  -  first Communicationport via TCP/IP (default: 12345)\n");
        printf("               Portnumber + 0 = Commandport for SRCP-Commands\n");
        printf("               Portnumber + 1 = Feedbackport for SRCP-Commands\n");
        printf("               Portnumber + 2 = Infoport for SRCP\n");
        printf("Devicepath  -  name of serial port to use (default: /dev/ttyS0)\n");
        printf("I8255-dev   -  name of port for I8255\n");
        printf("o           -  use M605X-server in M6020-mode\n");
#ifdef TESTMODE
        printf("t           -  start server in testmode (without connection to real interface)\n");
#endif
        exit(1);
        break;
      default:
  printf("unknown Parameter\n");
  printf("use: \"srcpd -h\" for help\n");
  exit(1);
  break;
    }
  }

  openlog("srcpd", LOG_CONS, LOG_USER);
  syslog(LOG_INFO, "%s", WELCOME_MSG);

  // aktuelle Einstellungen des Com-Ports sichern
  if(restore_com_parms) {
    save_comport(DEV_COMPORT);
  }
  // Initialisierung der seriellen Schnittstelle
  switch (working_server)
  {
    case SERVER_M605X:
      file_descriptor[SERVER_M605X] = init_line6051(DEV_COMPORT);
      if(file_descriptor[SERVER_M605X] < 0)
      {
        printf("Interface 6051 %s nicht vorhanden?!\n", DEV_COMPORT);
  if(restore_com_parms) {
          restore_comport(DEV_COMPORT);
  }
  exit(1);
      }
      break;
    case SERVER_IB:
      if(open_comport(&file_descriptor[SERVER_IB], DEV_COMPORT) != 0)
      {
      printf("Intellibox nicht gefunden !!!\n");
  if(restore_com_parms) {
            restore_comport(DEV_COMPORT);
  }
        exit(1);
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
  /* Initialisieren der Werte... */
  initGL();
  initGA();
  initFB();
  initPower();

  chdir("/");
  /* die Threads starten */
  error = pthread_create(&ttid_cmd, NULL, thr_handlePort, &cmds);
  if(error)
  {
    perror("cannot start Command Thread!");
    exit(1);
  }
  pthread_detach(ttid_cmd);

  error = pthread_create(&ttid_fb, NULL, thr_handlePort, &fbs);
  if(error)
  {
    perror("cannot start FeedBack Thread!");
    exit(1);
  }
  pthread_detach(ttid_fb);

  error = pthread_create(&ttid_info, NULL, thr_handlePort, &infos);
  if(error)
  {
    perror("cannot start Info Thread!");
    exit(1);
  }
  pthread_detach(ttid_info);

  error = pthread_create(&ttid_iface, NULL, server_fkt[working_server], NULL);
  if(error)
  {
    perror("cannot start Interface Thread!");
    exit(1);
  }
  pthread_detach(ttid_iface);

  error = pthread_create(&ttid_clock, NULL, thr_clock, NULL);
  if(error)
  {
    perror("cannot start Clock Thread!");
    exit(1);
  }
  pthread_detach(ttid_clock);

  if(use_i8255)
  {
    error = pthread_create(&ttid_i8255, NULL, thr_doi8255polling, (void*)DEV_I8255);
    if(error)
    {
      perror("cannot start I8255 Thread!");
      exit(1);
    }
    pthread_detach(ttid_i8255);
  }

  tmp_state = 0;
  /* Wachhund */
  while(1)
  {
    if(server_shutdown_state == 1)
    {
      pthread_cancel(ttid_cmd);
      pthread_cancel(ttid_fb);
      pthread_cancel(ttid_info);
      pthread_cancel(ttid_iface);
      pthread_cancel(ttid_clock);
      if(use_i8255)
      {
        pthread_cancel(ttid_i8255);
      }
      break;
    }
    io_thread_running = use_watchdog - 1;
    sleep(2); /* einmal in 2 Sekunden, ist eng?! */
  
    if(io_thread_running==0 || server_reset_state)
    {
      if(!server_reset_state)
      {
        syslog(LOG_INFO, "Oops: 6051 Thread hängt (%d)? Neustart desselben..\n", tmp_state);
      }      
      pthread_cancel(ttid_iface);
      if(use_i8255)
      {
        pthread_cancel(ttid_i8255);
      }
      sleep(1); // Zeit lassen für Neustart
      error = pthread_create(&ttid_iface, NULL, server_fkt[working_server], (void*)DEV_COMPORT);
      if(error)
      {
        perror("cannot start Interface Thread!");
        exit(1);
      }
      pthread_detach(ttid_iface);

      if(use_i8255)
      {
        error = pthread_create(&ttid_i8255, NULL, thr_doi8255polling, (void*)DEV_I8255);
        if(error)
        {
          perror("cannot start i8255'er Thread!");
          exit(1);
    }
      }
      pthread_detach(ttid_i8255);
  
      server_reset_state = 0;
    }
    else
    {
      tmp_state = io_thread_running;
    }
  }
  if(restore_com_parms) {
    close_comport(file_descriptor[working_server]);
    restore_comport(DEV_COMPORT);
  }
  syslog(LOG_INFO, "und tschüß..");
  exit(0);
}
