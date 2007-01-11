/* cvs: $Id$ */

/*
* Vorliegende Software unterliegt der General Public License,
* Version 1.2 20060526: Text reformatting and error checkking
' Version 1.1 20060505: Configuration of fb adresses from srcpd.conf
* Version 1, 2005. (c) Gerard van der Sel, 2005-2006.
* Version 0.4: 20050521: Feedback responce
* Version 0.3: 20050521: Controlling a switch/signal
* Version 0.2: 20050514: Controlling a engine
* Version 0.1: 20050508: Connection with CC-2000 and power on/off
* Version 0.0: 20050501: Translated file from file M605X which compiles
*/

/*
 * This software does the translation for a selectrix centrol centre
 * An old centrol centre is the default selection
 * In the xml-file the control centre can be changed to the new CC-2000
 *   A CC-2000 can program a engine
 *   (Control centre of MUT and Uwe Magnus are CC-2000 compatible)
 */

/* Die Konfiguration des seriellen Ports von M6050emu (D. Schaefer)   */
/* wenngleich etwas ver�dert, mea culpa..                            */

#include "stdincludes.h"
#include "portio.h"
#include "config-srcpd.h"
#include "srcp-power.h"
#include "srcp-info.h"
#include "srcp-srv.h"
#include "srcp-error.h"
#include "srcp-gl.h"
#include "srcp-ga.h"
#include "srcp-fb.h"
#include "selectrix.h"
#include "ttycygwin.h"

/* Macro definition  */
#define __selectrix ((SELECTRIX_DATA*)busses[busnumber].driverdata)

