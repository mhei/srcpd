/* $Id$ */

/* 
 * Vorliegende Software unterliegt der General Public License, 
 * Version 2, 1991. (c) Matthias Trute, 2000-2002.
 *
 */
#include "stdincludes.h"

#include "srcp-error.h"

int srcp_fmt_msg(int errorcode, char *msg, struct timeval time)
{
  switch (errorcode)
  {
    case 100:
      sprintf(msg, "%lu.%lu %d INFO", time.tv_sec, time.tv_usec/1000, errorcode);
      break;
    case 101:
      sprintf(msg, "%lu.%lu %d TERM", time.tv_sec, time.tv_usec/1000, errorcode);
      break;
    case 102:
      sprintf(msg, "%lu.%lu %d INIT", time.tv_sec, time.tv_usec/1000, errorcode);
      break;
    case 110:
      sprintf(msg, "%lu.%lu %d INFO", time.tv_sec, time.tv_usec/1000, errorcode);
      break;
    case 200:
      sprintf(msg, "%lu.%lu %d OK\n", time.tv_sec, time.tv_usec/1000, errorcode);
      break;
    case 202:
      sprintf(msg, "%lu.%lu %d OK CONNECTION MODE\n", time.tv_sec, time.tv_usec/1000, errorcode);
      break;
    case 201:
      sprintf(msg, "%lu.%lu %d OK PROTOCOL SRCP\n", time.tv_sec, time.tv_usec/1000, errorcode);
      break;
    case 400:
        sprintf(msg, "%lu.%lu %d ERROR unsupported protocol", time.tv_sec, time.tv_usec/1000, errorcode);
        break;
    case 401:
        sprintf(msg, "%lu.%lu %d ERROR unsupported connectionmode", time.tv_sec, time.tv_usec/1000, errorcode);
        break;
    case 402:
        sprintf(msg, "%lu.%lu %d ERROR unsufficient data", time.tv_sec, time.tv_usec/1000, errorcode);
        break;
    case 410:
      sprintf(msg, "%lu.%lu %d ERROR unknown command\n", time.tv_sec, time.tv_usec/1000, errorcode);
      break;
    case 411:
      sprintf(msg, "%lu.%lu %d ERROR unknown value", time.tv_sec, time.tv_usec/1000, errorcode);
      break;
    case 412:
      sprintf(msg, "%lu.%lu %d ERROR wrong value", time.tv_sec, time.tv_usec/1000, errorcode);
      break;
    case 413:
      sprintf(msg, "%lu.%lu %d ERROR temporarily prohibited", time.tv_sec, time.tv_usec/1000, errorcode);
       break;
    case 414:
      sprintf(msg, "%lu.%lu %d ERROR device locked", time.tv_sec, time.tv_usec/1000, errorcode);
      break;
    case 415:
      sprintf(msg, "%lu.%lu %d ERROR forbidden", time.tv_sec, time.tv_usec/1000, errorcode);
      break;
    case 416:
      sprintf(msg, "%lu.%lu %d ERROR no data", time.tv_sec, time.tv_usec/1000, errorcode);
      break;
    case 417:
      sprintf(msg, "%lu.%lu %d ERROR timeout", time.tv_sec, time.tv_usec/1000, errorcode);
      break;
    case 418:
      sprintf(msg, "%lu.%lu %d ERROR list too long", time.tv_sec, time.tv_usec/1000, errorcode);
      break;
    case 419:
      sprintf(msg, "%lu.%lu %d ERROR list too short", time.tv_sec, time.tv_usec/1000, errorcode);
      break;
    case 420:
      sprintf(msg, "%lu.%lu %d ERROR unsupported device protocol", time.tv_sec, time.tv_usec/1000, errorcode);
      break;
    case 421:
      sprintf(msg, "%lu.%lu %d ERROR unsupported device", time.tv_sec, time.tv_usec/1000, errorcode);
      break;
    case 422:
      sprintf(msg, "%lu.%lu %d ERROR unsupported device group", time.tv_sec, time.tv_usec/1000, errorcode);
      break;
    case 423:
      sprintf(msg, "%lu.%lu %d ERROR unsupported operation", time.tv_sec, time.tv_usec/1000, errorcode);
      break;
    case 424:
      sprintf(msg, "%lu.%lu %d ERROR device reinitialized", time.tv_sec, time.tv_usec/1000, errorcode);
      break;
    default:
      sprintf(msg, "%lu.%lu 600 ERROR internal error %d, please report to srcpd-devel@srcpd.sorceforge.net", time.tv_sec, time.tv_usec/1000, errorcode);
      return 1;
  }
  return 0;
}
