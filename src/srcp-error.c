/* $Id$ */

/* 
 * Vorliegende Software unterliegt der General Public License, 
 * Version 2, 1991. (c) Matthias Trute, 2000-2002.
 *
 */

#include <stdio.h>

#include "srcp-error.h"

int
srcp_fmt_msg(int errno, char *msg)
{
  switch (errno)
  {
    case 100:
      sprintf(msg, "%d INFO", errno);
      break;
    case 101:
      sprintf(msg, "%d TERM", errno);
      break;
    case 102:
      sprintf(msg, "%d INIT", errno);
      break;
    case 110:
      sprintf(msg, "%d INFO", errno);
      break;
    case 200:  
      sprintf(msg, "%d OK", errno);
      break;
    case 201:  
      sprintf(msg, "%d OK CONNECTION MODE", errno);
      break;
    case 202:  
      sprintf(msg, "%d OK PROTOCOL SRCP", errno);
      break;
    case 402:
        sprintf(msg, "%d ERROR WRONG COMMAND", errno);
        break;
    case 410:
      sprintf(msg, "%d ERROR unknown command", errno);
      break;
    case 411:
      sprintf(msg, "%d ERROR unknown value", errno);
      break;
    case 412:
      sprintf(msg, "%d ERROR wrong value", errno);
      break;
    case 413:
      sprintf(msg, "%d ERROR temporarily prohibited", errno);
       break;
    case 414:
      sprintf(msg, "%d ERROR device locked", errno);
      break;
    case 415:
      sprintf(msg, "%d ERROR forbidden", errno);
      break;
    case 416:
      sprintf(msg, "%d ERROR no data", errno);
      break;
    case 417:
      sprintf(msg, "%d ERROR timeout", errno);
      break;
    case 418:
      sprintf(msg, "%d ERROR list too long", errno);
      break;
    case 419:
      sprintf(msg, "%d ERROR list too short", errno);
      break;
    case 420:
      sprintf(msg, "%d ERROR unsupported device protocol", errno);
      break;
    case 421:
      sprintf(msg, "%d ERROR unsupported device", errno);
      break;
    case 422:
      sprintf(msg, "%d ERROR unsupported device group", errno);
      break;
    case 423:
      sprintf(msg, "%d ERROR unsupported operation", errno);
      break;
    case 424:
      sprintf(msg, "%d ERROR device reinitialized", errno);
      break;
    case 425:
      sprintf(msg, "%d ERROR not supported", errno);
      break;
    default:
      sprintf(msg, "600 ERROR internal error %d, please report to srcpd-devel@srcpd.sorceforge.net", errno);
      return 1;
  }
  return 0;
}
