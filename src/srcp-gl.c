
/*
 * Vorliegende Software unterliegt der General Public License,
 * Version 2, 1991. (c) Matthias Trute & the srcpd team, 2000-2003.
 *
 */

#include "stdincludes.h"

#include "config-srcpd.h"
#include "srcp-gl.h"
#include "srcp-error.h"
#include "srcp-info.h"

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

/**
 * isValidGL: checks if a given address could be a valid GL.
 * returns true or false. false, if not all requierements are met.
 */
int isValidGL(busnumber, addr) {
    DBG(busnumber, DBG_INFO, "GL VALID: %d %d (from %d to %d)", busnumber, addr, num_busses, gl[busnumber].numberOfGl-1);
    if (busnumber > 0 &&  		/* in bus 0 GL are not allowed */
	busnumber <= num_busses &&       /* only num_busses are configured */
	gl[busnumber].numberOfGl > 0 && /* number of GL is set */
	addr > 0 &&                     /* address must be greater 0 */
	addr <= gl[busnumber].numberOfGl ) { /* but not more than the maximum address on that bus */
	return 1==1;
    } else {
	return 1==0;
    }
}

/**
 * getMaxAddrGL: returns the maximum Address for GL on the given bus
 * returns: <0: invalid busnumber
            =0: no GL on that bus
	    >0: maximum address
 */
int getMaxAddrGL (int busnumber)
{
  if(busnumber > 0 && busnumber <= num_busses) {
      return gl[busnumber].numberOfGl;
  } else {
      return -1;
  }
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
  // rs = (vs * n_fs) / vmax;
  // for test: rs = ((vs * n_fs) / v_max) + 0.5
  // ==> rs = ((2 * vs * n_fs) + v_max) / (2 * v_max)
  rs = vs * n_fs;	// vs * n_fs
  rs <<= 1;		// 2 * vs * n_fs
  rs += vmax;		// (2 * vs * n_fs) + v_max
  rs /= vmax;		// ((2 * vs * n_fs) + v_max) / v_max
  rs >>= 1;		// ((2 * vs * n_fs) + v_max) / (2 * v_max)
  if ((rs == 0) && (vs != 0))
    rs = 1;

  return rs;
}

/* checks whether a GL is already initialized or not
 * returns false even, if it is an invalid address!
 */
int isInitializedGL(int busnumber, int addr)
{
    if(isValidGL(busnumber,addr) ) {
       return (gl[busnumber].glstate[addr].state == 1);
    } else {
       return 1==0;
    }
}

/* Übernehme die neuen Angaben für die Lok, einige wenige Prüfungen. Lock wird ignoriert!
   Lock wird in den SRCP Routinen beachtet, hier ist das nicht angebracht (Notstop)
*/