/*******************************************************************
* readconfig_Selectrix:
* Reads selectrix specific xml nodes and sets up bus specific data.
* Called by register_bus().
********************************************************************/
int readconfig_Selectrix(xmlDocPtr doc, xmlNodePtr node,
                         long int busnumber)
{
	int i;
	int portindex;

	DBG(busnumber, DBG_ERROR, "Entering selectrix specific data for bus: %d", busnumber);
	portindex = 0;
	busses[busnumber].type = SERVER_SELECTRIX;
	busses[busnumber].baudrate = B9600;
	busses[busnumber].init_func = &init_bus_Selectrix;
	busses[busnumber].term_func = &term_bus_Selectrix;
	busses[busnumber].thr_func = &thr_commandSelectrix;
	busses[busnumber].thr_reader = &thr_processSelectrix;
	busses[busnumber].thr_timer = &thr_feedbackSelectrix;
	busses[busnumber].init_gl_func = &init_gl_Selectrix;
	busses[busnumber].init_ga_func = &init_ga_Selectrix;
	busses[busnumber].init_fb_func = &init_fb_Selectrix;
	busses[busnumber].driverdata = malloc(sizeof(struct _SELECTRIX_DATA));
	/* TODO: what happens if malloc returns NULL?  */
	__selectrix->number_gl = 0;
	__selectrix->number_ga = 0;
	__selectrix->number_fb = 0;
	__selectrix->flags = 0;
	__selectrix->number_adres = SXmax;

	for (i = 0; i < SXmax; i++)	{
		__selectrix->bus_data[i] = 0;         /* Set all outputs to 0 */
		__selectrix->fb_adresses[i] = 255;    /* Set invalid adresses */
	}

	strcpy(busses[busnumber].description,
		"GA GL FB POWER LOCK DESCRIPTION");

	xmlNodePtr child = node->children;
	xmlChar *txt = NULL;

	while (child != NULL) {
		if (xmlStrcmp(child->name, BAD_CAST "number_fb") == 0) {
			txt = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
			if (txt != NULL) {
				__selectrix->number_fb = atoi((char *) txt);
				xmlFree(txt);
			}
		}

		else if (xmlStrcmp(child->name, BAD_CAST "number_gl") == 0) {
			txt = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
			if (txt != NULL) {
				__selectrix->number_gl = atoi((char *) txt);
				xmlFree(txt);
			}
		}

		else if (xmlStrcmp(child->name, BAD_CAST "number_ga") == 0) {
			txt = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
			if (txt != NULL) {
				__selectrix->number_ga = atoi((char *) txt);
				xmlFree(txt);
			}
		}

		else if (xmlStrcmp(child->name, BAD_CAST "mode_cc2000") == 0) {
			txt = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
			if (txt != NULL) {
				if (xmlStrcmp(txt, BAD_CAST "yes") == 0) {
					__selectrix->flags |= CC2000_MODE;
					/* Last 8 addresses for the CC2000 */
					__selectrix->number_adres = SXcc2000;
					strcpy(busses[busnumber].description,
						"GA GL FB SM POWER LOCK DESCRIPTION");
				}
				xmlFree(txt);
			}
		}
		else if (xmlStrcmp(child->name, BAD_CAST "ports") == 0) {
			portindex = 1;
			xmlNodePtr subchild = child->children;
			xmlChar *subtxt = NULL;
			while (subchild != NULL) {
				if (xmlStrcmp(subchild->name, BAD_CAST "port") == 0) {
					subtxt = xmlNodeListGetString(doc,
						subchild->xmlChildrenNode, 1);
					if (subtxt != NULL) {
						__selectrix->fb_adresses[portindex] =
						atoi((char *) subtxt);
						DBG(busnumber, DBG_WARN,
							"Adding feedbackport number %d with adres %s.",
							portindex, subtxt);
						xmlFree(subtxt);
						if (__selectrix->number_fb > portindex) {
							portindex++;
						}
					}
				}
				else
					DBG(busnumber, DBG_WARN,
						"WARNING, unknown subtag found: \"%s\"!\n",
						subchild->name);
				subchild = subchild->next;
			}
		}
		else
			DBG(busnumber, DBG_WARN,
				"WARNING, unknown tag found: \"%s\"!\n",
				child->name);
		child = child->next;
	}

	if (__selectrix->number_gl + __selectrix->number_ga +
	    __selectrix->number_fb < __selectrix->number_adres) {
		if (init_GL(busnumber, __selectrix->number_gl)) {
		     __selectrix->number_gl = 0;
			DBG(busnumber, DBG_ERROR, "Can't create array for locomotivs");
		}

		if (init_GA(busnumber, __selectrix->number_ga)) {
		    __selectrix->number_ga = 0;
			DBG(busnumber, DBG_ERROR, "Can't create array for assesoirs");
		}
		if (init_FB(busnumber, __selectrix->number_fb * 8)) {
		     __selectrix->number_fb = 0;
			DBG(busnumber, DBG_ERROR, "Can't create array for feedback");
		}
	}
	else {
		__selectrix->number_gl = 0;
		__selectrix->number_ga = 0;
		__selectrix->number_fb = 0;
		DBG(busnumber, DBG_ERROR, "Too many devices on the SX-bus");
	}
	DBG(busnumber, DBG_WARN,
		"found number of loco %d", __selectrix->number_gl);
	DBG(busnumber, DBG_WARN,
		"found number of switches %d", __selectrix->number_ga);
	DBG(busnumber, DBG_WARN,
		"found number of feedbacks %d", __selectrix->number_fb);
	if (portindex!= 0) {
		for (i=1; i<= portindex; i++) {
			DBG(busnumber, DBG_WARN,
				"found feedbackport number %d with adres %d.",
				i, __selectrix->fb_adresses[i]);
		}
	}
	return 1;
}


/*
 *
 *
 */
long int init_bus_Selectrix(long int busnumber)
{
	DBG(busnumber, DBG_INFO, "Selectrix init: debug %d",
		busses[busnumber].debuglevel);
	if (busses[busnumber].debuglevel <= DBG_DEBUG)
	{
		open_port(busnumber);
	}
	else
	{
		busses[busnumber].fd = -1;
	}
	DBG(busnumber, DBG_INFO, "Selectrix init done, fd=%d",
		busses[busnumber].fd);
	DBG(busnumber, DBG_INFO, "Selectrix description: %s",
		busses[busnumber].description);
	DBG(busnumber, DBG_INFO, "Selectrix flags: %d",
		busses[busnumber].flags);
	return 0;
}

