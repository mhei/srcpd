
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

#include "stdincludes.h"

#include "config-srcpd.h"
#include "srcp-gl.h"
#include "srcp-error.h"
#include "srcp-info.h"

#include "m605x.h"
#include "ib.h"
#include "loopback.h"

#define QUEUELEN 50

/* aktueller Stand */
static struct _GL gl[MAX_BUSSES]; 

/* Kommandoqueues pro Bus */
static struct _GLSTATE queue[MAX_BUSSES][QUEUELEN]; 
static pthread_mutex_t queue_mutex[MAX_BUSSES];
/* Schreibposition für die Writer der Queue */
static int out[MAX_BUSSES], in[MAX_BUSSES];

/* internal functions */
static int queue_len(int busnumber);
static int queue_isfull(int busnumber);

int get_number_gl (int busnumber)
{
  return gl[busnumber].numberOfGl;
}

static int initGL_default(int busnumber, int addr)
{
  DBG(busnumber, DBG_INFO, "GL default init for %d", addr);
  switch (busses[busnumber].type)
  {
    case SERVER_M605X:
        initGL(busnumber, addr, "M", 2, 14, 5);
        break;
    case SERVER_IB:
        gl[busnumber].glstate[addr].n_fs =  126;
        gl[busnumber].glstate[addr].n_func = 1;
        break;
    case SERVER_LOOPBACK:
        gl[busnumber].glstate[addr].n_fs =  100;
        gl[busnumber].glstate[addr].n_func = 4;
        break;
    }
    return SRCP_OK;
}

// es gibt Decoder für 14, 27, 28 und 128 FS
static int calcspeed(int vs, int vmax, int n_fs)
{
  int rs;

  if (vmax == 0)
    return vs;
  if (vs < 0)
    vs = 0;
  if (vs > vmax)
    vs = vmax;
  rs = (vs * n_fs) / vmax;
  if ((rs == 0) && (vs != 0))
    rs = 1;

  return rs;
}

int isInitializedGL(int busnumber, int addr)
{
   return (gl[busnumber].glstate[addr].n_fs != 0);
}

/* Übernehme die neuen Angaben für die Lok, einige wenige Prüfungen. Lock wird ignoriert!
   Lock wird in den SRCP Routinen beachtet, hier ist das nicht angebracht (Notstop)
*/
int queueGL(int busnumber, int addr, int dir, int speed, int maxspeed, int f,  int f1, int f2, int f3, int f4)
{
  struct timeval akt_time;
  int number_gl = get_number_gl(busnumber);
  if ((addr > 0) && (addr <= number_gl) )
  {
    if (!isInitializedGL(busnumber, addr))
    {
      initGL_default(busnumber, addr);
    }
    while (queue_isfull(busnumber))
    {
      DBG(busnumber, DBG_WARN, "GL Command Queue full");
      usleep(1000);
    }

    pthread_mutex_lock(&queue_mutex[busnumber]);

    queue[busnumber][in[busnumber]].speed     = calcspeed(speed, maxspeed, gl[busnumber].glstate[addr].n_fs);
    queue[busnumber][in[busnumber]].direction = dir;
    queue[busnumber][in[busnumber]].funcs     = f1 + (f2 << 1) + (f3 << 2) + (f4 << 3) + (f << 4);
    gettimeofday(&akt_time, NULL);
    queue[busnumber][in[busnumber]].tv        = akt_time;
    queue[busnumber][in[busnumber]].id        = addr;
    in[busnumber]++;
    if (in[busnumber] == QUEUELEN)
      in[busnumber] = 0;

    pthread_mutex_unlock(&queue_mutex[busnumber]);
  }
  else
  {
    return SRCP_WRONGVALUE;
  }
  return SRCP_OK;
}

int queue_GL_isempty(int busnumber)
{
  return (in[busnumber] == out[busnumber]);
}

static int queue_len(int busnumber)
{
  if (in[busnumber] >= out[busnumber])
    return in[busnumber] - out[busnumber];
  else
    return QUEUELEN + in[busnumber] - out[busnumber];
}

