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
#include "srcp-error.h"
#include "srcp-fb.h"
#include "srcp-time.h"
#include "srcp-ga.h"
#include "srcp-gl.h"
#include "srcp-power.h"
#include "srcp-srv.h"
#include "srcp-session.h"
#include "netserver.h"
#include "m605x.h"

#define COMMAND 1
#define INFO    2

extern struct _VTIME vtime;
extern char *WELCOME_MSG;

/* Zeilenweises Lesen vom Socket      */
/* nicht eben trivial!                */
int socket_readline(int Socket, char *line, int len)
{
  char c;
  int i = 0;
  int bytes_read = read(Socket, &c, 1);
  if (bytes_read <= 0)
  {
    return -1;
  }
  else
  {
    line[0] = c;
    /* die reihenfolge beachten! */
    while (c != '\n' && c != 0x00 && read(Socket, &c, 1) > 0)
    {
      /* Ende beim Zeilenende */
      if (c == '\n')
        break;
      /* Folgende Zeichen ignorieren wir */
      if (c == '\r' || c == (char) 0x00 || i >= len)
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
int socket_writereply(int Socket, int srcpcode, const char *line)
{
  char buf[1024];
  char buf2[511];
  struct timeval akt_time;

  srcp_fmt_msg(srcpcode, buf2);
  gettimeofday(&akt_time, NULL);
  sprintf(buf, "%ld.%ld %s %s\n", akt_time.tv_sec, akt_time.tv_usec / 1000,
    buf2, line);
  return(write(Socket, buf, strlen(buf)));
}

/******************
 * handles shakehand phase
 *
 *********************
 */

long int SessionID = 1;
pthread_mutex_t SessionID_mut = PTHREAD_MUTEX_INITIALIZER;

void * thr_doClient(void *v)
{
  int Socket = (int) v;
  char line[1000], cmd[1000], setcmd[1000], parameter[1000], reply[1000];
  int mode = COMMAND;
  long int sessionid, rc;

  if (write(Socket, WELCOME_MSG, strlen(WELCOME_MSG)) < 0)
  {
    shutdown(Socket, 2);
    close(Socket);
    return NULL;
  }
  while (1)
  {
    rc = 402;
    reply[0] = 0x00;
    memset(cmd, 0, sizeof(cmd));
    if (socket_readline(Socket, line, sizeof(line) - 1) < 0)
    {
      shutdown(Socket, 0);
      close(Socket);
      return NULL;
    }
    sscanf(line, "%s %1000c", cmd, parameter);
    if (strncasecmp(cmd, "GO", 2) == 0)
    {
      pthread_mutex_lock(&SessionID_mut);
      sessionid = SessionID++;
      pthread_mutex_unlock(&SessionID_mut);

      sprintf(reply, "GO %ld", sessionid);
      if (socket_writereply(Socket, SRCP_OK_GO, reply) < 0)
      {
        shutdown(Socket, 2);
        close(Socket);
        return NULL;
      }
      switch (mode)
      {
        case COMMAND:
                      start_session(sessionid, mode);
                      rc = doCmdClient(Socket, sessionid);
                      stop_session(sessionid);
                      return NULL;
                      break;
        case INFO:
                      start_session(sessionid, mode);
                      rc = doInfoClient(Socket, sessionid);
                      stop_session(sessionid);
                      return NULL;
                      break;
      }
      return NULL;
    }
    if (strncasecmp(cmd, "SET", 3) == 0)
    {
      char p[1000];
      sscanf(parameter, "%s %1000c", setcmd, p);
      if (strncasecmp(setcmd, "CONNECTIONMODE", 3) == 0)
      {
        rc = 401;
        if (strncasecmp(p, "SRCP INFO", 9) == 0)
        {
          mode = INFO;
          rc = SRCP_OK_CONNMODE;
          strcpy(reply, "CONNECTIONMODE");
        }
        if (strncasecmp(p, "SRCP COMMAND", 12) == 0)
        {
          mode = COMMAND;
          rc = SRCP_OK_CONNMODE;
          strcpy(reply, "CONNECTIONMODE");
        }
      }
      if (strncasecmp(setcmd, "PROTOCOL", 3) == 0)
      {
        rc = 401;
        if (strncasecmp(p, "SRCP 0.8", 8) == 0)
        {
          strcpy(reply, "PROTOCOL SRCP");
          rc = SRCP_OK_PROTOCOL;
        }
      }
    }
    socket_writereply(Socket, rc, reply);
  }
}

int handleSET(int sessionid, int bus, char *device, char *parameter, char *reply) {
  int rc = 422;
  *reply = 0x00;
  /* es wird etwas gesetzt.. */
  if (strncasecmp(device, "GL", 2) == 0)
  {
    long laddr, direction, speed, maxspeed, func, f1, f2, f3, f4;
    int anzparms;
    anzparms =
    sscanf(parameter, "%ld %ld %ld %ld %ld %ld %ld %ld %ld",
       &laddr, &direction, &speed, &maxspeed, &func,
       &f1, &f2, &f3, &f4);
    if (anzparms > 5)
    {
      rc = queueGL(bus, laddr, direction, speed, maxspeed, func, f1, f2, f3, f4);
    }
  }
  if (strncasecmp(device, "GA", 2) == 0)
  {
    long gaddr, port, aktion, delay;
    sscanf(parameter, "%ld %ld %ld %ld", &gaddr, &port, &aktion, &delay);
    /* Port 0,1; Aktion 0,1 */
    rc = queueGA(bus, gaddr, port, aktion, delay);
  }
  if (strncasecmp(device, "TIME", 4) == 0)
  {
    long d, h, m, s, rx, ry;
    sscanf(parameter, "%ld %ld %ld %ld %ld %ld", &d, &h, &m, &s, &rx, &ry);
    rc = setTime(d, h, m, s, rx, ry);
  }
  if(strncasecmp(device, "LOCK", 4) == 0) {
    long int addr;
    char devgrp[10];
    int nelem=-1;
    if(strlen(parameter)>0)
        nelem = sscanf(parameter, "%s %ld", devgrp, &addr);
    if(nelem <= 0) {
        rc = SRCP_LISTTOOSHORT;
    } else {
        rc = SRCP_UNSUPPORTEDDEVICEGROUP;
        if(strncmp(devgrp, "GL", 2)==0)
            rc = lockGL(bus, addr, sessionid);
        if(strncmp(devgrp, "GA", 2)==0)
            rc = getlockGA(bus, addr, sessionid);
    }
  }

  if (strncasecmp(device, "POWER", 5) == 0)
  {
    char state[5], msg[256];
    memset(msg, 0, sizeof(msg));
    sscanf(parameter, "%s %100c", state, msg);
    if (strncasecmp(state, "OFF", 3) == 0)
    {
      rc = setPower(bus, 0, msg);
    }
    else
    {
      if (strncasecmp(state, "ON", 2) == 0)
      {
        rc = setPower(bus, 1, msg);
      }
    }
  }
  return rc;
}

int handleGET(int sessionid, int bus, char *device, char *parameter, char *reply) {
  int rc = 423;
  *reply = 0x00;
  /* es wird etwas abgefragt */
  if (strncasecmp(device, "FB", 2) == 0)
  {
    long int nelem, port;
    nelem = sscanf(parameter, "%ld", &port);
    rc = infoFB(bus, port, reply);
  }
  if (strncasecmp(device, "GL", 2) == 0)
  {
    long addr;
    sscanf(parameter, "%ld", &addr);
    rc = infoGL(bus, addr, reply);
  }
  if (strncasecmp(device, "GA", 2) == 0)
  {
    long addr;
    sscanf(parameter, "%ld", &addr);
    rc = infoGA(bus, addr, reply);
  }
  if (strncasecmp(device, "POWER", 5) == 0)
  {
    rc = infoPower(bus, reply);
  }
  if (strncasecmp(device, "TIME", 4) == 0)
  {
    if (vtime.ratio_x && vtime.ratio_y)
    {
      rc = infoTime(vtime, reply);
    }
  }
  if(strncasecmp(device, "DESCRIPTION", 11) == 0) {
    /* Beschreibungen gibt es deren 2 */
    long int addr;
    char devgrp[10];
    int nelem=-1;
    if(strlen(parameter)>0)
        nelem = sscanf(parameter, "%s %ld", devgrp, &addr);
    if(nelem <= 0) {
        sprintf(reply, "%d DESCRIPTION %s", bus, busses[bus].description);
        rc = SRCP_INFO;
    } else {
        if(strncmp(devgrp, "GL", 2)==0)
            rc = describeGL(bus, addr, reply);
        if(strncmp(devgrp, "GA", 2)==0)
            rc = describeGA(bus, addr, reply);
        if(strncmp(devgrp, "FB", 2)==0)
            rc = describeFB(bus, addr, reply);
        if(strncmp(devgrp, "SESSION", 7)==0)
            rc = describeSESSION(bus, addr, reply);
        if(strncmp(devgrp, "TIME", 4)==0)
            rc = describeTIME(bus, addr, reply);
        if(strncmp(devgrp, "SERVER", 6)==0)
            rc = describeSERVER(bus, addr, reply);
    }
  }
  if(strncasecmp(device, "LOCK", 4) == 0) {
    long int addr;
    char devgrp[10];
    int nelem=-1;
    if(strlen(parameter)>0)
        nelem = sscanf(parameter, "%s %ld", devgrp, &addr);
    if(nelem <= 0) {
        rc = SRCP_LISTTOOSHORT;
    } else {
        rc = SRCP_UNSUPPORTEDDEVICEGROUP;
        if(strncmp(devgrp, "GL", 2)==0)
            rc = getlockGL(bus, addr, 0 /* change to session-id */);
        if(strncmp(devgrp, "GA", 2)==0)
            rc = getlockGA(bus, addr, 0 /* change to session-id */);
    }
  }
  return rc;
}

int handleRESET(int sessionid, int bus, char *device, char *parameter, char *reply) {
  return SRCP_NOTSUPPORTED;
}

int
handleWAIT(int sessionid, int bus, char *device, char *parameter, char *reply)
{
  int rc=SRCP_OK;
  *reply = 0x00;
  /* Wir warten.. */
  if (strncasecmp(device, "FB", 2) == 0)
  {
    long int port, waitvalue, aktvalue, timeout;
    sscanf(parameter, "%ld %ld %ld", &port, &waitvalue, &timeout);
    if (getFB(bus, port) == waitvalue)
    {
      rc = infoFB(bus, port, reply);
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
      }
      while (timeout >= 0 && aktvalue != waitvalue);
      if (timeout < 0)
      {
        rc = 417;
      }
      else
      {
        rc = infoFB(bus, port, reply);
      }
    }
  }
  if (strncasecmp(device, "TIME", 4) == 0)
  {
    unsigned long d, h, m, s;
    sscanf(parameter, "%ld %ld %ld %ld", &d, &h, &m, &s);
    /* es wird nicht gerechnet!, der Zeitfluß nicht gleichmäßig! */
    while (d < vtime.day && h < vtime.hour && m < vtime.min && s < vtime.sec)
    {
      usleep(1000);  /* wir warten 1ms realzeit.. */
    }
    rc = infoTime(vtime, reply);
  }
  return rc;
}

/* negative return code will terminate current session! */
int
handleTERM(int sessionid, int bus, char *device, char *parameter, char *reply)
{
  int rc = 421;
  *reply = 0x00;
  if (strncasecmp(device, "FB", 2) == 0)
  {
  }
  if (strncasecmp(device, "SERVER", 6) == 0)
  {
    if (bus == 0)
    {
      rc = SRCP_OK;
      server_shutdown();
    }
    else
    {
      rc = 422;
    }
  }
  if (strncasecmp(device, "SESSION", 7) == 0)
  {
    if (bus == 0)
    {
      rc = - SRCP_OK;
    }
    else
    {
      rc = 422;
    }
  }
  return rc;
}

int handleINIT(int sessionid, int bus, char *device, char *parameter, char *reply) {
  int rc = SRCP_NOTSUPPORTED;
  if (strncasecmp(device, "GL", 2) == 0)
  {
    long addr, protversion, n_fs, n_func;
    char prot[10];
    sscanf(parameter, "%ld %s %ld %ld %ld", &addr, prot, &protversion, &n_fs, &n_func);
    initGL(bus, addr, prot, protversion, n_fs, n_func);
  }

  if (strncasecmp(device, "TIME", 4) == 0)
  {
    long d, h, m, s, rx, ry;
    sscanf(parameter, "%ld %ld %ld %ld %ld %ld", &d, &h, &m, &s, &rx, &ry);
    rc = setTime(d, h, m, s, rx, ry);  /* prüft auch die Werte! */
  }
  return rc;
}

int
handleVERIFY(int sessionid, int bus, char *device, char *parameter, char *reply)
{
  return SRCP_NOTSUPPORTED;
}

int
handleCHECK(int sessionid, int bus, char *device, char *parameter, char *reply)
{
  return SRCP_NOTSUPPORTED;
}

/***************************************************************
 *  Für jeden Client läuft ein Thread, er macht das
 *  SRCP indem er die Datenstrukturen passend füllt
 *  und die Ergebnisse passend zurückschreibt
 ***************************************************************
 */

int doCmdClient(int Socket, int sessionid)
{
  char line[1024], reply[4095];
  char command[20], devicegroup[20], parameter[900];
  long int bus;
  long int rc;

  syslog(LOG_INFO, "thread >>doCmdClient<< is startet for socket %i", Socket);
  while (1)
  {
    memset(line, 0, sizeof(line));
    if (socket_readline(Socket, line, sizeof(line) - 1) < 0)
    {
      shutdown(Socket, 0);
      close(Socket);
      return -1;
    }
    memset(command, 0, sizeof(command));
    memset(devicegroup, 0, sizeof(devicegroup));
    memset(parameter, 0, sizeof(parameter));
    memset(reply, 0, sizeof(reply));
    sscanf(line, "%s %ld %s %900c", command, &bus, devicegroup, parameter);
    syslog(LOG_INFO, "getting command: %s %ld %s %s", command, bus, devicegroup, parameter);
    rc = 412;
    if (strncasecmp(command, "SET", 3) == 0)
    {
      rc = handleSET(sessionid, bus, devicegroup, parameter, reply);
    }
    if (strncasecmp(command, "GET", 3) == 0)
    {
      rc = handleGET(sessionid, bus, devicegroup, parameter, reply);
    }
    if (strncasecmp(command, "WAIT", 4) == 0)
    {
      rc = handleWAIT(sessionid, bus, devicegroup, parameter, reply);
    }
    if (strncasecmp(command, "INIT", 4) == 0)
    {
      rc = handleINIT(sessionid, bus, devicegroup, parameter, reply);
    }
    if (strncasecmp(command, "TERM", 4) == 0)
    {
      rc = handleTERM(sessionid, bus, devicegroup, parameter, reply);
      if(rc < 0) 
        break;
      rc = abs(rc);
    }
    if (strncasecmp(command, "VERIFY", 6) == 0)
    {
      rc = handleVERIFY(sessionid, bus, devicegroup, parameter, reply);
    }
    if (strncasecmp(command, "RESET", 5) == 0)
    {
      rc = handleRESET(sessionid, bus, devicegroup, parameter, reply);
    }
    if (socket_writereply(Socket, rc, reply) < 0)
    {
      break;
    }
  }
  shutdown(Socket, 2);
  close(Socket);
  return 0;
}

int doInfoClient(int Socket, int sessionidid)
{
  char reply[1000];
  while (1)
  {
    strcpy(reply, "I'm alive");
    socket_writereply(Socket, SRCP_INFO, reply);
    sleep(1);
  }
}
