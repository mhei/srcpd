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
#include <sys/time.h>

#include "config-srcpd.h"
#include "srcp-fb.h"
#include "srcp-time.h"
#include "srcp-ga.h"
#include "srcp-gl.h"
#include "srcp-power.h"
#include "srcp-srv.h"
#include "netserver.h"
#include "m605x.h"

#define COMMAND 1
#define INFO    2

extern struct _VTIME vtime;
extern char* WELCOME_MSG;

/* Zeilenweises Lesen vom Socket      */
/* nicht eben trivial!                */
int socket_readline(int socket, char *line, int len)
{
  char c;
  int i = 0;
  int bytes_read = read(socket, &c, 1);
  if(bytes_read <=0)
  {
    return -1;
  }
  else
  {
    line[0] = c;
    /* die reihenfolge beachten! */
    while(c!='\n' && c!= 0x00 && read(socket, &c, 1)>0)
    {
      /* Ende beim Zeilenende */
      if(c=='\n')
        break;
      /* Folgende Zeichen ignorieren wir */
      if(c=='\r' || c==(char)0x00 || i>=len)
        continue;
      line[++i] = c;
    }
  }
  line[++i] = 0x00;
  return 0;
}

/******************
 * noch ganz klar ein schoenwetter code!
 *
 */
int socket_writereply(int socket, char *line) {
    char buf[1024];
    struct timeval akt_time;    
    gettimeofday(&akt_time, NULL);

    sprintf(buf, "%ld.%ld %s\n", akt_time.tv_sec, akt_time.tv_usec, line);
    return write(socket, buf, strlen(buf));

}

/******************
 * handles shakehand phase
 *
 *********************
 */

int ClientID=0;

void* thr_doClient(void* v) {
  int socket = (int) v;
  char line[1000], cmd[1000], setcmd[1000], parameter[1000], reply[1000];
  int mode = COMMAND;

  /* hmm. eigentlich wird eine Semaphore gebraucht...*/  
  int clientid = ClientID++;
  
  if(write(socket, WELCOME_MSG, strlen(WELCOME_MSG))<0 )
  {
    shutdown(socket, 2);
    close(socket);
    return NULL;
  }
  while(1) {
    strcpy(reply, "402 ERROR unsufficient data");
    memset(cmd, 0, sizeof(cmd));
    if(socket_readline(socket, line, sizeof(line)-1)<0) {
	  shutdown(socket, 0);
          close(socket);
          return NULL;
    }
    sscanf(line, "%s %1000c", cmd, parameter);
    if(strncasecmp(cmd, "GO", 2) == 0) {
        sprintf(reply, "200 OK GO %ld", clientid);
        if(socket_writereply(socket, reply) < 0)
        {
         shutdown(socket, 2);
         close(socket);
         return NULL;
        }
	switch(mode) {
          case COMMAND:
            return doCmdClient(socket, clientid);
            break;
          case INFO:
	    return doInfoClient(socket, clientid);
	    break;
	}
    }	
    if(strncasecmp(cmd, "SET", 3) == 0) {
      char p[1000];
      sscanf(parameter, "%s %1000c", setcmd, p);
      if(strncasecmp(setcmd, "CONNECTIONMODE", 3) == 0) {
        strcpy(reply, "401 ERROR unsupported connection mode");
        if(strncasecmp(p, "SRCP INFO", 9) == 0) {
	    mode = INFO;
	    strcpy(reply, "202 OK CONNECTIONMODE");
	}
	if(strncasecmp(p, "SRCP COMMAND", 12) == 0 ) {
	    mode = COMMAND;
	    strcpy(reply, "202 OK CONNECTIONMODE");
	}
      }
      if(strncasecmp(setcmd, "PROTOCOL", 3) == 0) {
        strcpy(reply, "401 ERROR unsupported protocol");
        if(strncasecmp(p, "SRCP 0.8", 8) == 0) {
	    strcpy(reply, "202 OK PROTOCOL SRCP");
	}
      }

    }
    socket_writereply(socket, reply);
  }
}

/***************************************************************
 *  Für jeden Client läuft ein Thread, er macht das
 *  SRCP indem er die Datenstrukturen passend füllt
 *  und die Ergebnisse passend zurückschreibt
 ***************************************************************
 */

