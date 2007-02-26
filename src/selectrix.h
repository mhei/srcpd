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
#define SXmax     112   /* Number of adresses on the SX-bus*/
#define SXcc2000    104   /* Number of adresses on the SX-bus with a CC-2000 */

/* Adressses on the SX_bus */
#define SXcontrol   127   /* Control adres (all SX controllers) */
#define SXstatus    109   /* Status adres (CC-2000) */
#define SXcommand   106   /* Command adres (CC-2000) */
#define SXprog2     105   /* Program parameter 2 (CC-2000) */
#define SXporg1     104   /* Program parameter 1 (CC-2000) */

/* Central is a special */
#define CC2000_MODE   0x0001    /* CC2000 has restricted bus space and can program decoders */
#define Rautenhause_MODE   0x0002    /* Rautenhaus has special feedback support */

/* Array with the size of an SX-bus */
typedef int SX_BUS[SXmax];

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