/* maybe, 1 element in the queue cannot be used.. */
static int queue_isfull(int busnumber)
{
  return queue_len(busnumber) >= QUEUELEN - 1;
}

/** liefert nächsten Eintrag und >=0, oder -1 */
int getNextGL(int busnumber, struct _GLSTATE *l)
{
  if (in[busnumber] == out[busnumber])
    return -1;
  *l = queue[busnumber][out[busnumber]];
  return out[busnumber];
}

/** liefert nächsten Eintrag oder -1, setzt fifo pointer neu! */
int unqueueNextGL(int busnumber, struct _GLSTATE *l)
{
  if (in[busnumber] == out[busnumber])
    return -1;

  *l = queue[busnumber][out[busnumber]];
  out[busnumber]++;
  if (out[busnumber] == QUEUELEN)
    out[busnumber] = 0;
  return out[busnumber];
}

int getGL(int busnumber, int addr, struct _GLSTATE *l)
{
  int number_gl = get_number_gl(busnumber);
  if(number_gl <= 0)
    return SRCP_UNSUPPORTEDDEVICEGROUP;

  if((addr > 0) && (addr <= number_gl))
  {
    *l = gl[busnumber].glstate[addr];
    return SRCP_OK;
  }
  else
  {
    return SRCP_NODATA;
  }
}

/**
  info: update from hardware, not from network; queue into info mode!
*/
int setGL(int busnumber, int addr, struct _GLSTATE l, int info)
{
  int number_gl = get_number_gl(busnumber);
  if(number_gl <= 0)
    return SRCP_UNSUPPORTEDDEVICEGROUP;

  if((addr>0) && (addr <= number_gl))
  {
     gl[busnumber].glstate[addr].direction = l.direction;
     gl[busnumber].glstate[addr].speed = l.speed;
     gl[busnumber].glstate[addr].funcs = l.funcs;
     gettimeofday(&gl[busnumber].glstate[addr].tv, NULL);
     if (info == 1) {
       char msg[1000];
       infoGL(busnumber, addr, msg);
       queueInfoMessage(msg);
     }
    return SRCP_OK;
  }
  else
  {
    return SRCP_NODATA;
  }
}

int initGL(int busnumber, int addr, const char *protocol, int protoversion, int n_fs, int n_func)
{
  int number_gl = get_number_gl(busnumber);
  if(number_gl <= 0)
    return SRCP_UNSUPPORTEDDEVICEGROUP;
  if((addr>0) && (addr <= number_gl))
  {
    char msg[1000];
    gettimeofday(&gl[busnumber].glstate[addr].inittime, NULL);
    gl[busnumber].glstate[addr].n_fs=n_fs;
    gl[busnumber].glstate[addr].n_func=n_func;
    gl[busnumber].glstate[addr].protocolversion=protoversion;
    strncpy(gl[busnumber].glstate[addr].protocol, protocol, sizeof(gl[busnumber].glstate[addr].protocol));
    describeGL(busnumber, addr, msg);
    queueInfoMessage(msg);
    return SRCP_OK;
  }
  return SRCP_WRONGVALUE;
}

int describeGL(int busnumber, int addr, char *msg)
{
  int number_gl = get_number_gl(busnumber);
  if(number_gl <= 0)
    return SRCP_UNSUPPORTEDDEVICEGROUP;
  if((addr>0) && (addr <= number_gl) && (gl[busnumber].glstate[addr].protocolversion>0) ) {
    sprintf(msg, "%lu.%.3lu 101 INFO %d GL %d %s %d %d %d\n",
      gl[busnumber].glstate[addr].inittime.tv_sec, gl[busnumber].glstate[addr].inittime.tv_usec/1000,
      busnumber, addr, gl[busnumber].glstate[addr].protocol, gl[busnumber].glstate[addr].protocolversion,
      gl[busnumber].glstate[addr].n_func,gl[busnumber].glstate[addr].n_fs);
  }
  else
  {
    strcpy(msg, "");
    return SRCP_NODATA;
  }
  return SRCP_INFO;
}

