/* $Id$ */

/* 
 * Vorliegende Software unterliegt der General Public License, 
 * Version 2, 1991. (c) Matthias Trute, 2000-2001.
 *
 */


#ifndef _SRCPFB_H_
#define _SRCPFB_H_

int getFBone(const char *proto, int port);
void getFBall(const char *proto, char *reply);

void infoFB(const char *proto, int port, char *msg);
void initFB(void);

#endif
