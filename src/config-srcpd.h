/***************************************************************************
                          config_srcpd.h  -  description
                             -------------------
    begin                : Wed Jul 4 2001
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
#ifndef _CONFIG_SRCPD_H
#define _CONFIG_SRCPD_H

#define NUMBER_SERVERS    4         // Anzahl der im srcpd integrierten Server
#define SERVER_DDL        0         // srcpd arbeitet als DDL-Server
#define SERVER_M605X      1         // srcpd arbeitet als M605X-Server
#define SERVER_IB         2         // srcpd arbeitet als IB-Server
#define SERVER_LI100      3         // srcpd arbeitet als Lenz-Server

void readConfig(void);

#endif
