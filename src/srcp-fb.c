/* $Id$ */

/* 
 * Vorliegende Software unterliegt der General Public License, 
 * Version 2, 1991. (c) Matthias Trute, 2000-2001.
 *
 */


#include <stdio.h>
#include <string.h>
#include <syslog.h>

#include "srcp-fb.h"
#include "srcp-fb-s88.h"
#include "srcp-fb-i8255.h"

void getFBall(const char *proto, char *reply)
{
  int i;

  if(strncmp(proto, "M6051", 5)==0)
  {
    strcpy(reply, "INFO FB M6051 * ");   // und hier wird in d das Ergebnis aufgebaut
    for(i=0; i<getPortCount_S88(); i++)
    {
      char cc[5];
      sprintf(cc, "%d", getFB_S88(i));
      strcat(reply, cc);
    }
    strcat(reply, "\n");
  }
  else
    if (strncmp(proto, "i8255", 5)==0)
    {
      strcpy(reply, "INFO FB I8255 * ");   // und hier wird in d das Ergebnis aufgebaut
      for(i=1; i<=getPortCount_I8255(); i++)
      {
        char cc[5];
        sprintf(cc, "%d", getFB_I8255(i));
        strcat(reply, cc);
      }
    }
    else
    {
      sprintf(reply, "INFO -1");
    }
    strcat(reply, "\n");
}

int getFBone(const char *proto, int port)
{
  int result;

  result = -1;
  if(strcmp(proto, "M6051") == 0)
  {
    result = getFB_S88(port);
  }
  if(strcmp(proto, "IB") == 0)
  {
    result = getFB_S88(port);
  }
  if(strcmp(proto, "I8255") == 0)
  {
    result = getFB_I8255(port);
  }
  return result;
}

void infoFB(const char *proto, int port, char *msg)
{
  int state = getFBone(proto, port);
//  syslog(LOG_INFO, "infoFBone result: %i", state);
  if(state>=0)
  {
    sprintf(msg, "INFO FB %s %d %d\n", proto, port + 1, state);
  }
  else
  {
    sprintf(msg, "INFO -2\n");
  }
  syslog(LOG_INFO, "%s", msg);
}

void initFB()
{
  initFB_S88();
}
