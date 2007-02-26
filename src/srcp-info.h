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

#include "srcp-session.h"

int doInfoClient(int Socket, sessionid_t sessionid);
int startup_INFO(void);

int queueIsEmptyInfo();
int unqueueInfoNext(char *info);

int queueInfoMessage(char *);

#endif