int infoGL(int busnumber, int addr, char* msg)
{
  int number_gl = get_number_gl(busnumber);
  if(number_gl <= 0)
    return SRCP_UNSUPPORTEDDEVICEGROUP;
  if((addr>0) && (addr <= number_gl))
  {
    sprintf(msg, "%lu.%.3lu 100 INFO %d GL %d %d %d %d %d %d %d %d %d\n",
      gl[busnumber].glstate[addr].tv.tv_sec,
      gl[busnumber].glstate[addr].tv.tv_usec/1000,
          
      busnumber, addr, gl[busnumber].glstate[addr].direction,
      gl[busnumber].glstate[addr].speed,
      gl[busnumber].glstate[addr].n_fs, 
      (gl[busnumber].glstate[addr].funcs & 0x10)?1:0,
      (gl[busnumber].glstate[addr].funcs & 0x01)?1:0,
      (gl[busnumber].glstate[addr].funcs & 0x02)?1:0,
      (gl[busnumber].glstate[addr].funcs & 0x04)?1:0,
      (gl[busnumber].glstate[addr].funcs & 0x08)?1:0);
  }
  else
  {
    return SRCP_NODATA;
  }
  return SRCP_INFO;
}

/* has to use a semaphore, must be atomare! */
int lockGL(int busnumber, int addr, long int sessionid)
{
  if(gl[busnumber].glstate[addr].locked_by==sessionid)
  {
    return SRCP_OK;
  }
  else
  {
    if(gl[busnumber].glstate[addr].locked_by==0) {
        char msg[256];
        gl[busnumber].glstate[addr].locked_by=sessionid;
        gettimeofday(& gl[busnumber].glstate[addr].locktime, NULL);
        describeLOCKGL(busnumber, addr, msg);
        queueInfoMessage(msg);
        return SRCP_OK;
    } else {
        return SRCP_DEVICELOCKED;
    }
  }
  /* unreached */
}

int getlockGL(int busnumber, int addr, long int *session_id)
{
  *session_id = gl[busnumber].glstate[addr].locked_by;
  return SRCP_OK;
}

int describeLOCKGL(int bus, int addr, char *reply) {
    sprintf(reply, "%lu.%.3lu 100 INFO %d LOCK GL %d %ld\n",
          gl[bus].glstate[addr].locktime.tv_sec,
          gl[bus].glstate[addr].locktime.tv_usec/1000,
          bus, addr, gl[bus].glstate[addr].locked_by);
    return SRCP_OK;
}

int unlockGL(int busnumber, int addr, long int sessionid)
{
  if(gl[busnumber].glstate[addr].locked_by==sessionid || gl[busnumber].glstate[addr].locked_by==0)
  { char msg[256];
    gl[busnumber].glstate[addr].locked_by = 0;
    gettimeofday(& gl[busnumber].glstate[addr].locktime, NULL);
    sprintf(msg, "%lu.%.3lu 102 INFO %d LOCK GL %d %ld\n",
      gl[busnumber].glstate[addr].locktime.tv_sec,
      gl[busnumber].glstate[addr].locktime.tv_usec/1000,
      busnumber, addr, sessionid);
    queueInfoMessage(msg);
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
  DBG(0, DBG_INFO, "unlock GL by session-ID %ld", sessionid);
  for(i=0; i<=num_busses; i++)
  {
    number = get_number_gl(i);
    DBG(i, DBG_DEBUG, "number of gl for busnumber %d is %d", i, number);
    for(j=1;j<number; j++)
    {
      if(gl[i].glstate[j].locked_by == sessionid)
      {
        unlockGL(i, j, sessionid);
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

int init_GL(int busnumber, int number)
{
  int i;

  if (busnumber >= MAX_BUSSES)
    return 1;

  if(number > 0)
  {
    gl[busnumber].glstate = malloc( (number+1) * sizeof(struct _GLSTATE));
    if (gl[busnumber].glstate == NULL)
      return 1;
    gl[busnumber].numberOfGl = number;
    for(i=0;i<=number;i++)
    {
      bzero(&gl[busnumber].glstate[i], sizeof(struct _GLSTATE));
    }
  }
  return 0;
}
