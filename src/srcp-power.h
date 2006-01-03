/* $Id$ */

/*
 * Vorliegende Software unterliegt der General Public License,
 * Version 2, 1991. (c) Matthias Trute, 2000-2001.
 *
 */


#ifndef _SRCP_POWER_H
#define _SRCP_POWER_H

int initPower(long);
int infoPower(long int bus, char *msg);
int setPower(long int bus, int state, char *msg);
int getPower(long int bus);

#endif