/*
 *
 *
 */
long int term_bus_Selectrix(long int busnumber)
{
	if (!(busses[busnumber].fd != -1))
	{
		close_port(busnumber);
	}
	DBG(busnumber, DBG_INFO, "Selectrix bus term done, "
		"fd=%d", busses[busnumber].fd);
	return 0;
}

/*******************************************************
*     Device initialisation
********************************************************/
/* Engines */
/* INIT <bus> GL <adr> S 1 31 2 */
long int init_gl_Selectrix(struct _GLSTATE *gl)
{
	if (gl->protocol != 'S')
	{
		return SRCP_UNSUPPORTEDDEVICEPROTOCOL;
	}
	return ((gl->n_fs == 31) && (gl->protocolversion == 1)
			&& (gl->n_func == 2)) ? SRCP_OK : SRCP_WRONGVALUE;
}

/* Switches, signals, ... */
/* INIT <bus> GA <adr> S */
long int init_ga_Selectrix(struct _GASTATE *ga)
{
	return (ga->protocol ==
			'S') ? SRCP_OK : SRCP_UNSUPPORTEDDEVICEPROTOCOL;
}

/* Feedbacks */
/* INIT <bus> FB <adr> S <index> */
long int init_fb_Selectrix(long int busnumber, int adres,
						   const char protocol, int index)
{
	if (protocol == 'S') {
		if ((__selectrix->number_adres > adres)
			&& (__selectrix->number_fb >= index))
		{
			__selectrix->fb_adresses[index] = adres;
			return SRCP_OK;
		}
		else
		{
			return SRCP_WRONGVALUE;
		}
	}
	return SRCP_UNSUPPORTEDDEVICEPROTOCOL;
}

/*******************************************************
*     Base communication with the interface (Selectrix)
********************************************************/
/* Read data from the SX-bus (8 bits) */
int readSXbus(long int busnumber)
{
	unsigned char rr;
	int temp;
	
	if (busses[busnumber].fd != -1 )
	{
		/* Wait until a character arrives */
		temp = read_port(busses[busnumber].fd, &rr);
		if (temp < 1)
		{
			rr= 0xff;			// If nothig receved all set
		}
		DBG(busnumber, DBG_INFO,
			"Selectrix on bus %d, read byte %d .",
			busnumber, rr);
		return rr;
	}
	else
	{
		return 0x100;				// If port closed > 255
	}
}

void commandreadSXbus(long int busnumber, int SXadres)
{
	if (busses[busnumber].fd != -1 ) {
		/* write SXadres and the read command */
		write_port(busnumber, SXread + SXadres);
		/* extra byte for power to receive data */
		write_port(busnumber, 0xaa);
		/* receive data */
	}
	else
	{
		DBG(busnumber, DBG_INFO,
			"Selectrix on bus %d, adres %d not read.",
			busnumber, SXadres);
	}
}

/* Write data to the SX-bus (8bits) */
void writeSXbus(long int busnumber, int SXadres, int SXdata)
{
	if (busses[busnumber].fd != -1)
	{
		DBG(busnumber, DBG_INFO,
			"Selectrix on bus %d, send byte %d to adres %d.",
			busnumber, SXdata, SXadres);
		/* write SXadres and the write command */
		write_port(busnumber, SXwrite + SXadres);
		/* write data to the SX-bus */
		write_port(busnumber, SXdata);
	}
	else
	{
		DBG(busnumber, DBG_INFO,
			"Selectrix on bus %d, byte %d to adres %d not send.",
			busnumber, SXdata, SXadres);
	}
}

