/***************************************************************************
                           li100.h  -  description
                             -------------------
    begin                : Thu Jan 22 2002
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
#ifndef _LI100_H
#define _LI100_H

#include <libxml/tree.h>

typedef struct _LI100_DATA
{
    int number_ga;
    int number_gl;
    int number_fb;
    int last_bit;
    int last_type;
    int last_typeaddr;
    int last_value;
    int get_addr;
    struct _GASTATE tga[50];
    int working_LI100;
    int emergency_on_LI100;
    int version_interface;
    int code_interface;
    int version_zentrale;
    int code_zentrale;
    int extern_engine[100];
    int extern_engine_ctr;
} LI100_DATA;

int readConfig_LI100_USB(xmlDocPtr doc, xmlNodePtr node,  long int busnumber);
long int init_bus_LI100_USB( long int busnumber );
long int term_bus_LI100_USB( long int busnumber );
void send_command_ga_LI100_USB( long int busnumber );
void send_command_gl_LI100_USB( long int busnumber );
void send_command_sm_LI100_USB( long int busnumber );
void check_status_LI100_USB( long int busnumber );
int send_command_LI100_USB( long int busnumber, unsigned char *str );
void get_status_sm_LI100_USB( long int busnumber );

int readConfig_LI100_SERIAL(xmlDocPtr doc, xmlNodePtr node,  long int busnumber);
long int init_bus_LI100_SERIAL( long int busnumber );
long int term_bus_LI100_SERIAL( long int busnumber );
void send_command_ga_LI100_SERIAL( long int busnumber );
void send_command_gl_LI100_SERIAL( long int busnumber );
void send_command_sm_LI100_SERIAL( long int busnumber );
void check_status_LI100_SERIAL( long int busnumber );
int send_command_LI100_SERIAL( long int busnumber, unsigned char *str );
void get_status_sm_LI100_SERIAL( long int busnumber );

long int init_gl_LI100( struct _GLSTATE *gl );
long int init_ga_LI100( struct _GASTATE *ga );
void* thr_sendrec_LI100_USB( void* );
void* thr_sendrec_LI100_SERIAL( void* );
void add_extern_engine( long int busnumber, int address );
void remove_extern_engine( long int busnumber, int address );
void check_extern_engines_USB( long int busnumber );
void check_extern_engines_SERIAL( long int busnumber );

#endif
