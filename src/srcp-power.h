/* $Id$ */

/* 
 * Vorliegende Software unterliegt der General Public License, 
 * Version 2, 1991. (c) Matthias Trute, 2000-2001.
 *
 */


#ifndef _SRCPPOWER_H_
#define _SRCPPOWER_H_

void initPower(int);
void infoPower(int bus, char *msg);
void setPower(int bus, int state, char *msg);
int getPower(int bus);

#endif
