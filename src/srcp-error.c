/* $Id$ */

/* 
 * Vorliegende Software unterliegt der General Public License, 
 * Version 2, 1991. (c) Matthias Trute, 2000-2002.
 *
 */

#include <stdio.h>

#include "srcp-error.h"

int
srcp_fmt_msg(int errorcode, char *msg)
{
  switch (errorcode)
  {
    case 100:
      sprintf(msg, "%d INFO", errorcode);
      break;
    case 101:
      sprintf(msg, "%d TERM", errorcode);
      break;
    case 102:
      sprintf(msg, "%d INIT", errorcode);
      break;
    case 110:
      sprintf(msg, "%d INFO", errorcode);
      break;
    case 200:
      sprintf(msg, "%d OK", errorcode);
      break;
    case 201:
      sprintf(msg, "%d OK CONNECTION MODE", errorcode);
      break;
    case 202:
      sprintf(msg, "%d OK PROTOCOL SRCP", errorcode);
      break;
    case 402:
        sprintf(msg, "%d ERROR WRONG COMMAND", errorcode);
        break;
    case 410:
      sprintf(msg, "%d ERROR unknown command", errorcode);
      break;
    case 411:
      sprintf(msg, "%d ERROR unknown value", errorcode);
      break;
    case 412:
      sprintf(msg, "%d ERROR wrong value", errorcode);
      break;
    case 413:
      sprintf(msg, "%d ERROR temporarily prohibited", errorcode);
       break;
    case 414:
      sprintf(msg, "%d ERROR device locked", errorcode);
      break;
    case 415:
      sprintf(msg, "%d ERROR forbidden", errorcode);
      break;
    case 416:
      sprintf(msg, "%d ERROR no data", errorcode);
      break;
    case 417:
      sprintf(msg, "%d ERROR timeout", errorcode);
      break;
    case 418:
      sprintf(msg, "%d ERROR list too long", errorcode);
      break;
    case 419:
      sprintf(msg, "%d ERROR list too short", errorcode);
      break;
    case 420:
      sprintf(msg, "%d ERROR unsupported device protocol", errorcode);
      break;
    case 421:
      sprintf(msg, "%d ERROR unsupported device", errorcode);
      break;
    case 422:
      sprintf(msg, "%d ERROR unsupported device group", errorcode);
      break;
    case 423:
      sprintf(msg, "%d ERROR unsupported operation", errorcode);
      break;
    case 424:
      sprintf(msg, "%d ERROR device reinitialized", errorcode);
      break;
    case 425:
      sprintf(msg, "%d ERROR not supported", errorcode);
      break;
    default:
      sprintf(msg, "600 ERROR internal error %d, please report to srcpd-devel@srcpd.sorceforge.net", errorcode);
      return 1;
  }
  return 0;
}
