
/* 
 * Vorliegende Software unterliegt der General Public License, 
 * Version 2, 1991. (c) Matthias Trute, 2000-2001.
 *
 * 04.07.2001 Frank Schmischke
 *            - Feld für Vormerkungen wurde auf 50 reduziert
 *            - Position in Vormerkung ist jetzt unabhängig von der
 *              Decoderadresse
 *            - Prüfungen auf Protokoll fü|r Motorola wurden entfernt, da IB
 *              auch NMRA verarbeitet
 */


#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <pthread.h>
#include <sys/time.h>

#include "config-srcpd.h"
#include "srcp-gl.h"
#include "srcp-error.h"

#include "m605x.h"
#include "ib.h"
#include "loopback.h"

#define QUEUELEN 50

/* aktueller Stand */
static struct _GL gl[MAX_BUSSES];   // aktueller Stand, mehr gibt es nicht

/* Kommandoqueues pro Bus */
static struct _GLSTATE queue[MAX_BUSSES][QUEUELEN];  // Kommandoqueue.
static pthread_mutex_t queue_mutex[MAX_BUSSES];
static int out[MAX_BUSSES], in[MAX_BUSSES];

/* internal functions */
static int queue_len(int bus);
static int queue_isfull(int bus);
 
int get_number_gl (int bus)
{
  return gl[bus].numberOfGl;
}
 
static int initGL_default(int bus, int addr)
{
  switch (busses[bus].type)
  {
    case SERVER_M605X:
        gl[bus].glstate[addr].n_fs = 14;
        gl[bus].glstate[addr].n_func = 1;
        break;
    case SERVER_IB:
        gl[bus].glstate[addr].n_fs =  126;
        gl[bus].glstate[addr].n_func = 1;
        break;
    case SERVER_LOOPBACK:
        gl[bus].glstate[addr].n_fs =  100;
        gl[bus].glstate[addr].n_func = 4;
        break;
    }
    return SRCP_OK;
}

// es gibt Decoder für 14, 27, 28 und 128 FS
// Achtung bei IB alles auf 126 FS bezogen (wenn Ergebnis > 0, dann noch eins aufaddieren)
static int calcspeed(int vs, int vmax, int n_fs)
{
  int rs;
  if(0==vmax)
    return vs;
  if(vs<0)
    vs = 0;
  if(vs>vmax)
    vs = vmax;
  rs = (vs * n_fs) / vmax;
  if((rs==0) && (vs!=0))
    rs = 1;
  return rs;
}

int isInitializedGL(int bus, int addr)
{
   return gl[bus].glstate[addr].n_fs==0;
}

/* Übernehme die neuen Angaben für die Lok, einige wenige Prüfungen */
int queueGL(int bus, int addr, int dir, int speed, int maxspeed, int f,  int f1, int f2, int f3, int f4)
{
  struct timeval akt_time;
  int number_gl = get_number_gl(bus);
  syslog(LOG_INFO, "setGL für %i", addr);
  if ((addr > 0) && (addr <= number_gl) )
  {
    if (!isInitializedGL(bus, addr))
    {
      initGL_default(bus, addr);
    }
    while (queue_isfull(bus))
    {
      usleep(1000);
    }

    pthread_mutex_lock(&queue_mutex[bus]);

    queue[bus][in[bus]].speed     = calcspeed(speed, maxspeed, gl[bus].glstate[addr].n_fs);
    queue[bus][in[bus]].direction = dir;
    queue[bus][in[bus]].funcs     = f1 + (f2 << 1) + (f3 << 2) + (f4 << 3) + (f << 4);
    gettimeofday(&akt_time, NULL);
    queue[bus][in[bus]].tv        = akt_time;
    queue[bus][in[bus]].id        = addr;
    in[bus]++;
    if (in[bus] == QUEUELEN)
      in[bus] = 0;
      
    pthread_mutex_unlock(&queue_mutex[bus]);
  }
  else
  {
    return SRCP_WRONGVALUE;
  }
  return SRCP_OK;
}

int queue_GL_isempty(int bus)
{
  return (in[bus] == out[bus]);
}

static int queue_len(int bus)
{
  if (in[bus] >= out[bus])
    return in[bus] - out[bus];
  else
    return QUEUELEN + in[bus] - out[bus];
}

/* maybe, 1 element in the queue cannot be used.. */
static int queue_isfull(int bus)
{
  return queue_len(bus) >= QUEUELEN - 1;
}

/** liefert nächsten Eintrag und >=0, oder -1 */
int getNextGL(int bus, struct _GLSTATE *l)
{
  if (in[bus] == out[bus])
    return -1;
  *l = queue[bus][out[bus]];
  return out[bus];
}

/** liefert nächsten Eintrag oder -1, setzt fifo pointer neu! */
int unqueueNextGL(int bus, struct _GLSTATE *l)
{
  if (in[bus] == out[bus])
    return -1;

  *l = queue[bus][out[bus]];
  out[bus]++;
  if (out[bus] == QUEUELEN)
    out[bus] = 0;
  return out[bus];
}

