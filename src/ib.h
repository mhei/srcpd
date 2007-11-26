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
    int last_value;
    ga_state_t tga[50];
    int working_IB;
    int emergency_on_ib;
    unsigned int pause_between_cmd;
} IB_DATA;

int readConfig_IB(xmlDocPtr doc, xmlNodePtr node,  bus_t busnumber);


int init_bus_IB(bus_t busnumber);
int term_bus_IB(bus_t busnumber);
int init_gl_IB(struct _GLSTATE *gl);
int init_ga_IB(ga_state_t *ga);

void* thr_sendrec_IB(void *);

void send_command_ga_IB(bus_t busnumber);
void send_command_gl_IB(bus_t busnumber);
void send_command_sm_IB(bus_t busnumber);
void check_status_IB(bus_t busnumber);
void check_status_pt_IB(bus_t busnumber);


#define XLok        0x80
#define XLokSts     0x84
#define XLokCfg     0x85
#define XFunc       0x88
#define XFuncX      0x89
#define XFuncSts    0x8C
#define XFuncXSts   0x8D
#define XTrnt       0x90
#define XTrntFree   0x93
#define XTrntSts    0x94
#define XTrntGrp    0x95
#define XSensor     0x98
#define XSensOff    0x99
#define X88PGet     0x9C
#define X88PSet     0x9D
#define Xs88PTim    0x9E
#define Xs88Cnt     0x9F
#define XVer        0xA0
#define XP50C       0xA1
#define XStatus     0xA2
#define XSOGet      0xA4
#define XHalt       0xA5
#define XPwrOff     0xA6
#define XPwrOn      0xA7
#define XLokoNet    0xC0
#define XNOP        0xC4
#define XDummy      0xC4
#define XP50Len1    0xC6
#define XP50Len2    0xC7
#define XEvent      0xC8
#define XEvtLok     0xC9
#define XEvtTrn     0xCA
#define XEvtSen     0xCB
#define XEvtIR      0xCC
#define XEvtTkR     0xCF
#define XEvtMem     0xD0
#define XCR         0x0D

/* IB error codes */

#define XBADPARAM   0x02
#define XPWOFF      0x06
#define XNOTSPC     0x07
#define XNOLSPC     0x08
#define XNODATA     0x0A
#define XNOSLOT     0x0B
#define XBADLNP     0x0C
#define XBADTNP     0x0E
#define XLKBUSY     0x0D
#define XNOISPC     0x10
#define XLOWTSP     0x40
#define XLKHALT     0x41
#define XLKPOFF     0x42

#endif
