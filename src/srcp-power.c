/* $Id$ */

/* 
 * Vorliegende Software unterliegt der General Public License, 
 * Version 2, 1991. (c) Matthias Trute, 2000-2001.
 *
 */


#include <string.h>
#include <stdio.h>
#include "srcp-power.h"

volatile int power_state       = 0; /* liegt Spannung am GLeis?            */
volatile int power_changed     = 1; /* bei jeder Änderung gesetzt               */
char power_msg[1024];

void setPower(int state, char *msg) {
    power_state = state;
    strcpy(power_msg, msg);
    power_changed = 1;
}

int getPower(void) {
    return power_state;
}

void infoPower(char *msg) {
    sprintf(msg, "INFO POWER %s %s\n", power_state?"ON":"OFF", power_msg);
}


void initPower() {
}