/*******************************************************
*     Command generation (Selectrix)
********************************************************/
void *thr_commandSelectrix(void *v)
{
	int addr, data, power, state;
	struct _GLSTATE gltmp;
	struct _GASTATE gatmp;
	long int busnumber = (long int) v;
	
	state = 0;
	busses[busnumber].watchdog = 0;
	DBG(busnumber, DBG_INFO, "Selectrix on bus #%ld thread started.",
		busnumber);
	while (1)
	{
		busses[busnumber].watchdog = 1;
		/* Start/Stop */
		if (busses[busnumber].power_changed != 0)
		{
			state = 1;
			char msg[1000];
			busses[busnumber].watchdog = 2;
			power = ((busses[busnumber].power_state) ? 0x80 : 0x00);
			writeSXbus(busnumber, SXcontrol, power);
			infoPower(busnumber, msg);
			queueInfoMessage(msg);
			DBG(busnumber, DBG_DEBUG,
				"Selectrix on bus %d had a power change.", busnumber);
			busses[busnumber].power_changed = 0;
		}
		busses[busnumber].watchdog = 3;
		/* do nothing, if power off */
		if (busses[busnumber].power_state == 0)
		{
			/* Maybe programming */
			busses[busnumber].watchdog = 4;
			usleep(100000);
			continue;
		}
		/* Loco decoders */
		busses[busnumber].watchdog = 5;
		if (!queue_GL_isempty(busnumber))
		{
			state = 2;
			busses[busnumber].watchdog = 6;
			unqueueNextGL(busnumber, &gltmp);
			/* Adres of the engine */
			addr = gltmp.id;
			/* Check if valid address */
			if (__selectrix->number_adres > addr)
			{
				/* Check: terminating the engine */
				if (gltmp.state == 2) {
					DBG(busnumber, DBG_DEBUG,
						"Selectrix on bus %d, engine with address %d "
						"is removed", busnumber, addr);
					__selectrix->number_gl--;
				}
				/* Check: emergency stop */
				if (gltmp.direction == 2) {
					gltmp.speed = 0;
				}
				/*Speed, Direction, Light, Function */
				data = gltmp.speed + ((gltmp.direction) ? 0 : 0x20) +
					((gltmp.funcs & 0x01) ? 0x40 : 0) +
					((gltmp.funcs & 0x02) ? 0x80 : 0);
				writeSXbus(busnumber, addr, data);
				__selectrix->bus_data[addr] = data;
				setGL(busnumber, addr, gltmp);
				DBG(busnumber, DBG_DEBUG,
					"Selectrix on bus %d, engine with address %d has data %X.",
					busnumber, addr, data);
			}
			else
			{
				DBG(busnumber, DBG_DEBUG,
					"Selectrix on bus %d, invalid address %d with engine",
					busnumber, addr);
			}
		}
		busses[busnumber].watchdog = 7;
		/* Antriebe (solenoids and signals) */
		if (!queue_GA_isempty(busnumber))
		{
			state = 3;
			busses[busnumber].watchdog = 8;
			unqueueNextGA(busnumber, &gatmp);
			addr = gatmp.id;
			if (__selectrix->number_adres > addr)
			{
				data = __selectrix->bus_data[addr];
				DBG(busnumber, DBG_DEBUG,
					"Selectrix on bus %d, address %d has old data %X.",
					busnumber, addr, data);
				/* Select the action to do */
				if (gatmp.action == 0)
				{
					/* Set pin to "0" */
					switch (gatmp.port) {
						case 1:
							data &= 0xfe;
							break;
						case 2:
							data &= 0xfd;
							break;
						case 3:
							data &= 0xfb;
							break;
						case 4:
							data &= 0xf7;
							break;
						case 5:
							data &= 0xef;
							break;
						case 6:
							data &= 0xdf;
							break;
						case 7:
							data &= 0xbf;
							break;
						case 8:
							data &= 0x7f;
							break;
						default:
							DBG(busnumber, DBG_DEBUG,
							"Selectrix on bus %d, invalid portnumber "
							"%d with switch/signal or ...",
							busnumber,  gatmp.port);
							break;
					}
				}
				else
				{
					/* Set pin to "1" */
					switch (gatmp.port) {
						case 1:
							data |= 0x01;
							break;
						case 2:
							data |= 0x02;
							break;
						case 3:
							data |= 0x04;
							break;
						case 4:
							data |= 0x08;
							break;
						case 5:
							data |= 0x10;
							break;
						case 6:
							data |= 0x20;
							break;
						case 7:
							data |= 0x40;
							break;
						case 8:
							data |= 0x80;
							break;
						default:
							DBG(busnumber, DBG_DEBUG,
							"Selectrix on bus %d, invalid portnumber "
							"%d with switch/signal or ...",
							busnumber, gatmp.port);
							break;
					}
				}
				writeSXbus(busnumber, addr, data);
				__selectrix->bus_data[addr] = data;
				DBG(busnumber, DBG_DEBUG,
					"Selectrix on bus %d, address %d has new data %X.",
					busnumber, addr, data);
			}
			else
			{
				DBG(busnumber, DBG_DEBUG,
				"Selectrix on bus %d, invalid address %d with "
				"switch/signal or ...", busnumber, addr);
			}
		}
			busses[busnumber].watchdog = 9;
		/* Feed back contacts */
		if ((__selectrix->number_fb > 0) && (__selectrix->startFB == 1))
		{
			__selectrix->startFB = 0;
			state = 4;
			busses[busnumber].watchdog = 10;
			/* Fetch the modul address */
			addr = __selectrix->fb_adresses[__selectrix->currentFB];
			//DBG(busnumber, DBG_DEBUG,
			//	 "Selectrix on bus %d, feedbackaddress %d.",
			//	 busnumber, addr);
			if (__selectrix->number_adres > addr) {
				/* Read the SXbus */
				commandreadSXbus(busnumber, addr);
				 __selectrix->startFB = 2;
			}
			else
			{
				DBG(busnumber, DBG_DEBUG,
				"Selectrix on bus %d, invalid address %d with feedback index %d.",
				busnumber, addr, __selectrix->currentFB);
			}
		}
		if (state == 0)
		{
			/* Lock thread till new data to process arrives */
			suspendThread(busnumber);
		}
	}
}

