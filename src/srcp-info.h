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

#include "srcp-gl.h"
#include "srcp-ga.h"
#include "srcp-fb.h"

enum TypeOfInfo
{
  INFO_GL = 0,
  INFO_GA,
  INFO_FB,
  INFO_SM
};

struct _INFO
{
  enum TypeOfInfo infoType;
  int bus;              // number of from info
  char protocol[5];
  int protocolversion;
  int n_func;
  int n_fs;
  int id;               /* Adresse, wird auch als Semaphor genutzt! */
  int speed;            /* Sollgeschwindigkeit skal. auf 0..14      */
  int maxspeed;         /* Maximalgeschwindigkeit                   */
  int direction;        /* 0/1/2                                    */
  char funcs;           /* F1..F4, F                                */

  int port;             /* Portnummer     */
  int action;           /* 0,1,2,3...     */
};

int doInfoClient(int, int);
int startup_INFO(void);

int queueIsEmptyInfo();
int unqueueInfoNext(struct _INFO *info);

int queueInfoGL(int bus, int addr, int dir, int speed, int maxspeed, int f,
      int f1, int f2, int f3, int f4);
int queueInfoGA(int bus, int addr, int port, int action);
int queueInfoFB(int bus, int port, int action);
int queueInfoSM(int bus, int addr);

#endif
