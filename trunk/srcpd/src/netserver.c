
/* 
 * Vorliegende Software unterliegt der General Public License, 
 * Version 2, 1991. (c) Matthias Trute, 2000-2001.
 *
 * 2002-12-29 Manuel Borchers
 *            handleSET():
 *            - getlockGL changed to getlockGA in the GA-command-processing
 *            - if the device is locked, return code is now set to
 *              SRCP_DEVICELOCKED
 */

#include "stdincludes.h"

#include "config-srcpd.h"
#include "srcp-error.h"
#include "srcp-fb.h"
#include "srcp-time.h"
#include "srcp-ga.h"
#include "srcp-gl.h"
#include "srcp-power.h"
#include "srcp-sm.h"
#include "srcp-srv.h"
#include "srcp-session.h"
#include "srcp-info.h"
#include "netserver.h"
#include "m605x.h"
#include "threads.h"

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
int socket_writereply(int Socket, int srcpcode, const char *line, struct timeval *timestamp)
{
  char *buf=NULL;
  int status;
  if(srcpcode == SRCP_INFO)
  {
    buf=malloc(strlen(line));
    strcpy(buf, line);
  }
  else
  {
    char buf2[511];
    srcp_fmt_msg(srcpcode, buf2);
    buf=malloc(strlen(buf2)+50+strlen(line));
    sprintf(buf, "%ld.%ld %s %s\n", timestamp->tv_sec, timestamp->tv_usec / 1000, buf2, line);
  }
  DBG(0, DBG_DEBUG, "socket %d, write %s", Socket, buf);
  status = write(Socket, buf, strlen(buf));
  DBG(0, DBG_DEBUG, "status from write: %d", status);
  free(buf);
  return status;
}

/**
 * Shakehand phase.
 *
 */

long int SessionID = 1;
pthread_mutex_t SessionID_mut = PTHREAD_MUTEX_INITIALIZER;

void *thr_doClient(void *v)
{
  struct timeval akt_time;

  int Socket = (int) v;
  char line[1000], cmd[1000], setcmd[1000], parameter[1000], reply[1000];
  int mode = COMMAND;
  long int sessionid, rc;
  /* drop root permission for this thread */
  change_privileges(0);
  
  if (write(Socket, WELCOME_MSG, strlen(WELCOME_MSG)) < 0)
  {
    shutdown(Socket, 2);
    close(Socket);
    return NULL;
  }
  while (1)
  {
    rc = SRCP_HS_NODATA;
    reply[0] = 0x00;
    memset(cmd, 0, sizeof(cmd));
    if (socket_readline(Socket, line, sizeof(line) - 1) < 0)
    {
      shutdown(Socket, 0);
      close(Socket);
      return NULL;
    }
    rc = sscanf(line, "%s %1000c", cmd, parameter);
    if (rc >0 && strncasecmp(cmd, "GO", 2) == 0)
    {
      pthread_mutex_lock(&SessionID_mut);
      sessionid = SessionID++;
      gettimeofday(&akt_time, NULL);
      pthread_mutex_unlock(&SessionID_mut);
      sprintf(reply, "GO %ld", sessionid);
      if (socket_writereply(Socket, SRCP_OK_GO, reply, &akt_time) < 0)
      {
        shutdown(Socket, 2);
        close(Socket);
        return NULL;
      }
      start_session(sessionid, mode);
      switch (mode) {
        case COMMAND:
                      rc = doCmdClient(Socket, sessionid);
                      break;
        case INFO:
                      rc = doInfoClient(Socket, sessionid);
                      break;
      }
      stop_session(sessionid);
      return NULL;
    }
    if (strncasecmp(cmd, "SET", 3) == 0)
    {
      char p[1000];
      sscanf(parameter, "%s %1000c", setcmd, p);
      if (strncasecmp(setcmd, "CONNECTIONMODE", 3) == 0)
      {
        rc = SRCP_HS_WRONGCONNMODE;
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
        rc = SRCP_HS_WRONGPROTOCOL;
        if (strncasecmp(p, "SRCP 0.8", 8) == 0)
        {
          strcpy(reply, "PROTOCOL SRCP");
          rc = SRCP_OK_PROTOCOL;
        }
      }
    }
    gettimeofday(&akt_time, NULL);
    socket_writereply(Socket, rc, reply, &akt_time);
  }
}

/**
 * Core SRCP Commands
 * handle all aspects of the command for all commands
 */

/**
 * SET
 */
