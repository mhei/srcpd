/* $Id$ */

/* 
 * Vorliegende Software unterliegt der General Public License, 
 * Version 2, 1991. (c) Matthias Trute, 2000-2001.
 *
 */


#ifndef _SRCPPOWER_H_
#define _SRCPPOWER_H_

extern char power_msg[1024];
extern volatile int power_state ;  /* liegt Spannung am GLeis?            */
extern volatile int power_changed; /* bei jeder Änderung gesetzt          */

void initPower();
void infoPower(char *msg);
void setPower(int state, char *msg);
int getPower(void);


#endif
