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

#include "config-srcpd.h"
#include "iochannel.h"
#include "srcp-fb.h"
#include "srcp-fb-i8255.h"
#include "srcp-fb-s88.h"
#include "srcp-time.h"
#include "srcp-ga.h"
#include "srcp-gl.h"
#include "srcp-power.h"
#include "srcp-srv.h"
#include "netserver.h"
#include "m605x.h"

extern struct _VTIME vtime;
extern char* WELCOME_MSG;
extern int debuglevel;
extern int working_server;
extern int NUMBER_FB;
extern int NUMBER_GA;
extern int NUMBER_GL;
extern volatile int fb[MAXFBS];
extern volatile struct _GL gl[MAXGLS];  /* aktueller Stand, mehr gibt es nicht */
extern volatile struct _GA ga[MAXGAS];  /* soviele Generic Accessoires gibts             */

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

/***************************************************************
 *  Für jeden Client läuft ein Thread, er macht das
 *  SRCP indem er die Datenstrukturen passend füllt
 *  und die Ergebnisse passend zurückschreibt
 ***************************************************************
 */
void* thr_doCmdClient(void* v)
{
  int socket = (int) v;
  char cmd[256], reply[4095];
  char command[20], spec[20], parameter[256];

  if(write(socket, WELCOME_MSG, strlen(WELCOME_MSG))<0 )
  {
    shutdown(socket, 2);
    close(socket);
    return NULL;
  }
  while(1)
  {
    bzero(reply, sizeof(reply));
    bzero(cmd, sizeof(cmd));
    if(socket_readline(socket, cmd, sizeof(cmd)-1)<0)
    {
      shutdown(socket, 0);
      close(socket);
      return NULL;
    }
    if(debuglevel)
      syslog(LOG_INFO, "CMD: %s", cmd);
    bzero(command, sizeof(command));
    bzero(spec, sizeof(spec));
    bzero(parameter, sizeof(parameter));
    sscanf(cmd, "%s %s %200c", command, spec, parameter);
    if(strncasecmp(command, "LOGOUT", 5) == 0)
    {
      shutdown(socket, 0);
      close(socket);
      return NULL;
    }
    if(strncasecmp(command, "STARTVOLTAGE", 12) == 0)
      setPower(1, "");
    if(strncasecmp(command, "STOPVOLTAGE", 11) == 0)
      setPower(0, "");
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
        long laddr, direction, speed, maxspeed, func, n_fkt, f1, f2, f3, f4;
        int anzparms;
        char protocol[10];
        anzparms = sscanf(parameter, "%s %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld", protocol,
           &laddr, &direction, &speed, &maxspeed, &func, &n_fkt, &f1, &f2, &f3, &f4);
        if(anzparms>5)
        {
          setGL(protocol, laddr, direction, speed, maxspeed, func, n_fkt, f1, f2, f3, f4);
        }
      }
      if(strncasecmp(spec, "GA", 2) == 0)
      {
        long gaddr, port, aktion, delay;
        char prot[10];
        sscanf(parameter, "%s %ld %ld %ld %ld", prot, &gaddr, &port, &aktion, &delay);
        /* Port 0,1; Aktion 0,1 */
        setGA(prot, gaddr, port, aktion, delay);
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
        bzero(msg, sizeof(msg));
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
      strcpy(reply, "INFO -1\n");
      /* es wird etwas abgefragt */
      if(strncasecmp(spec, "FB", 2) == 0)
      {
        long int nelem, port;
        unsigned char proto[127], parm[127], d[4096];
        nelem = sscanf(parameter, "%s %s", proto, parm);
        if(strncmp(parm, "*", 1)==0)
        {
          getFBall(proto, d);
        }
        else
        {
          port = atol(parm);
          infoFB(proto, port, d);
        }
        strcpy(reply, d);
      }
      if(strncasecmp(spec, "GL", 2) == 0)
      {
        long laddr;
        struct _GL l;
        char prot[256];
        sscanf(parameter, "%s %ld", prot, &laddr);
        if(getGL(prot, laddr, &l))
          infoGL(l, reply);
        else
          strcpy(reply, "INFO -2\n");
      }
      if(strncasecmp(spec, "GA", 2) == 0)
      {
        long addr;
        struct _GA a;
        char prot[256];
        sscanf(parameter, "%s %ld", prot, &addr);
        if(getGA(prot, addr, &a))
          infoGA(a, reply);
        else
          strcpy(reply, "INFO -2\n");
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
      strcpy(reply, "INFO -1\n");
      if(strncasecmp(spec, "FB", 2) == 0)
      {
        long int port, waitvalue, aktvalue, timeout;
        unsigned char prot[256], d[256];
        sscanf(parameter, "%s %ld %ld %ld", prot, &port, &waitvalue, &timeout);
        if(getFBone(prot, port)==waitvalue)
        {
          infoFB(prot, port, d);
        }
        else
        {
          /* wir warten 1/100 Sekunden genau */
          timeout *= 100;
          do
          {
            /* fprintf(stderr, "waiting %d (noch %d sekunden)\n", port, timeout); */
            usleep(10000);
            aktvalue = getFBone(prot, port);
            timeout--;
          } while (timeout>=0 && aktvalue!=waitvalue);
          if(timeout<0)
            strcpy(d, "INFO -3\n");
          else
            infoFB(prot, port, d);
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
        if(strncmp(proto, "i8255", 5) == 0)
        {
          if(nelem==3)
            initFB_I8255(portnum, baseport);
        }
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
        char c[256];
        sscanf(parameter, "%s ", c);
        if(strncmp(c, "i8255", 5) == 0)
        {
          termFB_I8255();
        }
      }
    }
    if(strncasecmp(command, "WRITE", 5) == 0)
      strcpy(reply, "INFO -1\n");
    if(strncasecmp(command, "READ", 4) == 0)
      strcpy(reply, "INFO -1\n");
    if(strncasecmp(command, "VERIFY", 6) == 0)
      strcpy(reply, "INFO -1\n");
    if(strlen(reply)>0)
      if(write(socket, reply, strlen(reply)) < 0)
      {
        shutdown(socket, 2);
        close(socket);
        return NULL;
      }
  }
}

/* Der Thread, der den Feedback Port bedient */
/* ziemlich dicht an der Hardware            */
void* thr_doFBClient(void* v)
{
  int socket = (int) v;
  char infoline[2048];
  int fbl[MAXFBS];
  int i,j;
  char Np[256];

  for(i=0; i < NUMBER_FB; i++)
    fbl[i] = 0;
  syslog(LOG_INFO, "Start thr_doFBClient");
  while(1)
  {
    // prüfe laufend die Änderungen und
    // puste die dann raus!
    bzero(infoline, sizeof(infoline));
    // create some info..
    for(i=0; i<NUMBER_FB; i++)
    {
      if(fbl[i] != fb[i])
      {
        // bei diesem Modul hat sich was geändert
        for(j=0; j<16;j++)
        {
          if(_getS88Modulport(fbl[i], j) != _getS88Modulport(fb[i], j))
          {
            if(working_server == SERVER_M605X)
              infoFB("M6051", i*16+j, Np);
            else
              infoFB("IB", i*16+j, Np);
            strcat(infoline, Np);
          }
        }
        fbl[i] = fb[i];
      }
    }
    if(infoline[0])
    {
      if(write(socket, infoline, strlen(infoline)) < 0)
      {
        shutdown(socket, 2);
        close(socket);
        return NULL;
      }
    }
    else
    {
      usleep(2000); /* 2 ms, hält die Prozessorlast niedrig. */
    }
  }
}

/********************************************************
 * Schreibt jede Änderung an einem bekannten Ausrüstungs-
 * teil als INFO an den Client. Hat dazu eine lokale
 * Kopie aller Daten..
 ********************************************************
 */
void* thr_doInfoClient(void *v)
{
  int socket = (int) v;
  char infoline[256];
  int i;

  /* lokale Kopie des Anlagenzustandes */
  struct _GL gll[MAXGLS];
  struct _GA gal[MAXGAS];
  struct _VTIME vtimel;
  int power_lstate = -1;

  bzero(gll, sizeof(gll));
  bzero(gal, sizeof(gal));

  /* Jetzt die initialen Informationen */
  /* gemäß 0.7.3 zusammenstellen       */
  for(i=1; i<=NUMBER_GL; i++)
  {
    gll[i] = gl[i];
    if(gll[i].tv.tv_sec>0)
    {
      infoGL(gll[i], infoline);
      if(write(socket, infoline, strlen(infoline)) < 0)
      {
        shutdown(socket, 2);
        close(socket);
        return NULL;
      }
    }
  }
  for(i=1; i<=NUMBER_GA; i++)
  {
    gal[i] = ga[i];
    if(gal[i].tv[0].tv_sec || gal[i].tv[1].tv_sec)
    {
      /* Zuerst immer action==Null, weil die Märklin Dekoder nun mal so sind */
      if(gal[i].tv[0].tv_sec > gal[i].tv[1].tv_sec)
      {
        sprintf(infoline, "INFO GA %s %d %d %d %ld\nINFO GA %s %d %d %d %ld\n",
        gal[i].prot, gal[i].id, 0, 0, gal[i].activetime,
        gal[i].prot, gal[i].id, 1, gal[i].action, gal[i].activetime);
      }
      else
      {
        if(gal[i].tv[0].tv_sec < gal[i].tv[1].tv_sec)
        {
          sprintf(infoline, "INFO GA %s %d %d %d %ld\nINFO GA %s %d %d %d %ld\n",
          gal[i].prot, gal[i].id, 1, 0, gal[i].activetime,
          gal[i].prot, gal[i].id, 0, gal[i].action, gal[i].activetime);
        }
        else
        {
          // tv_sec is gleich
          if(gal[i].tv[0].tv_usec > gal[i].tv[1].tv_usec)
          {
            sprintf(infoline, "INFO GA %s %d %d %d %ld\nINFO GA %s %d %d %d %ld\n",
            gal[i].prot, gal[i].id, 0, 0, gal[i].activetime,
            gal[i].prot, gal[i].id, 1, gal[i].action, gal[i].activetime);
          }
          else
          {
            sprintf(infoline, "INFO GA %s %d %d %d %ld\nINFO GA %s %d %d %d %ld\n",
            gal[i].prot, gal[i].id, 1, 0, gal[i].activetime,
            gal[i].prot, gal[i].id, 0, gal[i].action, gal[i].activetime);
          }
        }
        if(write(socket, infoline, strlen(infoline)) < 0)
        {
          shutdown(socket, 2);
          close(socket);
          return NULL;
        }
      }
    }
  }
  vtimel = vtime;
  if(vtimel.ratio_y)
  {
    infoTime(vtimel, infoline);
    if(write(socket, infoline, strlen(infoline)) < 0)
    {
      shutdown(socket, 2);
      close(socket);
      return NULL;
    }
  }
  syslog(LOG_INFO, "Start thr_doInfoClient");
  /* Und die Endlosschleife für die    */
  /* Veränderungen legt los            */
  while(1)
  {
    bzero(infoline, sizeof(infoline));
    for(i=1; i<=NUMBER_GL; i++)
    {

      if(!cmpGL(gll[i], gl[i]))
      {
        gll[i] = gl[i];
        infoGL(gll[i], infoline);
        break;
      }
    }
    if(infoline[0] == '\0')
    {
      for(i=1; i<=NUMBER_GA; i++)
      {
        if(!cmpGA(ga[i], gal[i]))
        {
          gal[i]=ga[i];
          infoGA(ga[i], infoline);
          break;
        }
      }
    }
    if(infoline[0] == '\0')
    {
      if((vtime.day != vtimel.day) ||
       (vtime.hour != vtimel.hour) ||
       (vtime.min  != vtimel.min))
      {
        infoTime(vtime, infoline);
        vtimel = vtime;
      }
    }
    if(infoline[0] == '\0')
    {
      if(getPower() != power_lstate)
      {
        infoPower(infoline);
        power_lstate = getPower();
      }
    }
    if(infoline[0])
    {
      if(write(socket, infoline, strlen(infoline)) < 0)
      {
        shutdown(socket, 2);
        close(socket);
        return NULL;
      }
    }
    else
    {
      usleep(2000); /* 2 ms, hält die Prozessorlast niedrig. */
    }
  }
}
