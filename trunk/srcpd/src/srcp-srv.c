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

int startup_SERVER(void) {
    return 0;
}
int init_bus_server(int bus) {
    return 0;
}

int term_bus_server(int bus) {
    return 0;
}

void server_reset()
{
  server_reset_state = 1;
}

void server_shutdown()
{
  server_shutdown_state = 1;
}
