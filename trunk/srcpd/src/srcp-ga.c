
/* 
 * Vorliegende Software unterliegt der General Public License, 
 * Version 2, 1991. (c) Matthias Trute, 2000-2001.
 *
 * This software is published under the restrictions of the 
 * GNU License Version2
 *
 * Ostern 2002, Matthias Trute
 *            - echte Kommandoqueue anstelle der Vormerkungen.
 *
 * 04.07.2001 Frank Schmischke
 *            - Feld für Vormerkungen wurde auf 50 reduziert
 *            - Position im Feld für Vormerkung ist jetzt unabhängig von der
 *              Decoderadresse
 */

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <sys/time.h>
#include <pthread.h>

#include "config-srcpd.h"
#include "srcp-ga.h"
#include "srcp-error.h"
#include "srcp-info.h"

#include "m605x.h"
#include "ib.h"
#include "loopback.h"

#define QUEUELEN 500

/* aktueller Stand */
volatile struct _GA ga[MAX_BUSSES];   // aktueller Stand, mehr gibt es nicht

/* Kommandoqueues pro Bus */
static struct _GASTATE queue[MAX_BUSSES][QUEUELEN];  // Kommandoqueue.
static pthread_mutex_t queue_mutex[MAX_BUSSES];
static int out[MAX_BUSSES], in[MAX_BUSSES];

/* internal functions */
static int queue_len(int busnumber);
static int queue_isfull(int busnumber);

int get_number_ga (int busnumber)
{
  return ga[busnumber].numberOfGa;
}