void* doCmdClient(int socket, int sessionid)
{
  char cmd[256], reply[4095];
  char command[20], spec[20], parameter[256];

  while(1)
  {
    memset(reply, 0, sizeof(reply));
    memset(cmd, 0, sizeof(cmd));
    if(socket_readline(socket, cmd, sizeof(cmd)-1)<0)
    {
      shutdown(socket, 0);
      close(socket);
      return NULL;
    }
    memset(command, 0, sizeof(command));
    memset(spec, 0, sizeof(spec));
    memset(parameter, 0, sizeof(parameter));
    sscanf(cmd, "%s %s %200c", command, spec, parameter);
    if(strncasecmp(command, "LOGOUT", 5) == 0)
    {
      shutdown(socket, 0);
      close(socket);
      return NULL;
    }
    if(strncasecmp(command, "RESET", 5) == 0)
      server_reset();
    if(strncasecmp(command, "SHUTDOWN", 8) == 0)
      server_shutdown();
    /* Befehl ermitteln */
    if(strncasecmp(command, "SET", 3) == 0)
    {
      /* es wird etwas gesetzt.. */
      if(strncasecmp(spec, "GL", 2) == 0)
      {
        long bus, laddr, direction, speed, maxspeed, func, n_fkt, f1, f2, f3, f4;
        int anzparms;
        anzparms = sscanf(parameter, "%ld %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld", &bus,
           &laddr, &direction, &speed, &maxspeed, &func, &n_fkt, &f1, &f2, &f3, &f4);
        if(anzparms>5)
        {
          setGL(sessionid, bus, laddr, direction, speed, maxspeed, func, n_fkt, f1, f2, f3, f4);
        }
      }
      if(strncasecmp(spec, "GA", 2) == 0)
      {
        long bus, gaddr, port, aktion, delay;
        sscanf(parameter, "%ld %ld %ld %ld %ld", &bus, &gaddr, &port, &aktion, &delay);
        /* Port 0,1; Aktion 0,1 */
        setGA(sessionid, bus, gaddr, port, aktion, delay);
      }
      if(strncasecmp(spec, "TIME", 4) == 0)
      {
        long d, h, m, s, rx, ry;
        sscanf(parameter, "%ld %ld %ld %ld %ld %ld", &d, &h, &m, &s, &rx, &ry);
        setTime(d, h, m, s, rx, ry);
      }
      if(strncasecmp(spec, "POWER", 5) == 0)
      {
        char state[5], msg[256];
        memset(msg, 0, sizeof(msg));
        sscanf(parameter, "%s %100c", state, msg);
        if(strncasecmp(state, "OFF", 3) == 0)
        {
          setPower(0, msg);
        }
        else
        {
          if(strncasecmp(state, "ON", 2) == 0)
            setPower(1, msg);
        }
      }
    }
    if(strncasecmp(command, "GET", 3) == 0)
    {
      strcpy(reply, "INFO -1");
      /* es wird etwas abgefragt */
      if(strncasecmp(spec, "FB", 2) == 0)
      {
        long int nelem, bus, port;
        unsigned char parm[127], d[4096];
        nelem = sscanf(parameter, "%s %s", d, parm);
        port = atol(parm);
	bus  = atol(d);
        infoFB(bus, port, d);
        strcpy(reply, d);
      }
      if(strncasecmp(spec, "GL", 2) == 0)
      {
        long bus, laddr;
        struct _GL l;
        sscanf(parameter, "%ld %ld", &bus, &laddr);
        if(getGL(bus, laddr, &l))
          infoGL(l, reply);
        else
          strcpy(reply, "INFO -2");
      }
      if(strncasecmp(spec, "GA", 2) == 0)
      {
        long bus, addr;
        struct _GA a;
        sscanf(parameter, "%ld %ld", &bus, &addr);
        if(getGA(bus, addr, &a))
          infoGA(a, reply);
        else
          strcpy(reply, "INFO -2");
      }
      if(strncasecmp(spec, "POWER", 5) == 0)
        infoPower(reply);
      if(strncasecmp(spec, "TIME", 4) == 0)
      {
        if(vtime.ratio_x && vtime.ratio_y)
        {
          infoTime(vtime, reply);
        }
      }
    }
    if(strncasecmp(command, "WAIT", 4) == 0)
    {
      /* Wir warten.. */
      strcpy(reply, "INFO -1");
      if(strncasecmp(spec, "FB", 2) == 0)
      {
        long int bus, port, waitvalue, aktvalue, timeout;
        unsigned char d[256];
        sscanf(parameter, "%ld %ld %ld %ld", &bus, &port, &waitvalue, &timeout);
        if(getFBone(bus, port)==waitvalue)
        {
          infoFB(bus, port, d);
        }
        else
        {
          /* wir warten 1/100 Sekunden genau */
          timeout *= 100;
          do
          {
            /* fprintf(stderr, "waiting %d (noch %d sekunden)\n", port, timeout); */
            usleep(10000);
            aktvalue = getFBone(bus, port);
            timeout--;
          } while (timeout>=0 && aktvalue!=waitvalue);
          if(timeout<0)
            strcpy(d, "INFO -3");
          else
            infoFB(bus, port, d);
        }
        strcpy(reply, d);
      }
      if(strncasecmp(spec, "TIME", 4) == 0)
      {
        unsigned long d, h, m, s;
        sscanf(parameter, "%ld %ld %ld %ld", &d, &h, &m, &s);
        /* es wird nicht gerechnet!, der Zeitfluß nicht nicht gleichmäßig! */
        while (d < vtime.day && h < vtime.hour && m < vtime.min && s < vtime.sec)
        {
          usleep(1000); /* wir warten 1ms realzeit.. */
        }
        infoTime(vtime, reply);
      }
    }
    if(strncasecmp(command, "INIT", 4) == 0)
    {
      if(strncasecmp(spec, "FB", 2) == 0)
      {
        char proto[256];
        long int portnum, baseport, nelem;
        nelem = sscanf(parameter, "%s %ld %ld ", proto, &portnum, &baseport);
      }
      if(strncasecmp(spec, "TIME", 4) == 0)
      {
        long d, h, m, s, rx, ry;
        sscanf(parameter, "%ld %ld %ld %ld %ld %ld", &d, &h, &m, &s, &rx, &ry);
        setTime(d, h, m, s, rx, ry); /* prüft auch die Werte! */
      }
    }
    if(strncasecmp(command, "TERM", 4) == 0)
    {
      if(strncasecmp(spec, "FB", 2) == 0)
      {
      }
    }
    if(strncasecmp(command, "VERIFY", 6) == 0)
      strcpy(reply, "INFO -1");
    if(strlen(reply)>0)
      if(socket_writereply(socket, reply) < 0)
      {
        shutdown(socket, 2);
        close(socket);
        return NULL;
      }
  }
}

void* doInfoClient(int socket, int sessionid) {
    char reply[1000];
    while(1) {	
	strcpy(reply, "I'm alive");
	socket_writereply(socket, reply);
	sleep(1);
    }
}
