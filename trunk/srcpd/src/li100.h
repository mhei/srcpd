/***************************************************************************
                           li100.h  -  description
                             -------------------
    begin                : Thu Jan 22 2002
    copyright            : (C) 2002-2007 by Dipl.-Ing. Frank Schmischke
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
#ifndef _LI100_H
#define _LI100_H

#include <libxml/tree.h>        /*xmlDocPtr, xmlNodePtr */


typedef struct _LI100_DATA {
    int number_ga;
    int number_gl;
    int number_fb;
    int last_bit;
    int last_type;
    int last_typeaddr;
    int last_value;
    int get_addr;
    ga_state_t tga[50];
    int working_LI100;
    int emergency_on_LI100;
    int version_interface;
    int code_interface;
    int version_zentrale;
    int code_zentrale;
    int extern_engine[100];
    int extern_engine_ctr;
    int pgm_mode;
} LI100_DATA;

int readConfig_LI100_USB(xmlDocPtr doc, xmlNodePtr node, bus_t busnumber);
int init_bus_LI100_USB(bus_t busnumber);

int readConfig_LI100_SERIAL(xmlDocPtr doc, xmlNodePtr node,
                            bus_t busnumber);
int init_bus_LI100_SERIAL(bus_t busnumber);

int init_gl_LI100(gl_state_t * gl);
int init_ga_LI100(ga_state_t * ga);
void *thr_sendrec_LI100_USB(void *);
void *thr_sendrec_LI100_SERIAL(void *);

#endif
