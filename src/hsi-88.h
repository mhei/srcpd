/***************************************************************************
                          hsi-88.h  -  description
                             -------------------
    begin                : Mon Oct 29 2001
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
#ifndef HSI_88_H
#define HSI_88_H

int init_lineHSI88(char*);
void* thr_sendrec_hsi_88(void*);

#endif
