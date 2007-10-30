/***************************************************************************
                          srcpd.c  -  description
                          -----------------------
    begin                : Wed Jul 4 2001
    copyright            : (C) 2001, 2002 by the srcpd team
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

#define	MAXFD	64 /* for daemon_init */


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
    if (f == NULL)
        syslog(LOG_INFO, "Opening pid file '%s' failed: %s (errno = %d)\n",
               ((SERVER_DATA *) busses[0].driverdata)->PIDFILE,
               strerror(errno), errno);
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
               strerror(errno), errno);
}

/* signal SIGHUP(1) caught */
void hup_handler(int s)
{
    signal(s, hup_handler);
    server_reset_state = 1;
    syslog(LOG_INFO,
           "SIGHUP(1) received! Reset done. Working in initial state.");
}

/* signal SIGTERM(15) caught */
void term_handler(int s)
{
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

    /* Search for the bus that needs to be served */
    retval = select(maxfd + 1, &rfds, NULL, NULL, &tv);
    if (retval == -1) {
        syslog(LOG_INFO, "Select failed: %s (errno = %d)\n",
               strerror(errno), errno);
    }

    /* nothing changed */
    else if (retval == 0) {
        return;
    }

    /* some descriptor changed */
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

/*
 * daemonize process
 * this is from "UNIX Network Programming, W. R. Stevens et al."
 */
int daemon_init()
{
    int i;
    pid_t pid;

    if ((pid = fork()) < 0)
        return (-1);
    else if (pid)
        /* parent terminates */
        _exit(0);

    /* child 1 continues... */

    /* become session leader */
    if (setsid() < 0)
        return (-1);

    signal(SIGHUP, SIG_IGN);
    if ((pid = fork()) < 0)
        return (-1);
    else if (pid)
        /* child 1 terminates */
        _exit(0);

    /* child 2 continues... */

    /* change working directory */
    chdir("/");

    /* close off file descriptors */
    for (i = 0; i < MAXFD; i++)
        close(i);

    /* redirect stdin, stdout, and stderr to /dev/null */
    open("/dev/null", O_RDONLY);
    open("/dev/null", O_RDWR);
    open("/dev/null", O_RDWR);

    /* success */
    return 0;
}

int main(int argc, char **argv)
{
    int result;
    long int i;
    int sleep_ctr;
    char c, conffile[MAXPATHLEN];
    pthread_t ttid_cmd, ttid_clock, ttid_pid;
    struct _THREADS cmds;
    struct sigaction saio;


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

    /* read command line parameters */
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
                printf("unknown parameter\n");
                printf("use: \"srcpd -h\" for help, exiting\n");
                exit(1);
                break;
        }
    }

    DBG(0, DBG_INFO, "conffile = \"%s\"\n", conffile);
    readConfig(conffile);

    cmds.port = ((SERVER_DATA *) busses[0].driverdata)->TCPPORT;
    cmds.func = thr_doClient;

    /* do not daemonize if in debug mode */
    if (busses[0].debuglevel < DBG_DEBUG) {

        /*daemonize process*/
        if (0 != daemon_init()) {
            printf("Daemonization failed!\n");
            exit(1);
        }

    }

    openlog("srcpd", LOG_PID, LOG_USER);
    syslog(LOG_INFO, "%s", WELCOME_MSG);

    install_signal_handler();

    /*
     * Now we have to initialize all buses,
     * this function should open the devices
     */
    maxfd = 0;
    FD_ZERO(&rfds);
    for (i = 0; i <= num_busses; i++) {
        if (busses[i].init_func != NULL) {

            /* error during initialization */
            if ((*busses[i].init_func) (i) != 0) {
                syslog(LOG_INFO, "Initialization of bus %ld failed.", i);
                exit(1);
            }

            if (busses[i].fd != -1) {
                FD_SET(busses[i].fd, &rfds);
                maxfd = (maxfd > busses[i].fd ? maxfd : busses[i].fd);
                /* Configure port to throw read signal */
                fcntl(busses[i].fd, F_SETOWN, getpid());
                fcntl(busses[i].fd, F_SETFL, FASYNC);
            }
        }
    }

    CreatePIDFile(getpid());

    /* Modellzeitgeber starten, der ist aber zunaechst idle */
    result = pthread_create(&ttid_clock, NULL, thr_clock, NULL);
    if (result != 0) {
        syslog(LOG_INFO, "Create clock thread failed: %s (errno = %d)\n",
               strerror(errno), errno);
    }
    pthread_detach(ttid_clock);

    /* und jetzt die Bustreiber selbst starten. Das Device ist offen,
       die Datenstrukturen initialisiert */
    syslog(LOG_INFO, "Going to start %d interface threads for the buses",
           num_busses);

    /* start threads for all buses */
    for (i = 1; i <= num_busses; i++) {
        syslog(LOG_INFO, "Going to start interface thread #%ld type(%d)",
               i, busses[i].type);

        if (busses[i].thr_timer != NULL) {
               result = pthread_create(&ttid_pid, NULL, busses[i].thr_timer,
                                        (void *) i);
               if (result != 0) {
                   syslog(LOG_INFO, "Create timer thread for bus %ld "
                           "failed: %s (errno = %d)\n", i,
                           strerror(errno), errno);
                   exit(1);
               }
               pthread_detach(ttid_pid);
               busses[i].pidtimer = ttid_pid;
        }

        if (busses[i].thr_func != NULL) {
               result = pthread_create(&ttid_pid, NULL, busses[i].thr_func,
                                        (void *) i);
               if (result != 0) {
                   syslog(LOG_INFO, "Create interface thread for bus %ld "
                           "failed: %s (errno = %d)\n", i,
                           strerror(errno), errno);
                    exit(1);
               }
               pthread_detach(ttid_pid);
               busses[i].pid = ttid_pid;
        }

        syslog(LOG_INFO, "Interface thread #%ld started successfully, "
                "type(%d): pid %ld", i, busses[i].type,
                (long) (busses[i].pid));

        if (((busses[i].flags & AUTO_POWER_ON) == AUTO_POWER_ON)) {
            setPower(i, 1, "AUTO POWER ON");
        }
        else {
            setPower(i, 0, "AUTO POWER OFF");
        }
    }

    /* network connection thread */
    result = pthread_create(&ttid_cmd, NULL, thr_handlePort, &cmds);
    if (result != 0) {
        syslog(LOG_INFO, "Create command thread failed: %s (errno = %d)\n",
                strerror(errno), errno);
        exit(1);
    }
    pthread_detach(ttid_cmd);

    syslog(LOG_INFO, "All threads started");

    /* Setup serial interrupt processing */
    // not needed anymore: saio.sa_restorer = NULL;
    saio.sa_flags = 0;
    saio.sa_handler = &processSignal;
    sigemptyset(&saio.sa_mask);
    /* Apply interrupt handling for this process */
    sigaction(SIGIO, &saio, NULL);

    server_shutdown_state = 0;
    sleep_ctr = 10;

    /*
     * Main loop: Wait for _real_ tasks: shutdown, reset and watch for
     * hanging processes
     */
    while (1) {
        if (server_shutdown_state == 1) {
            /* wait 2 seconds, according to protocol specification */
            sleep(2);
            break;
        }

        usleep(100000);
        sleep_ctr--;

        if (sleep_ctr == 0) {
            /* clear LOCKs */
            unlock_gl_bytime();
            unlock_ga_bytime();

            /* activate watchdog if necessary */
            for (i = 1; i <= num_busses; i++) {
                if (busses[i].watchdog == 0
                    && !queue_GL_isempty(i)
                    && !queue_GA_isempty(i)
                    && (busses[i].flags & USE_WATCHDOG)) {
                    syslog(LOG_INFO, "Oops: Interface thread #%ld "
                            "hangs, restarting: (old pid: %ld, %d)",
                            i, (long) busses[i].pid, busses[i].watchdog);
                    pthread_cancel(busses[i].pid);
                    waitpid((long) busses[i].pid, NULL, 0);
                    result = pthread_create(&ttid_pid, NULL,
                            busses[i].thr_func, (void *) i);
                    if (result != 0) {
                        syslog(LOG_INFO, "Recreate interface thread "
                                "failed: %s (errno = %d)\n",
                                strerror(errno), errno);
                        break;
                    }
                    busses[i].pid = ttid_pid;
                    pthread_detach(busses[i].pid);
                }
                busses[i].watchdog = 0;
            }
            sleep_ctr = 10;
        }
    }

    syslog(LOG_INFO, "Terminating SRCP service...");
    pthread_cancel(ttid_cmd);
    pthread_cancel(ttid_clock);

    /* now terminate all bus threads */
    for (i = 1; i <= num_busses; i++) {
        pthread_cancel(busses[i].pid);
    }

    /*FIXME: this operation fails due to missing access rights*/ 
    DeletePIDFile();

    /* server thread is last to terminate */
    for (i = num_busses; i >= 0; i--) {
        (*busses[i].term_func) (i);
    }
    wait(0);
    syslog(LOG_INFO, "SRCP service terminated.");
    closelog();
    exit(0);
}