int handleSET(int sessionid, int bus, char *device, char *parameter, char *reply)
{
  int rc = SRCP_UNSUPPORTEDDEVICEGROUP;
  *reply = 0x00;
  if (strncasecmp(device, "GL", 2) == 0)
  {
    long laddr, direction, speed, maxspeed, func, f1, f2, f3, f4;
    int anzparms;
    anzparms = sscanf(parameter, "%ld %ld %ld %ld %ld %ld %ld %ld %ld",
       &laddr, &direction, &speed, &maxspeed, &func, &f1, &f2, &f3, &f4);
    if (anzparms > 5)
    {
      long int lockid;
      /* Only if not locked or emergency stop !! */
      getlockGL(bus, laddr, &lockid);
      if(lockid==0 || lockid==sessionid || direction==2) {
          rc = queueGL(bus, laddr, direction, speed, maxspeed, func, f1, f2, f3, f4);
      }
    }
  }
	
  if (strncasecmp(device, "GA", 2) == 0)
  {
    long gaddr, port, aktion, delay;
    long int lockid;
		
    sscanf(parameter, "%ld %ld %ld %ld", &gaddr, &port, &aktion, &delay);
    /* Port 0,1; Aktion 0,1 */
    /* Only if not locked!! */
    getlockGA(bus, gaddr, &lockid);
		
		if(lockid==0 || lockid==sessionid) {
			rc = queueGA(bus, gaddr, port, aktion, delay);
		} else {
			rc = SRCP_DEVICELOCKED;
		}
		
  }
  if (strncasecmp(device, "SM", 2) == 0)
  {
    long addr, value1, value2, value3;
    int type;
    char *ctype;

    ctype = malloc(1000);
    sscanf(parameter, "%ld %s %ld %ld %ld", &addr, ctype, &value1, &value2, &value3);
    type = CV;
    if (strcasecmp(ctype, "REG") == 0)
      type = REGISTER;
    else
      if (strcasecmp(ctype, "CVBIT") == 0)
        type = CV_BIT;
    free(ctype);
    if (type == CV_BIT)
      rc = infoSM(bus, SET, type, addr, value1, value2, value3, reply);
    else
      rc = infoSM(bus, SET, type, addr, value1, 0, value2, reply);
  }
  if (strncasecmp(device, "TIME", 4) == 0)
  {
    long d, h, m, s;
    sscanf(parameter, "%ld %ld %ld %ld", &d, &h, &m, &s);
    rc = setTime(d, h, m, s);
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
            rc = lockGA(bus, addr, sessionid);
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
  int rc = SRCP_UNSUPPORTEDDEVICEGROUP;
  *reply = 0x00;
  if (strncasecmp(device, "FB", 2) == 0)
  {
    long int nelem, port;
    nelem = sscanf(parameter, "%ld", &port);
    if(nelem == 1)
      rc = infoFB(bus, port, reply);
    else {
      rc = SRCP_LISTTOOLONG;
    }
      
  }
  if (strncasecmp(device, "GL", 2) == 0)
  {
    long nelem, addr;
    nelem = sscanf(parameter, "%ld", &addr);
    if(nelem == 1)
      rc = infoGL(bus, addr, reply);
    else
      rc = SRCP_LISTTOOLONG;
  }
  if (strncasecmp(device, "GA", 2) == 0)
  {
    long addr, port, nelem;
    nelem = sscanf(parameter, "%ld %ld", &addr, &port);
    switch (nelem) {
      case 0:
      case 1:
        rc = SRCP_LISTTOOSHORT;
        break;
      case 2:
        rc = infoGA(bus, addr, port, reply);
        break;
      default:
        rc = SRCP_LISTTOOLONG;
      }
  }
  if (strncasecmp(device, "SM", 2) == 0)
  {
    long addr, value1, value2;
    int type;
    char *ctype;

    ctype = malloc(1000);
    sscanf(parameter, "%ld %s %ld %ld", &addr, ctype, &value1, &value2);
    type = CV;
    if (strcasecmp(ctype, "REG") == 0)
      type = REGISTER;
    else
      if (strcasecmp(ctype, "CVBIT") == 0)
        type = CV_BIT;
    free(ctype);
    rc = infoSM(bus, GET, type, addr, value1, value2, 0, reply);
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
    else
    {
      rc = SRCP_NODATA;
    }
  }
  if(strncasecmp(device, "DESCRIPTION", 11) == 0) 
  {
    /* Beschreibungen gibt es deren 2 */
    long int addr;
    char devgrp[10];
    int nelem=-1;
		
		struct timeval tv;
		
		gettimeofday(&tv, NULL);
		
    if(strlen(parameter)>0)
        nelem = sscanf(parameter, "%s %ld", devgrp, &addr);
    if(nelem <= 0) 
    {
      sprintf(reply, "%ld.%ld 100 INFO %d DESCRIPTION %s\n",
				tv.tv_sec, tv.tv_usec/1000,
				bus, busses[bus].description);
      rc = SRCP_INFO;
    } 
    else 
    {
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
        if(strncmp(devgrp, "GL", 2)==0) {
            long int session_id;
            rc = getlockGL(bus, addr, &session_id);
            if (rc==SRCP_OK)
                sprintf(reply, "%d GL %ld %ld", bus, addr, session_id);
        }
        if(strncmp(devgrp, "GA", 2)==0) {
            long int session_id;
            rc = getlockGA(bus, addr, &session_id);
            if (rc==SRCP_OK)
                sprintf(reply, "%d GA %ld %ld", bus, addr, session_id);
        }
    }
  }
  return rc;
}

int handleRESET(int sessionid, int bus, char *device, char *parameter, char *reply) {
  return SRCP_UNSUPPORTEDOPERATION;
}

int handleWAIT(int sessionid, int bus, char *device, char *parameter, char *reply)
{
  struct timeval time;
  int rc = SRCP_UNSUPPORTEDDEVICEGROUP;
  *reply = 0x00;
  /* check, if bus has FB's */
  if (strncasecmp(device, "FB", 2) == 0)
  {
    long int port,  timeout;
    int value, waitvalue, aktvalue;
    sscanf(parameter, "%ld %d %ld", &port, &waitvalue, &timeout);
    
    if (getFB(bus, port, &time, &value) == SRCP_OK && value==waitvalue)
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
        getFB(bus, port, &time, &aktvalue);
        timeout--;
      }
      while (timeout >= 0 && aktvalue != waitvalue);
      if (timeout < 0)
      {
        rc = SRCP_TIMEOUT;
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
    /* es wird nicht gerechnet!, der Zeitflu� ist nicht gleichm��ig! */
    while (d < vtime.day && h < vtime.hour && m < vtime.min && s < vtime.sec)
    {
      usleep(1000);  /* wir warten 1ms realzeit.. */
    }
    rc = infoTime(vtime, reply);
  }
  return rc;
}

/* negative return code will terminate current session! */
int handleTERM(int sessionid, int bus, char *device, char *parameter, char *reply)
{
  int rc = SRCP_UNSUPPORTEDDEVICE;
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
      rc = SRCP_UNSUPPORTEDDEVICEGROUP;
    }
  }
  if (strncasecmp(device, "SESSION", 7) == 0)
  {
    if (bus == 0)
    {
      long int termsession = 0;
      int nelem = 0;
      if(strlen(parameter) > 0)
        nelem = sscanf(parameter, "%ld", &termsession);
      if(nelem <= 0)
        termsession = 0;
      rc = termSESSION(bus, sessionid, termsession, reply);	
    }
    else
    {
      rc = SRCP_UNSUPPORTEDDEVICEGROUP;
    }
  }
  return rc;
}

