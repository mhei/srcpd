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

int ClientID=1;
pthread_mutex_t ClientID_mut = PTHREAD_MUTEX_INITIALIZER;

void* thr_doClient(void* v) {
  int socket = (int) v;
  char line[1000], cmd[1000], setcmd[1000], parameter[1000], reply[1000];
  int mode = COMMAND;
  int clientid;

  pthread_mutex_lock(&ClientID_mut);
  clientid = ClientID++;
  pthread_mutex_unlock(&ClientID_mut);
  
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
	return NULL;
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

void handleSET(int clientid, int bus, char *device, char *parameter, char *reply) {
    strcpy(reply, "422 ERROR no such device group on bus");
      /* es wird etwas gesetzt.. */
      if(strncasecmp(device, "GL", 2) == 0)
      {
        long laddr, direction, speed, maxspeed, func, n_fkt, f1, f2, f3, f4;
        int anzparms;
        anzparms = sscanf(parameter, "%ld %ld %ld %ld %ld %ld %ld %ld %ld %ld",
           &laddr, &direction, &speed, &maxspeed, &func, &n_fkt, &f1, &f2, &f3, &f4);
        if(anzparms>5)
        {
          setGL(bus, laddr, direction, speed, maxspeed, func, n_fkt, f1, f2, f3, f4);
        }
      }
      if(strncasecmp(device, "GA", 2) == 0)
      {
        long gaddr, port, aktion, delay;
        sscanf(parameter, "%ld %ld %ld %ld", &gaddr, &port, &aktion, &delay);
        /* Port 0,1; Aktion 0,1 */
        setGA(bus, gaddr, port, aktion, delay);
      }
      if(strncasecmp(device, "TIME", 4) == 0)
      {
        long d, h, m, s, rx, ry;
        sscanf(parameter, "%ld %ld %ld %ld %ld %ld", &d, &h, &m, &s, &rx, &ry);
        setTime(d, h, m, s, rx, ry);
      }
      if(strncasecmp(device, "POWER", 5) == 0)
      {
        char state[5], msg[256];
        memset(msg, 0, sizeof(msg));
        sscanf(parameter, "%s %100c", state, msg);
        if(strncasecmp(state, "OFF", 3) == 0)
        {
          setPower(bus, 0, msg);
        }
        else
        {
          if(strncasecmp(state, "ON", 2) == 0)
            setPower(bus, 1, msg);
        }
      }

}
void handleGET(int clientid, int bus, char *device, char *parameter, char *reply) {
      strcpy(reply, "423 ERROR unsupported operation");
      /* es wird etwas abgefragt */
      if(strncasecmp(device, "FB", 2) == 0)
      {
        long int nelem, port;
        nelem = sscanf(parameter, "%ld", port);
        infoFB(bus, port, reply);
      }
      if(strncasecmp(device, "GL", 2) == 0)
      {
        long laddr;
        struct _GL l;
        sscanf(parameter, "%ld", &laddr);
        if(getGL(bus, laddr, &l))
          infoGL(l, reply);
        else
          strcpy(reply, "416 ERROR no data");
      }
      if(strncasecmp(device, "GA", 2) == 0)
      {
        long addr;
        struct _GA a;
        sscanf(parameter, "%ld", &addr);
        if(getGA(bus, addr, &a))
          infoGA(a, reply);
        else
          strcpy(reply, "416 ERROR no data");
      }
      if(strncasecmp(device, "POWER", 5) == 0)
        infoPower(bus, reply);
      if(strncasecmp(device, "TIME", 4) == 0)
      {
        if(vtime.ratio_x && vtime.ratio_y)
        {
          infoTime(vtime, reply);
        }
      }

}

void handleRESET(int clientid, int bus, char *device, char *parameter, char *reply) {
}
void handleWAIT(int clientid, int bus, char *device, char *parameter, char *reply) {
      /* Wir warten.. */
      strcpy(reply, "INFO -1");
      if(strncasecmp(device, "FB", 2) == 0)
      {
        long int port, waitvalue, aktvalue, timeout;
        sscanf(parameter, "%ld %ld %ld", &port, &waitvalue, &timeout);
        if(getFB(bus, port)==waitvalue)
        {
          infoFB(bus, port, reply);
        }
        else
        {
          /* wir warten 1/100 Sekunden genau */
          timeout *= 100;
          do
          {
            /* fprintf(stderr, "waiting %d (noch %d sekunden)\n", port, timeout); */
            usleep(10000);
            aktvalue = getFB(bus, port);
            timeout--;
          } while (timeout>=0 && aktvalue!=waitvalue);
          if(timeout<0)
            strcpy(reply, "417 ERROR timeout");
          else
            infoFB(bus, port, reply);
        }
      }
      if(strncasecmp(device, "TIME", 4) == 0)
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

int handleTERM(int clientid, int bus, char *device, char *parameter, char *reply) {
    int rc = 0;
      strcpy(reply, "421 ERROR unsupported device");
      if(strncasecmp(device, "FB", 2) == 0)
      {
      }
      if(strncasecmp(device, "SERVER", 6) == 0) {
        if(bus==0) {
	    strcpy(reply, "200 OK");
            server_shutdown(bus);
	} else {
	    strcpy(reply, "422 ERROR");
	}
      }
      if(strncasecmp(device, "SESSION", 7) == 0) {
        if(bus==0) {
	    strcpy(reply, "200 OK");
	    rc = 1;
	} else {
	    strcpy(reply, "422 ERROR");
	}
      }
    return rc;
}
void handleINIT(int clientid, int bus, char *device, char *parameter, char *reply) {
      if(strncasecmp(device, "TIME", 4) == 0)
      {
        long d, h, m, s, rx, ry;
        sscanf(parameter, "%ld %ld %ld %ld %ld %ld", &d, &h, &m, &s, &rx, &ry);
        setTime(d, h, m, s, rx, ry); /* prüft auch die Werte! */
      }

}
void handleVERIFY(int clientid, int bus, char *device, char *parameter, char *reply) {
}
void handleCHECK(int clientid, int bus, char *device, char *parameter, char *reply) {
}

/***************************************************************
 *  Für jeden Client läuft ein Thread, er macht das
 *  SRCP indem er die Datenstrukturen passend füllt
 *  und die Ergebnisse passend zurückschreibt
 ***************************************************************
 */

void* doCmdClient(int socket, int clientid)
{
  char line[1024], reply[4095];
  char command[20], device[20], parameter[900];
  long int bus;

  while(1)
  {
    memset(line, 0, sizeof(line));
    strcpy(reply, "410 ERROR unsupported command");
    if(socket_readline(socket, line, sizeof(line)-1)<0)
    {
      shutdown(socket, 0);
      close(socket);
      return NULL;
    }
    memset(command, 0, sizeof(command));
    memset(device, 0, sizeof(device));
    memset(parameter, 0, sizeof(parameter));
    sscanf(line, "%s %ld %s %200c", command, &bus, device, parameter);
    if(strncasecmp(command, "SET", 3) == 0) {
      handleSET(clientid, bus, device, parameter, reply);
    }
    if(strncasecmp(command, "GET", 3) == 0) {
      handleGET(clientid, bus, device, parameter, reply);
    }
    if(strncasecmp(command, "WAIT", 4) == 0)    {
      handleWAIT(clientid, bus, device, parameter, reply);
    }
    if(strncasecmp(command, "INIT", 4) == 0)    {
      handleINIT(clientid, bus, device, parameter, reply);
    }
    if(strncasecmp(command, "TERM", 4) == 0)    {
      if(handleTERM(clientid, bus, device, parameter, reply))
        break;
    }
    if(strncasecmp(command, "VERIFY", 6) == 0) {
      handleVERIFY(clientid, bus, device, parameter, reply);
    }
    if(strncasecmp(command, "RESET", 5) == 0) {
      handleRESET(clientid, bus, device, parameter, reply);
    }
    if(socket_writereply(socket, reply) < 0)  {
      break;
    }
  }
  shutdown(socket, 2);
  close(socket);
  return NULL;
}

void* doInfoClient(int socket, int clientid) {
    char reply[1000];
    while(1) {	
	strcpy(reply, "I'm alive");
	socket_writereply(socket, reply);
	sleep(1);
    }
}