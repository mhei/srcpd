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
#include "srcp-session.h"
#include "io.h"
#include "netserver.h"
#include "srcp-descr.h"
#include "srcp-fb.h"
#include "srcp-ga.h"
#include "srcp-gl.h"
#include "srcp-lock.h"
#include "srcp-power.h"

#include "srcp-srv.h"
#include "srcp-time.h"
#include "srcp-info.h"
#include "threads.h"

void hup_handler(int);
void term_handler(int);
void install_signal_handler(void);

/* structures to determine which port needs to be served */
fd_set rfds;
int maxfd;

extern int server_shutdown_state;
extern int server_reset_state;
extern const char *WELCOME_MSG;

void CreatePIDFile(int pid)
{
    FILE *f;
    f = fopen(((SERVER_DATA *) busses[0].driverdata)->PIDFILE, "wb");
    if (!f)
        syslog(LOG_INFO, "   cannot open %s. Ignoring.",
               ((SERVER_DATA *) busses[0].driverdata)->PIDFILE);
    else {
        fprintf(f, "%d\n", pid);
        fflush(f);
        fclose(f);
    }
}

void DeletePIDFile()
{
    int result;
    result = unlink(((SERVER_DATA *) busses[0].driverdata)->PIDFILE);
    if (result != 0)
        syslog(LOG_INFO, "Unlinking pid file failed: %s (errno = %d)\n",
               strerror(errno),  errno);
}

void hup_handler(int s)
{
    /* signal SIGHUP(1) caught */
    server_reset_state = 1;
    syslog(LOG_INFO,
           "SIGHUP(1) received! Reset done. Working in initial state.");

    signal(s, hup_handler);

    return;
}

void term_handler(int s)
{
    // signal SIGTERM(15) caught
    syslog(LOG_INFO, "SIGTERM(15) received! Terminating ...");
    server_shutdown_state = 1;
}

void install_signal_handler()
{
    signal(SIGTERM, term_handler);
    signal(SIGABRT, term_handler);
    signal(SIGINT, term_handler);
    signal(SIGHUP, hup_handler);
    signal(SIGPIPE, SIG_IGN);
    /* important, because write() on sockets should return errors */
}

/** processSignal
 * Signal handler for comport signal
 */
void processSignal(int status)
{
    struct timeval tv;
    int retval;
    long int i;

    /* Don't wait */
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    /* Search for the bus that needs to be servered */
    retval = select(maxfd + 1, &rfds, NULL, NULL, &tv);
    if (retval < 0) {
        /* Error from select */
    }
    else {
        for (i = 1; i <= num_busses; i++) {
            if ((busses[i].fd != -1) && (FD_ISSET(busses[i].fd, &rfds))) {
                if (busses[i].sig_reader != NULL) {
                   (*busses[i].sig_reader) (i);
                }
            }
        }
    }
}