int queueGL(int busnumber, int addr, int dir, int speed, int maxspeed, const int f)
{
  struct timeval akt_time;

  if( isValidGL(busnumber, addr) ) {
	if (!isInitializedGL(busnumber, addr))
	{
    	    initGL( busnumber, addr, 'P', 1, 14, 1);
	    DBG(busnumber, DBG_WARN, "GL default init for %d-%d", busnumber, addr);
	}
	if (queue_isfull(busnumber))
	{
    	    DBG(busnumber, DBG_WARN, "GL Command Queue full");
    	    return SRCP_TEMPORARILYPROHIBITED;
	}

	pthread_mutex_lock(&queue_mutex[busnumber]);
	// Protokollbezeichner und sonstige INIT Werte in die Queue kopieren!
	queue[busnumber][in[busnumber]].protocol        = gl[busnumber].glstate[addr].protocol;
	queue[busnumber][in[busnumber]].protocolversion = gl[busnumber].glstate[addr].protocolversion;

	queue[busnumber][in[busnumber]].speed     = calcspeed(speed, maxspeed, gl[busnumber].glstate[addr].n_fs);

	queue[busnumber][in[busnumber]].direction = dir;
	queue[busnumber][in[busnumber]].funcs     = f;
	gettimeofday(&akt_time, NULL);
	queue[busnumber][in[busnumber]].tv        = akt_time;
	queue[busnumber][in[busnumber]].id        = addr;
	in[busnumber]++;
	if (in[busnumber] == QUEUELEN) in[busnumber] = 0;

	pthread_mutex_unlock(&queue_mutex[busnumber]);
	return SRCP_OK;
  } else {
    return SRCP_WRONGVALUE;
  }
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
  if( isValidGL(busnumber, addr) && isInitializedGL(busnumber, addr) )
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
 * setGL is called from the hardware drivers to keep the
 * the data and the info mode informed. It is called from
 * within the SRCP SET Command code.
 * It respects the TERM function.
*/
int setGL(int busnumber, int addr, struct _GLSTATE l)
{
  if( isValidGL(busnumber, addr) )
  {
     char msg[1000];
     gl[busnumber].glstate[addr].direction = l.direction;
     gl[busnumber].glstate[addr].speed = l.speed;
     gl[busnumber].glstate[addr].funcs = l.funcs;
     gettimeofday(&gl[busnumber].glstate[addr].tv, NULL);
     if (gl[busnumber].glstate[addr].state == 2) {
	     sprintf(msg, "%lu.%.3lu 102 INFO %d GL %d",
	       gl[busnumber].glstate[addr].tv.tv_sec, gl[busnumber].glstate[addr].tv.tv_usec/1000,
	       busnumber, addr);
	     bzero(&gl[busnumber].glstate[addr], sizeof(struct _GLSTATE));
     } else {
	     infoGL(busnumber, addr, msg);
     }
     queueInfoMessage(msg);
    return SRCP_OK;
  }
  else
  {
    return SRCP_WRONGVALUE;
  }
}

int initGL(int busnumber, int addr, const char protocol, int protoversion, int n_fs, int n_func)
{
  int rc = SRCP_WRONGVALUE;
  if( isValidGL(busnumber, addr) )
  {
    char msg[1000];
    struct _GLSTATE tgl;
    memset(&tgl, 0, sizeof(tgl));
    rc = SRCP_OK;
    gettimeofday(&tgl.inittime, NULL);
    gettimeofday(&tgl.tv, NULL);
    tgl.n_fs=n_fs;
    tgl.n_func=n_func;
    tgl.protocolversion=protoversion;
    tgl.protocol=protocol;
    tgl.id = addr;
    if(busses[busnumber].init_gl_func && gl[busnumber].glstate[addr].state == 0)
	    rc = (*busses[busnumber].init_gl_func)(&tgl);
    if(rc == SRCP_OK) {
	gl[busnumber].glstate[addr] = tgl;
        gl[busnumber].glstate[addr].state = 1;
	describeGL(busnumber, addr, msg);
        queueInfoMessage(msg);
	queueGL(busnumber, addr, 0, 0, 1, 0);
    }
  } else {
    rc = SRCP_WRONGVALUE;
  }
  return rc;
}


int termGL(busnumber, addr) {
	if(isInitializedGL(busnumber, addr)){
		gl[busnumber].glstate[addr].state = 2;
		queueGL(busnumber, addr, 0, 0, 1, 0);
		return SRCP_OK;
	} else {
		return SRCP_NODATA;
	}

}

/*
 * RESET a GL to its defaults
 */
int resetGL(int busnumber, int addr) {
    if(isValidGL(busnumber, addr) && isInitializedGL(busnumber, addr)){
	queueGL(busnumber, addr, 0, 0, 1, 0);
	return SRCP_OK;
    } else {
	return SRCP_NODATA;
    }
}
int describeGL(int busnumber, int addr, char *msg)
{
  if( isValidGL(busnumber, addr) && isInitializedGL(busnumber, addr) ) {
    sprintf(msg, "%lu.%.3lu 101 INFO %d GL %d %c %d %d %d\n",
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
  int i;
  char *tmp;
  if( isValidGL(busnumber, addr) && isInitializedGL(busnumber, addr) )
  {
    sprintf(msg, "%lu.%.3lu 100 INFO %d GL %d %d %d %d %d",
      gl[busnumber].glstate[addr].tv.tv_sec,
      gl[busnumber].glstate[addr].tv.tv_usec/1000,

      busnumber, addr, gl[busnumber].glstate[addr].direction,
      gl[busnumber].glstate[addr].speed,
      gl[busnumber].glstate[addr].n_fs, (gl[busnumber].glstate[addr].funcs & 0x01)?1:0);

      for(i=1;i<gl[busnumber].glstate[addr].n_func; i++) {
        tmp = malloc(strlen(msg)+100);
        sprintf(tmp, "%s %d", msg, ( (gl[busnumber].glstate[addr].funcs>>i) & 0x01)?1:0);
        strcpy(msg, tmp);
        free(tmp);
      }
      tmp = malloc(strlen(msg)+2);
      sprintf(tmp, "%s\n", msg);
      strcpy(msg, tmp);
      free(tmp);

  }
  else
  {
    return SRCP_NODATA;
  }
  return SRCP_INFO;
}

/* has to use a semaphore, must be atomare! */
int lockGL(int busnumber, int addr, long int duration, long int sessionid)
{
  char msg[256];
  if (isValidGL(busnumber, addr) && isInitializedGL(busnumber, addr) ) {
    if( gl[busnumber].glstate[addr].locked_by==sessionid || gl[busnumber].glstate[addr].locked_by==0) {
        gl[busnumber].glstate[addr].locked_by=sessionid;
        gl[busnumber].glstate[addr].lockduration = duration;
        gettimeofday(& gl[busnumber].glstate[addr].locktime, NULL);
        describeLOCKGL(busnumber, addr, msg);
        queueInfoMessage(msg);
        return SRCP_OK;
    } else {
        return SRCP_DEVICELOCKED;
    }
  } else {
    return SRCP_WRONGVALUE;
  }
}

int getlockGL(int busnumber, int addr, long int *session_id)
{
  if (isValidGL(busnumber, addr) && isInitializedGL(busnumber, addr) ) {

    *session_id = gl[busnumber].glstate[addr].locked_by;
    return SRCP_OK;
  } else {
    return SRCP_WRONGVALUE;
  }
}

int describeLOCKGL(int bus, int addr, char *reply) {
  if (isValidGL(bus, addr) && isInitializedGL(bus, addr) ) {

    sprintf(reply, "%lu.%.3lu 100 INFO %d LOCK GL %d %ld %ld\n",
          gl[bus].glstate[addr].locktime.tv_sec,
          gl[bus].glstate[addr].locktime.tv_usec/1000,
          bus, addr, gl[bus].glstate[addr].lockduration, gl[bus].glstate[addr].locked_by);
    return SRCP_OK;
  } else {
    return SRCP_WRONGVALUE;
  }
}

int unlockGL(int busnumber, int addr, long int sessionid)
{
  if (isValidGL(busnumber, addr) && isInitializedGL(busnumber, addr) ) {

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
  } else {
    return SRCP_WRONGVALUE;
  }
}

/**
 * called when a session is terminating
 */
void unlock_gl_bysessionid(long int sessionid)
{
  int i,j;
  int number;
  DBG(0, DBG_INFO, "unlock GL by session-ID %ld", sessionid);
  for(i=0; i<=num_busses; i++)
  {
    number = getMaxAddrGL(i);
    for(j=1;j<=number; j++)
    {
      if(gl[i].glstate[j].locked_by == sessionid)
      {
        unlockGL(i, j, sessionid);
      }
    }
  }
}

/**
 * called once per second to unlock
 */
void unlock_gl_bytime(void)
{
  int i,j;
  int number;
  for(i=0; i<=num_busses; i++)
  {
    number = getMaxAddrGL(i);
    for(j=1;j<=number; j++)
    {
      if(gl[i].glstate[j].lockduration>0 && gl[i].glstate[j].lockduration -- == 1)
      {
        unlockGL(i, j, gl[i].glstate[j].locked_by);
      }
    }
  }
}

/**
 * First initialisation after program startup
 */
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

/**
 * allocates memory to hold all the data
 * called from the configuration routines
 */
int init_GL(int busnumber, int number)
{
  int i;
    DBG(DBG_WARN, busnumber, "INIT GL: %d", number);
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