/* Übernehme die neuen Angaben für die Weiche, einige wenige Prüfungen */
int queueGA(int busnumber, int addr, int port, int action, long int activetime)
{
  struct timeval akt_time;
  int number_ga = get_number_ga(busnumber);

  if ((addr > 0) && (addr <= number_ga) )
  {
    while (queue_isfull(busnumber))
    {
      usleep(1000);
    }

    pthread_mutex_lock(&queue_mutex[busnumber]);
    queue[busnumber][in[busnumber]].action = action;
    queue[busnumber][in[busnumber]].port = port;
    queue[busnumber][in[busnumber]].activetime = activetime;
    gettimeofday(&akt_time, NULL);
    queue[busnumber][in[busnumber]].tv[port] = akt_time;
    queue[busnumber][in[busnumber]].id = addr;
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

int queue_GA_isempty(int busnumber)
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
int getNextGA(int busnumber, struct _GASTATE *a)
{
  if (in[busnumber] == out[busnumber])
    return -1;
  *a = queue[busnumber][out[busnumber]];
  return out[busnumber];
}

/** liefert nächsten Eintrag oder -1, setzt fifo pointer neu! */
int unqueueNextGA(int busnumber, struct _GASTATE *a)
{
  if (in[busnumber] == out[busnumber])
    return -1;

  *a = queue[busnumber][out[busnumber]];
  out[busnumber]++;
  if (out[busnumber] == QUEUELEN)
    out[busnumber] = 0;
  return out[busnumber];
}

int getGA(int busnumber, int addr, struct _GASTATE *a)
{
  int number_ga = get_number_ga(busnumber);

  if((addr>0) && (addr <= number_ga))
  {
    *a = ga[busnumber].gastate[addr];
    return SRCP_OK;
  }
  else
  {
    return SRCP_NODATA;
  }
}

static int initGA_default(int busnumber, int addr)
{
  int number_ga = get_number_ga(busnumber);

  if((addr > 0) && (addr <= number_ga))
  {
    strcpy(ga[busnumber].gastate[addr].protocol, "P");
    return SRCP_OK;
  }
  else
  {
    return SRCP_UNSUPPORTEDDEVICE;
  }
}

int isInitializedGA(int busnumber, int addr)
{
  return ga[busnumber].gastate[addr].protocol == NULL;
}

/* ********************
     SRCP Kommandos
*/
int setGA(int busnumber, int addr, struct _GASTATE a, int info)
{
  int number_ga = get_number_ga(busnumber);

  if((addr>0) && (addr <= number_ga))
  {
    if(!isInitializedGA(busnumber, addr))
      initGA_default(busnumber, addr);
    ga[busnumber].gastate[addr] = a;
    gettimeofday(&ga[busnumber].gastate[addr].tv[ga[busnumber].gastate[addr].port], NULL);
    if (info == 1)
      queueInfoGA(busnumber, addr, ga[busnumber].gastate[addr].port, ga[busnumber].gastate[addr].action,
        &ga[busnumber].gastate[addr].tv[ga[busnumber].gastate[addr].port]);
    return SRCP_OK;
  }
  else
  {
    return SRCP_NODATA;
  }
}

int describeGA(int busnumber, int addr, char *msg)
{
  int number_ga = get_number_ga(busnumber);

  if(number_ga<0)
    return SRCP_UNSUPPORTEDDEVICEGROUP;

  if((addr>0) && (addr <= number_ga) && (ga[busnumber].gastate[addr].protocol) )
  {
    sprintf(msg, "%d GA %d %s", busnumber, addr, ga[busnumber].gastate[addr].protocol);
  }
  else
  {
    strcpy(msg, "");
    return SRCP_NODATA;
  }
  return SRCP_INFO;
}

int infoGA(int busnumber, int addr, int port, char* msg)
{
  int number_ga = get_number_ga(busnumber);

  if((addr>0) && (addr <= number_ga))
  {
    sprintf(msg, "%ld.%ld 100 INFO %d GA %d %d %d\n",
      ga[busnumber].gastate[addr].tv[port].tv_sec,
      ga[busnumber].gastate[addr].tv[port].tv_usec/1000, busnumber,
      ga[busnumber].gastate[addr].id, ga[busnumber].gastate[addr].port,
      ga[busnumber].gastate[addr].action);
  }
  else
  {
    return SRCP_NODATA;
  }
  return SRCP_INFO;
}

int initGA(int busnumber, int addr, const char *protocol)
{
  int number_ga = get_number_ga(busnumber);

  if((addr > 0) && (addr <= number_ga))
  {
    strcpy(ga[busnumber].gastate[addr].protocol, protocol);
    return SRCP_OK;
  }
  else
  {
    return SRCP_UNSUPPORTEDDEVICE;
  }
}

int lockGA(int busnumber, int addr, long int sessionid)
{
  return SRCP_NOTSUPPORTED;
}

int getlockGA(int busnumber, int addr, long int sessionid)
{
  return SRCP_NOTSUPPORTED;
}


int unlockGA(int busnumber, int addr, int sessionid)
{
  if(ga[busnumber].gastate[addr].locked_by==sessionid)
  {
    ga[busnumber].gastate[addr].locked_by = 0;
    return SRCP_OK;
  }
  else
  {
    return SRCP_DEVICELOCKED;
  }
}

void unlock_ga_bysessionid(long int sessionid)
{
  int i,j;
  int number;
  syslog(LOG_INFO, "unlock GA by session-ID %ld", sessionid);
  for(i=0; i<MAX_BUSSES; i++)
  {
    number = get_number_ga(i);
    syslog(LOG_INFO, "number of GA for busnumber %d is %d", i, number);
    for(j=0;j<number; j++)
    {
      if(ga[i].gastate[j].locked_by == sessionid)
      {
        unlockGA(i, j+1, sessionid);
      }
    }
  }
}

int startup_GA(void)
{
  int i;
  for(i=0;i<MAX_BUSSES;i++)
  {
    in[i]=0;
    out[i]=0;
    ga[i].numberOfGa = 0;
    ga[i].gastate = NULL;
    pthread_mutex_init(&queue_mutex[i], NULL);
  }
  return 0;
}

int init_GA(int busnumber, int number)
{
  int i;

  if (busnumber >= MAX_BUSSES)
    return 1;

  if(number > 0)
  {
    ga[busnumber].gastate = malloc(number * sizeof(struct _GASTATE));
    if (ga[busnumber].gastate == NULL)
      return 1;
    ga[busnumber].numberOfGa = number;
    for(i=0;i<number;i++)
      ga[busnumber].gastate[i].protocol = NULL;
  }
  return 0;
}
