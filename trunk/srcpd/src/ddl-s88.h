/***************************************************************************
                          ddl-s88.h  -  description
                             -------------------
    begin                : Wed Aug 1 2001
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
#ifndef DDL_S88_H
#define DDL_S88_H

int init_bus_S88();
int term_bus_S88();
void * thr_sendrec_S88(void *);

int init_s88(char*);
int get_s88(int);

void load_s88(int);
void clear_s88(int);

#endif
