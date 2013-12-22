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

#include <libxml/tree.h> /*xmlDocPtr, xmlNodePtr*/


typedef struct _IB_DATA {
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
    bool pt_initialized; /* This will be set to true when using the programming track. */
} IB_DATA;

int readConfig_IB(xmlDocPtr doc, xmlNodePtr node, bus_t busnumber);
int init_bus_IB(bus_t busnumber);
int init_gl_IB(gl_state_t *gl);
int init_ga_IB(ga_state_t *ga);
void* thr_sendrec_IB(void *);

/* IB uses 126 speed steps internally. */
#define SPEED_STEPS 126

/* values for power_state */
enum {POWER_OFF = 0, POWER_ON = 1};

static const char P50X_DISABLE[] = "xZzA1";
static const char P50X_ENABLE[] = "ZzA0";

/* general purpose P50Xb commands */
enum {
    XLok       = 0x80,
    XLkDisp    = 0x83,
    XLokSts    = 0x84,
    XLokCfg    = 0x85,
    XFunc      = 0x88,
    XFuncX     = 0x89,
    XFunc34    = 0x8A,
    XFuncSts   = 0x8C,
    XFuncXSts  = 0x8D,
    XFunc34Sts = 0x8E,
    XLimit     = 0x8F,
    XTrnt      = 0x90,
    XTrntFree  = 0x93,
    XTrntSts   = 0x94,
    XTrntGrp   = 0x95,
    XSensor    = 0x98,
    XSensOff   = 0x99,
    X88PGet    = 0x9C,
    X88PSet    = 0x9D,
    Xs88PTim   = 0x9E,
    Xs88Cnt    = 0x9F,
    XVer       = 0xA0,
    XP50XCh    = 0xA1,
    XStatus    = 0xA2,
    XSOSet     = 0xA3,
    XSOGet     = 0xA4,
    XHalt      = 0xA5,
    XPwrOff    = 0xA6,
    XPwrOn     = 0xA7,
    XLokoNet   = 0xC0,
    XNOP       = 0xC4,
    XP50Len1   = 0xC6,
    XP50Len2   = 0xC7,
    XEvent     = 0xC8,
    XEvtLok    = 0xC9,
    XEvtTrn    = 0xCA,
    XEvtSen    = 0xCB,
    XEvtIR     = 0xCC,
    XEvtLN     = 0xCD,
    XEvtPT     = 0xCE,
    XEvtTkR    = 0xCF,
    XEvtMem    = 0xD0,
    XEvtLSY    = 0xD1,
    XEvtBiDi   = 0xD2,
    XBiDiSet   = 0xD3,
    XBiDiQuery = 0xD4,

/* programming track P50X commands */

    XDCC_PDR   = 0xDA,
    XDCC_PAR   = 0xDB,
    XPT_DCCEWr = 0xDC,
    XPT_FMZEWr = 0xDD,
    XDCC_PD    = 0xDE,
    XDCC_PA    = 0xDF,
    XPT_Sts    = 0xE0,
    XPT_On     = 0xE1,
    XPT_Off    = 0xE2,
    XPT_SXRd   = 0xE4,
    XPT_SXWr   = 0xE5,
    XPT_SXSr   = 0xE6,
    XPT_FMZSr  = 0xE7,
    XPT_FMZWr  = 0xE8,
    XPT_MrkSr  = 0xE9,
    XPT_DCCSr  = 0xEA,
    XPT_DCCQA  = 0xEB,
    XPT_DCCRR  = 0xEC,
    XPT_DCCWR  = 0xED,
    XPT_DCCRP  = 0xEE,
    XPT_DCCWP  = 0xEF,
    XPT_DCCRD  = 0xF0,
    XPT_DCCWD  = 0xF1,
    XPT_DCCRB  = 0xF2,
    XPT_DCCWB  = 0xF3,
    XPT_DCCQD  = 0xF4,
    XPT_DCCRL  = 0xF5,
    XPT_DCCWL  = 0xF6,
    XPT_DCCRA  = 0xF7,
    XPT_DCCWA  = 0xF8,
    XPT_U750   = 0xF9,
    XPT_U755   = 0xFA,
    XPT_U760   = 0xFB,
    XPT_Src    = 0xFC,
    XPT_Ctrl   = 0xFD,
    XPT_Term   = 0xFE
};

/* IB error codes */
enum {
    XBADPARAM  = 0x02,
    XPWOFF     = 0x06,
    XNOTSPC    = 0x07,
    XNOLSPC    = 0x08,
    XNODATA    = 0x0A,
    XNOSLOT    = 0x0B,
    XBADLNP    = 0x0C,
    XLKBUSY    = 0x0D,
    XBADTNP    = 0x0E,
    XBADSOV    = 0x0F,
    XNOISPC    = 0x10,
    XLOWTSP    = 0x40,
    XLKHALT    = 0x41,
    XLKPOFF    = 0x42
};

#endif
