/***************************************************************************
                          intelliboxd.h  -  description
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
#ifndef IB_H
#define IB_H

int init_comport(int);
int open_comport(int);

void* thr_sendrecintellibox(void*);
int init_bus_IB(int);
#endif
