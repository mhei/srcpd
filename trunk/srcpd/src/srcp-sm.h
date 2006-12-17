/***************************************************************************
                          srcp-sm.h  -  description
                             -------------------
    begin                : Mon Aug 12 2002
    copyright            : (C) 2002 by Dipl.-Ing. Frank Schmischke
    email                : frank.schmischke@t-online.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef _SRCP_SM_H
#define _SRCP_SM_H

#include <sys/time.h>
#include "config-srcpd.h"

enum COMMAND {
    SET = 0,
    GET,
    VERIFY,
    INIT,
    TERM
};

enum TYPE {
    REGISTER = 0,
    PAGE,
    CV,
    CV_BIT
};

/* Lokdekoder */
struct _SM {
    char protocol[6];           // at the moment only NMRA is support
    // (for IB, but not completely)
    // (work in progress)
    int type;
    int command;
    int protocolversion;
    int addr;
    int typeaddr;
    int bit;                    // bit to set/get for CVBIT
    int value;
    struct timeval tv;          // time of change
} SM;

int queueSM(long int busnumber, int command, int type, int addr, int typeaddr,
            int bit, int value);
int queue_SM_isempty(long int busnumber);
int unqueueNextSM(long int busnumber, struct _SM *l);

int getSM(long int busnumber, int addr, struct _SM *l);
int setSM(long int busnumber, int type, int addr, int typeaddr, int bit,
          int value, int return_value);
int infoSM(long int busnumber, int command, int type, int addr, int typeaddr,
           int bit, int value, char *info);

#endif
