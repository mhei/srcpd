/*
 * Vorliegende Software unterliegt der General Public License,
 * Version 2, 1991. (c) Gerard van der Sel, 2005-2006.
 *
 */


#ifndef _Selectrix_H
#define _Selectrix_H

#include <libxml/tree.h>

/* Read and Write command for the interface (Send with the adres) */
#define SXread      0x00    /* Lese befehl fur Selectrixbus */
#define SXwrite     0x80    /* Schreib befehl fur Selectrixbus */

/* Maximum number off adresses on the SX-bus */
#define SXmax       112   /* Number of adresses on the SX-bus*/
#define SXcc2000    104   /* Number of adresses on the SX-bus with a CC-2000 */

/* Adressses on the SX_bus */
#define SXcontrol   127   /* Power on/off adres (all SX controllers) */
#define RautenhsCC  126   /* Rautenhaus control address */

/* Control adddresses for a CC2000 */
#define SXstatus    109   /* Status adres */
#define SXcommand   106   /* Command adres */
#define SXprog2     105   /* Program parameter 2 */
#define SXporg1     104   /* Program parameter 1 */

/* Rautenhuas control bits (for adres 126) */
#define cntrlON     128   /* Start Rautenhaus protocol */
#define cntrlOFF     64   /* Stop Rautenhaus protocol */
#define fdbckON      32   /* Changes on then SX bus are reported automaticaly */
#define fdbckOFF     16   /* Stop reporting changes on the SX bus */
#define clkOFF0       8   /* Don't check adres 111 (clock) on bus 1 */
#define clkOFF1       4   /* Don't check adres 111 (clock) on bus 0 */

/* Constants for flags */
/* Central is a special */
#define CC2000_MODE       0x0001    /* CC2000 has restricted bus space and can program decoders */
#define Rautenhaus_MODE   0x0002    /* Rautenhaus has special feedback support */

#define Rautenhaus_FDBCK  0x0010    /* Automatic feedback report */
#define Rautenhaus_DBL    0x0020    /* Two busses are controlled */
#define Rautenhaus_ADR    0x0040    /* Last byte was an address */

/* Array with the size of an SX-bus */
/* Array with the size of two SX-busses  */
//typedef int SX_BUS[SXmax];
typedef int SX_BUS[256];     /* Space for adresses/data on the SX-bus */

/* Structure of a Selectrix bus */
typedef struct _SELECTRIX_DATA {
    int number_fb;
    int number_ga;
    int number_gl;
    int number_adres;
    int flags;
    SX_BUS fb_adresses;
    SX_BUS bus_data;
    /* Counter for adres of feedback */
    int startFB;
    int currentFB;                             //! holds current number of the feedback
} SELECTRIX_DATA;

/* Initialisation of the structure */
int readconfig_Selectrix(xmlDocPtr doc, xmlNodePtr node, bus_t busnumber);

/* Methods for Selectrix */
int init_lineSelectrix(bus_t bus);
int init_bus_Selectrix(bus_t bus);
int term_bus_Selectrix(bus_t bus);
int init_gl_Selectrix(struct _GLSTATE *gl);
int init_ga_Selectrix(struct _GASTATE *ga);
int init_fb_Selectrix(bus_t busnumber, int addr, const char protocol, int index);
void *thr_commandSelectrix(void *);
void *thr_feedbackSelectrix(void *);
void sig_processSelectrix(bus_t busnumber);

#endif
