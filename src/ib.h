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

typedef struct _IB_DATA 
{
    int number_ga;
    int number_gl;
    int number_fb;
    int last_bit;
    int last_type;
    int last_typeaddr;
    struct _GASTATE tga[50];
    int working_IB;
} IB_DATA;

void readconfig_intellibox(xmlDocPtr doc, xmlNodePtr node,  int busnumber);


int init_bus_IB(int);
int term_bus_IB(int);

int init_gl_IB(struct _GLSTATE *gl);
int init_ga_IB(struct _GASTATE *ga);

void* thr_sendrec_IB(void *);

void send_command_ga_ib(int busnumber);
void send_command_gl_ib(int busnumber);
void send_command_sm_ib(int busnumber);
void check_status_ib(int busnumber);
void check_status_pt_ib(int busnumber);

#endif
