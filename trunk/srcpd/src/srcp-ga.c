
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
static int queue_len(int bus);
static int queue_isfull(int bus);

int get_number_ga (int bus)
{
  return ga[bus].numberOfGa;
}

/* Übernehme die neuen Angaben für die Weiche, einige wenige Prüfungen */
int queueGA(int bus, int addr, int port, int action, long int activetime)
{
  struct timeval akt_time;
  int number_ga = get_number_ga(bus);

  if ((addr > 0) && (addr <= number_ga) )
  {
    while (queue_isfull(bus))
    {
      usleep(1000);
    }

    pthread_mutex_lock(&queue_mutex[bus]);
    queue[bus][in[bus]].action = action;
    queue[bus][in[bus]].port = port;
    queue[bus][in[bus]].activetime = activetime;
    gettimeofday(&akt_time, NULL);
    queue[bus][in[bus]].tv[port] = akt_time;
    queue[bus][in[bus]].id = addr;
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

int queue_GA_isempty(int bus)
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
int getNextGA(int bus, struct _GASTATE *a)
{
  if (in[bus] == out[bus])
    return -1;
  *a = queue[bus][out[bus]];
  return out[bus];
}

/** liefert nächsten Eintrag oder -1, setzt fifo pointer neu! */
int unqueueNextGA(int bus, struct _GASTATE *a)
{
  if (in[bus] == out[bus])
    return -1;

  *a = queue[bus][out[bus]];
  out[bus]++;
  if (out[bus] == QUEUELEN)
    out[bus] = 0;
  return out[bus];
}

int getGA(int bus, int addr, struct _GASTATE *a)
{
  int number_ga = get_number_ga(bus);

  if((addr>0) && (addr <= number_ga))
  {
    *a = ga[bus].gastate[addr];
    return SRCP_OK;
  }
  else
  {
    return SRCP_NODATA;
  }
}

static int initGA_default(int bus, int addr)
{
  int number_ga = get_number_ga(bus);
  
  if((addr > 0) && (addr <= number_ga))
  {
    strcpy(ga[bus].gastate[addr].protocol, "P");
    return SRCP_OK;
  }
  else
  {
    return SRCP_UNSUPPORTEDDEVICE;
  }
}

int isInitializedGA(int bus, int addr)
{
  return ga[bus].gastate[addr].protocol == NULL;
}

/* ********************
     SRCP Kommandos
*/
int setGA(int bus, int addr, struct _GASTATE a)
{
  int number_ga = get_number_ga(bus);

  if((addr>0) && (addr <= number_ga))
  {
    if(!isInitializedGA(bus, addr))
      initGA_default(bus, addr);
    ga[bus].gastate[addr] = a;
    gettimeofday((struct timeval*)&ga[bus].gastate[addr].tv, NULL);
    return SRCP_OK;
  }
  else
  {
    return SRCP_NODATA;
  }
}

int describeGA(int bus, int addr, char *msg)
{
  int number_ga = get_number_ga(bus);

  if(number_ga<0)
    return SRCP_UNSUPPORTEDDEVICEGROUP;

  if((addr>0) && (addr <= number_ga) && (ga[bus].gastate[addr].protocol) )
  {
    sprintf(msg, "%d GA %d %s", bus, addr, ga[bus].gastate[addr].protocol);
  } 
  else 
  {
    strcpy(msg, "");
    return SRCP_NODATA;
  }
  return SRCP_INFO;
}

int infoGA(int bus, int addr, int port, char* msg)
{
  int number_ga = get_number_ga(bus);

  if((addr>0) && (addr <= number_ga))
  {
    sprintf(msg, "%ld.%ld 100 INFO %d GA %d %d %d\n", 
      ga[bus].gastate[addr].tv[port].tv_sec,
      ga[bus].gastate[addr].tv[port].tv_usec/1000, bus,
      ga[bus].gastate[addr].id, ga[bus].gastate[addr].port,
      ga[bus].gastate[addr].action);
  }
  else
  {
    return SRCP_NODATA;
  }
  return SRCP_INFO;
}

int initGA(int bus, int addr, const char *protocol)
{
  int number_ga = get_number_ga(bus);
  
  if((addr > 0) && (addr <= number_ga))
  {
    strcpy(ga[bus].gastate[addr].protocol, protocol);
    return SRCP_OK;
  }
  else
  {
    return SRCP_UNSUPPORTEDDEVICE;
  }
}

int lockGA(int bus, int addr, long int sessionid)
{
  return SRCP_NOTSUPPORTED;
}

int getlockGA(int bus, int addr, long int sessionid)
{
  return SRCP_NOTSUPPORTED;
}


int unlockGA(int bus, int addr, int sessionid)
{
  if(ga[bus].gastate[addr].locked_by==sessionid)
  {
    ga[bus].gastate[addr].locked_by = 0;
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
    syslog(LOG_INFO, "number of GA for bus %d is %d", i, number);
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

int init_GA(int bus, int number)
{
  int i;

  if (bus >= MAX_BUSSES)
    return 1;

  if(number > 0)
  {
    ga[bus].gastate = malloc(number * sizeof(struct _GASTATE));
    if (ga[bus].gastate == NULL)
      return 1;
    ga[bus].numberOfGa = number;
    for(i=0;i<number;i++)
      ga[bus].gastate[i].protocol = NULL;
  }
  return 0;
}
