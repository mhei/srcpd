/***************************************************************************
                          srcp-info.h  -  description
                             -------------------
    begin                : Mon May 20 2002
    copyright            : (C) 2002 by 
    email                : 
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef _SRCP_INFO_H
#define _SRCP_INFO_H

void *thr_doInfoClient(void *v);
int doInfoClient(int Socket, int sessionid);
int startup_INFO(void);

int queueIsEmptyInfo();
int unqueueInfoNext(char *info);

int queueInfoGL(int busnumber, int addr, int dir, int speed, int maxspeed, int f,
      int f1, int f2, int f3, int f4, struct timeval *akt_time);
int queueInfoGA(int busnumber, int addr, int port, int action, struct timeval *akt_time);
int queueInfoFB(int busnumber, int port, int action, struct timeval *akt_time);
int queueInfoSM(int busnumber, int addr, int type, int typeaddr, int bit, int value, struct timeval *akt_time);

#endif