int main(int argc, char **argv)
{
    int error;
    long int i;
    int sleep_ctr;
    pid_t pid;
    char c, conffile[MAXPATHLEN];
    pthread_t ttid_cmd, ttid_clock, ttid_pid;
    struct _THREADS cmds;
    struct sigaction saio;

    install_signal_handler();

    /* First: Init the device data used internally */
    startup_GL();
    startup_GA();
    startup_FB();
    startup_INFO();
    startup_LOCK();
    startup_DESCRIPTION();
    startup_TIME();
    startup_SERVER();
    startup_SESSION();

    sprintf(conffile, "%s/srcpd.conf", SYSCONFDIR);

    /* Parameter auswerten */
    opterr = 0;
    while ((c = getopt(argc, argv, "f:hv")) != EOF) {
        switch (c) {
            case 'f':
                if (strlen(optarg) < MAXPATHLEN - 1)
                    strcpy(conffile, optarg);
                break;
            case 'v':
                printf(WELCOME_MSG);
                exit(1);
                break;
            case 'h':
                printf(WELCOME_MSG);
                printf("srcpd -f <conffile> -v -h\n");
                printf
                    ("v           -  prints program version and exits\n");
                printf
                    ("f           -  use another config file (default %s)\n",
                     conffile);
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

    DBG(0, DBG_INFO, "conffile = \"%s\"\n", conffile);
    readConfig(conffile);

    cmds.port = ((SERVER_DATA *) busses[0].driverdata)->TCPPORT;
    cmds.func = thr_doClient;
    openlog("srcpd", LOG_CONS, LOG_USER);
    syslog(LOG_INFO, "%s", WELCOME_MSG);

    /* Now we have to initialize all busses */
    /* this function should open the device */
    maxfd = 0;
    FD_ZERO(&rfds);
    for (i = 0; i <= num_busses; i++) {
        if (busses[i].init_func) {
            if ((*busses[i].init_func) (i) != 0)
                exit(1);        // error while initialize
            if (busses[i].fd != -1) {
                FD_SET(busses[i].fd, &rfds);
                maxfd = (maxfd > busses[i].fd ? maxfd : busses[i].fd);
                /* Configure port to throw read signal */
                fcntl(busses[i].fd, F_SETOWN, getpid());
                fcntl(busses[i].fd, F_SETFL, FASYNC);
            }
        }
    }

    // a little help for debugging the threads
    if (busses[0].debuglevel < DBG_DEBUG) {
        /* forken */
        if ((pid = fork()) < 0) {
            return -1;
        }
        else {
            if (pid != 0) {
                exit(0);
            }
        }
        // ab hier keine Konsole mehr... Wir sind ein D�on geworden!
        chdir("/");
    }

    CreatePIDFile(getpid());


    /* Modellzeitgeber starten, der ist aber zun�hst idle */
    error = pthread_create(&ttid_clock, NULL, thr_clock, NULL);
    if (error) {
        syslog(LOG_INFO, "cannot start Clock Thread!");
    }
    pthread_detach(ttid_clock);
    /* und jetzt die Bustreiber selbst starten. Das Device ist offen,
       die Datenstrukturen initialisiert */
    syslog(LOG_INFO, "Going to start %d Interface Threads for the busses",
           num_busses);
    /* Jetzt die Threads fr die Busse */
    for (i = 1; i <= num_busses; i++) {
        syslog(LOG_INFO, "going to start Interface Thread  #%ld type(%d)",
               i, busses[i].type);
        if (busses[i].thr_timer != NULL) {
               error = pthread_create(&ttid_pid, NULL, busses[i].thr_timer,
                                        (void *) i);
               if (error) {
                    syslog(LOG_INFO, "cannot start Timer Thread #%ld", i);
                    exit(1);
               }
               pthread_detach(ttid_pid);
               busses[i].pidtimer = ttid_pid;
        }
        if (busses[i].thr_func != NULL) {
               error = pthread_create(&ttid_pid, NULL, busses[i].thr_func,
                                        (void *) i);
               if (error) {
                    syslog(LOG_INFO, "cannot start Interface Thread #%ld", i);
                    exit(1);
               }
               pthread_detach(ttid_pid);
               busses[i].pid = ttid_pid;
        }
        syslog(LOG_INFO,
               "Interface Thread #%ld started successfully type(%d): pid %ld",
               i, busses[i].type, (long) (busses[i].pid));
        if (((busses[i].flags & AUTO_POWER_ON) == AUTO_POWER_ON)) {
            setPower(i, 1, "AUTO POWER ON");
        }
        else {
            setPower(i, 0, "AUTO POWER OFF");
        }
    }
    /* Netzwerkverbindungen */
    error = pthread_create(&ttid_cmd, NULL, thr_handlePort, &cmds);
    if (error) {
        syslog(LOG_INFO, "cannot start Command Thread #%ld: %d", i, error);
        exit(1);
    }
    pthread_detach(ttid_cmd);

    syslog(LOG_INFO, "All Threads started");
    /* Setup serial interrupt processing */
    // not needed anymore: saio.sa_restorer = NULL;
    saio.sa_flags = 0;
    saio.sa_handler = &processSignal;
    sigemptyset(&saio.sa_mask);
    /* Apply interrupt handling for this proces */
    sigaction(SIGIO, &saio, NULL);

    server_shutdown_state = 0;
    sleep_ctr = 10;
    /* And now: Wait for _real_ tasks: shutdown, reset, watch for
       hanging processes */
    while (1) {
        if (server_shutdown_state == 1) {
            sleep(2);           /* Protokollforderung */
            break;              /* leave the while() loop */
        }
        usleep(100000);

        sleep_ctr--;
        if (sleep_ctr == 0) {
            /* LOCKs aufraeumen */
            unlock_gl_bytime();
            unlock_ga_bytime();

            /* Jetzt Wachhund spielen, falls gewnscht */
            for (i = 1; i <= num_busses; i++) {
                if (busses[i].watchdog == 0
                    && !queue_GL_isempty(i)
                    && !queue_GA_isempty(i)
                    && (busses[i].flags & USE_WATCHDOG)) {
                    syslog(LOG_INFO, "Oops: Interface Thread #%ld "
                            "hangs, restarting it: (old pid: %ld, %d)",
                            i, (long) busses[i].pid, busses[i].watchdog);
                    pthread_cancel(busses[i].pid);
                    waitpid((long) busses[i].pid, NULL, 0);
                    error =
                        pthread_create(&ttid_pid, NULL, busses[i].thr_func,
                                       (void *) i);
                    if (error) {
                        perror("Cannot restart Interface Thread!");
                        break;  /* erm�licht aufr�men am Ende */
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
    for (i = 1; i <= num_busses; i++) {
        pthread_cancel(busses[i].pid);
    }
 
    DeletePIDFile();
    // der Server als letzter
    for (i = num_busses; i >= 0; i--) {
        (*busses[i].term_func) (i);
    }
    wait(0);
    syslog(LOG_INFO, "bye bye... ;=)");
    exit(0);
}