int getGL(int bus, int addr, struct _GLSTATE *l)
{
  int number_gl = get_number_gl(bus);
  if(number_gl < 0)
    return SRCP_UNSUPPORTEDDEVICEGROUP;

  if((addr > 0) && (addr <= number_gl))
  {
    *l = gl[bus].glstate[addr];
    return SRCP_OK;
  }
  else
  {
    return SRCP_NODATA;
  }
}

/**

*/
int setGL(int bus, int addr, struct _GLSTATE l)
{
  int number_gl = get_number_gl(bus);
  if(number_gl == 0)
    return SRCP_UNSUPPORTEDDEVICEGROUP;

  if((addr>0) && (addr <= number_gl))
  {
    gl[bus].glstate[addr] = l;
    gettimeofday(&gl[bus].glstate[addr].tv, NULL);
    return SRCP_OK;
  }
  else
  {
    return SRCP_NODATA;
  }
}

int initGL(int bus, int addr, const char *protocol, int protoversion, int n_fs, int n_func)
{
  int number_gl = get_number_gl(bus);
  if(number_gl == 0)
    return SRCP_UNSUPPORTEDDEVICEGROUP;
  if((addr>0) && (addr <= number_gl))
  {
    gl[bus].glstate[addr].n_fs=n_fs;
    gl[bus].glstate[addr].n_func=n_func;
    gl[bus].glstate[addr].protocolversion=protoversion;
    strncpy(gl[bus].glstate[addr].protocol, protocol, sizeof(gl[bus].glstate[addr].protocol));
    return SRCP_OK;
  }
  return SRCP_WRONGVALUE;
}

int describeGL(int bus, int addr, char *msg)
{
  int number_gl = get_number_gl(bus);
  if(number_gl == 0)
    return SRCP_UNSUPPORTEDDEVICEGROUP;
  if((addr>0) && (addr <= number_gl) && (gl[bus].glstate[addr].protocolversion>0) ) {
    sprintf(msg, "%d GL %d %s %d %d %d ",
      bus, addr, gl[bus].glstate[addr].protocol, gl[bus].glstate[addr].protocolversion,
      gl[bus].glstate[addr].n_func,gl[bus].glstate[addr].n_fs);
  }
  else
  {
    strcpy(msg, "");
    return SRCP_NODATA;
  }
  return SRCP_INFO;
}

int infoGL(int bus, int addr, char* msg)
{
  int number_gl = get_number_gl(bus);
  if(number_gl == 0)
    return SRCP_UNSUPPORTEDDEVICEGROUP;
  if((addr>0) && (addr <= number_gl))
  {
    sprintf(msg, "%d GL %d %d %d %d %d %d %d %d %d",
      bus, addr, gl[bus].glstate[addr].direction,
      gl[bus].glstate[addr].speed, gl[bus].glstate[addr].maxspeed,
      (gl[bus].glstate[addr].funcs & 0x10)?1:0,
      (gl[bus].glstate[addr].funcs & 0x01)?1:0,
      (gl[bus].glstate[addr].funcs & 0x02)?1:0,
      (gl[bus].glstate[addr].funcs & 0x04)?1:0,
      (gl[bus].glstate[addr].funcs & 0x08)?1:0);
  }
  else
  {
    return SRCP_NODATA;
  }
  return SRCP_INFO;
}

int lockGL(int bus, int addr, long int sessionid)
{
  return SRCP_NOTSUPPORTED;
}

int getlockGL(int bus, int addr, long int sessionid)
{
  return SRCP_NOTSUPPORTED;
}


int unlockGL(int bus, int addr, long int sessionid)
{
  if(gl[bus].glstate[addr].locked_by==sessionid) 
  {
    gl[bus].glstate[addr].locked_by = 0;
    return SRCP_OK;
  }
  else
  {
    return SRCP_DEVICELOCKED;
  }
}

void unlock_gl_bysessionid(long int sessionid)
{
  int i,j;
  int number;
  syslog(LOG_INFO, "unlock GL by session-ID %ld", sessionid);
  for(i=0; i<MAX_BUSSES; i++)
  {
    number = get_number_gl(i);
    syslog(LOG_INFO, "number of gl for bus %d is %d", i, number);
    for(j=0;j<number; j++)
    {
      if(gl[i].glstate[j].locked_by == sessionid)
      {
        unlockGL(i, j+1, sessionid);
      }
    }
  }
}

int startup_GL(void)
{
  int i;
  for(i=0;i<MAX_BUSSES;i++)
  {
    in[i]=0;
    out[i]=0;
    gl[i].numberOfGl = 0;
    gl[i].glstate = NULL;
    pthread_mutex_init(&queue_mutex[i], NULL);
  }
  return 0;
}

int init_GL(int bus, int number)
{
  int i;

  if (bus >= MAX_BUSSES)
    return 1;

  if(number > 0)
  {
    gl[bus].glstate = malloc(number * sizeof(struct _GLSTATE));
    if (gl[bus].glstate == NULL)
      return 1;
    gl[bus].numberOfGl = number;
    for(i=0;i<number;i++)
    {
      bzero(&gl[bus].glstate[i], sizeof(struct _GLSTATE));
    }
  }
  return 0;
}
