/***************************************************************************
                            ib.h  -  description
                             -------------------
    begin                : Thu Apr 19 2001
    copyright            : (C) 2001 by Dipl.-Ing. Frank Schmischke
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
#ifndef _IB_H
#define _IB_H

#include <libxml/tree.h>

typedef struct _IB_DATA {
    int number_ga;
    int number_gl;
    int number_fb;
} IB_DATA;

void readconfig_intellibox(xmlDocPtr doc, xmlNodePtr node,  int busnumber);


int init_bus_IB(int);
int term_bus_IB(int );

void * thr_sendrec_IB(void *);

void close_comport(int fd);
int init_comport(int bus);
int open_comport(int bus);
void* thr_sendrecintellibox(void*);
void send_command_ga(int fd);
void send_command_gl(int fd);
void check_status(int fd);

#endif
