/* $Id$ */

/* 
 * Vorliegende Software unterliegt der General Public License, 
 * Version 2, 1991. (c) Matthias Trute, 2000-2001.
 *
 */


#include "m605x.h"
#include "srcp-srv.h"

int server_reset_state;
int server_shutdown_state;

void server_reset(int bus)
{
  server_reset_state = 1;
}

void server_shutdown(int bus)
{
  server_shutdown_state = 1;
}