/*******************************************************
*     Command processing (Selectrix)
********************************************************/
void *thr_processSelectrix(void *v)
{
	int data, addr;

	long int busnumber = (long int) v;
	while (1)
	{
		data = readSXbus(busnumber);
		if (__selectrix->startFB == 2)
		{
			/* Read the SXbus */
			if (data < 0x100)
			{
				addr = __selectrix->fb_adresses[__selectrix->currentFB];
				__selectrix->bus_data[addr] = data;
				/* Set the deamon global data */
				/* Use 1, 2, ... as adres for feedback */
				setFBmodul(busnumber, __selectrix->currentFB, data);
				/* Use real adres for feedback */
					//setFBmodul(busnumber, addr, data);
				DBG(busnumber, DBG_DEBUG,
					"Selectrix on bus %d, address %d has feedback data %X.",
					busnumber, addr, data);
			}
			 __selectrix->startFB = 0;
		}
	}
}

/*******************************************************
*     Command processing (Selectrix)
********************************************************/
void *thr_feedbackSelectrix(void *v)
{
	int addr;

	long int busnumber = (long int) v;
	__selectrix->currentFB = __selectrix->number_fb;
	while (1)
	{
		/* Feed back contacts */
		if (__selectrix->number_fb > 0)
		{
			// Select the next module
			if (__selectrix->currentFB >= __selectrix->number_fb)
			{
				__selectrix->currentFB = 1;   // Reset to start
			}
			else
			{
				__selectrix->currentFB++;    // Next
			}
			/* Fetch the modul address */
			addr = __selectrix->fb_adresses[__selectrix->currentFB];
			//DBG(busnumber, DBG_DEBUG,
			//	 "Selectrix on bus %d, feedbackaddress %d.",
			//	 busnumber, addr);
			if (__selectrix->number_adres > addr)
			{
				if (__selectrix->startFB == 0)
				{
					/* Let thread process a feedback */
					__selectrix->startFB = 1;
					resumeThread(busnumber);
				}
			}
			else
			{
				DBG(busnumber, DBG_DEBUG,
				"Selectrix on bus %d, invalid address %d with feedback index %d.",
				busnumber, addr, __selectrix->currentFB);
			}
			/* Process  every feedbackimes per second4 t */
			usleep(250000 / __selectrix->number_fb);
		}
		else
		{
			usleep(1000000);
		}
	}
}