int handleINIT(int sessionid, int bus, char *device, char *parameter, char *reply) {
  int rc = SRCP_UNSUPPORTEDDEVICEGROUP;
  if (strncasecmp(device, "GL", 2) == 0)
  {
    long addr, protversion, n_fs, n_func;
    char prot[10];
    sscanf(parameter, "%ld %s %ld %ld %ld", &addr, prot, &protversion, &n_fs, &n_func);
    initGL(bus, addr, prot, protversion, n_fs, n_func);
  }

  if (strncasecmp(device, "TIME", 4) == 0)
  {
    long rx, ry;
    sscanf(parameter, "%ld %ld", &rx, &ry);
    rc = initTime(rx, ry);  /* pr�ft auch die Werte! */
  }
  return rc;
}

int handleVERIFY(int sessionid, int bus, char *device, char *parameter, char *reply)
{
  return SRCP_UNSUPPORTEDOPERATION;
}

int handleCHECK(int sessionid, int bus, char *device, char *parameter, char *reply)
{
  return SRCP_UNSUPPORTEDOPERATION;
}

/***************************************************************
 *  F�r jeden Client l�uft ein Thread, er macht das
 *  SRCP indem er die Datenstrukturen passend f�llt
 *  und die Ergebnisse passend zur�ckschreibt
 ***************************************************************
 */

int doCmdClient(int Socket, int sessionid)
{
  char line[1024], reply[4095];
  char command[20], devicegroup[20], parameter[900];
  long int bus;
  long int rc;
  struct timeval akt_time;

  DBG(0, DBG_INFO, "thread >>doCmdClient<< is startet for socket %i", Socket);
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
    DBG(bus, DBG_INFO, "getting command from session %ld: %s %s %s", sessionid, command,  devicegroup, parameter);
    rc = SRCP_UNKNOWNCOMMAND;
    reply[0] = 0x00;

    if((bus >= 0) && (bus < MAX_BUSSES))
    {
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
    }
    gettimeofday(&akt_time, NULL);
    if (socket_writereply(Socket, rc, reply, &akt_time) < 0)
    {
      break;
    }
  }
  shutdown(Socket, 2);
  close(Socket);
  return 0;
}
