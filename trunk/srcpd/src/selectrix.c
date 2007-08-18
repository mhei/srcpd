/* cvs: $Id$ */

/*
* This software is published under the terms of the GNU General Public
* License, Version ?
*
* Version 2.0 20070418: Release of SLX852
* Version 1.4 20070315: Communication with the SLX852
* Version 1.3 20070213: Communication with events
* Version 1.2 20060526: Text reformatting and error checking
' Version 1.1 20060505: Configuration of fb addresses from srcpd.conf
* Version 1, 2005. (c) Gerard van der Sel, 2005-2006.
* Version 0.4: 20050521: Feedback response
* Version 0.3: 20050521: Controlling a switch/signal
* Version 0.2: 20050514: Controlling a engine
* Version 0.1: 20050508: Connection with CC-2000 and power on/off
* Version 0.0: 20050501: Translated file from file M605X which compiles
*/

/*
 * This software does the translation for a selectrix central centre
 * An old Central centre is the default selection
 * In the xml-file the control centre can be changed to the new CC-2000
 *   A CC-2000 can program a engine
 *   (Control centre of MUT and Uwe Magnus are CC-2000 compatible)
 */

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
int readconfig_Selectrix(xmlDocPtr doc, xmlNodePtr node, bus_t busnumber)
{
	int i, offset;
	int portindex = 0;

	DBG(busnumber, DBG_ERROR, "Entering selectrix specific data "
                "for bus: %d", busnumber);

	busses[busnumber].driverdata = malloc(sizeof(struct _SELECTRIX_DATA));
        if (busses[busnumber].driverdata == NULL) {
            DBG(busnumber, DBG_ERROR,
                    "Memory allocation error in module '%s'.", node->name);
            return 0;
        }

	busses[busnumber].type = SERVER_SELECTRIX;
	busses[busnumber].baudrate = B9600;
	busses[busnumber].init_func = &init_bus_Selectrix;
	busses[busnumber].term_func = &term_bus_Selectrix;
	busses[busnumber].thr_func = &thr_commandSelectrix;
	busses[busnumber].thr_timer = &thr_feedbackSelectrix;
	busses[busnumber].sig_reader = &sig_processSelectrix;
	busses[busnumber].init_gl_func = &init_gl_Selectrix;
	busses[busnumber].init_ga_func = &init_ga_Selectrix;
	busses[busnumber].init_fb_func = &init_fb_Selectrix;
	__selectrix->number_gl = 0;
	__selectrix->number_ga = 0;
	__selectrix->number_fb = 0;
	__selectrix->flags = 0;
	__selectrix->number_adres = SXmax;

	for (i = 0; i < SXmax; i++)
	{
		__selectrix->bus_data[i] = 0;         /* Set all outputs to 0 */
		__selectrix->fb_adresses[i] = 255;    /* Set invalid addresses */
	}

	strcpy(busses[busnumber].description,
		"GA GL FB POWER LOCK DESCRIPTION");

	xmlNodePtr child = node->children;
	xmlChar *txt = NULL;

	while (child != NULL)
	{
            if (xmlStrncmp(child->name, BAD_CAST "text", 4) == 0) {
                /* just do nothing, it is only a comment */
            }

            else if (xmlStrcmp(child->name, BAD_CAST "number_fb") == 0)
		{
			txt = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
			if (txt != NULL)
			{
				__selectrix->number_fb = atoi((char *) txt);
				xmlFree(txt);
			}
		}

		else if (xmlStrcmp(child->name, BAD_CAST "number_gl") == 0)
		{
			txt = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
			if (txt != NULL)
			{
				__selectrix->number_gl = atoi((char *) txt);
				xmlFree(txt);
			}
		}

		else if (xmlStrcmp(child->name, BAD_CAST "number_ga") == 0)
		{
			txt = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
			if (txt != NULL)
			{
				__selectrix->number_ga = atoi((char *) txt);
				xmlFree(txt);
			}
		}
		else if (xmlStrcmp(child->name, BAD_CAST "controller") == 0)
		{
			txt = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
			if (txt != NULL)
			{
				if (xmlStrcmp(txt, BAD_CAST "CC2000") == 0)
				{
					__selectrix->flags |= CC2000_MODE;
					/* Last 8 addresses for the CC2000 */
					__selectrix->number_adres = SXcc2000;
					strcpy(busses[busnumber].description,
						"GA GL FB SM POWER LOCK DESCRIPTION");
				}
			}
		}
		else if (xmlStrcmp(child->name, BAD_CAST "interface") == 0)
		{
			txt = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
			if (txt != NULL)
			{
				if (xmlStrcmp(txt, BAD_CAST "RTHS_0") == 0)
				{
					/* Set interface according selectrix interface */
					__selectrix->flags |= Rautenhaus_MODE;
					/* rest of the flags are zero */
				}
				else if (xmlStrcmp(txt, BAD_CAST "RTHS_1") == 0)
				{
					/* Never select this since it is not supported */
					__selectrix->flags |= Rautenhaus_MODE;
					__selectrix->flags |= Rautenhaus_DBL;
				}
				else if (xmlStrcmp(txt, BAD_CAST "RTHS_2") == 0)
				{
					/* Select automatic feedback reporting */
					__selectrix->flags |= Rautenhaus_MODE;
					__selectrix->flags |= Rautenhaus_FDBCK;
					__selectrix->flags |= Rautenhaus_ADR;
				}
				else if (xmlStrcmp(txt, BAD_CAST "RTHS_3") == 0)
				{
					/* Select automatic feedback reporting */
					__selectrix->flags |= Rautenhaus_MODE;
					__selectrix->flags |= Rautenhaus_DBL;
					__selectrix->flags |= Rautenhaus_FDBCK;
					__selectrix->flags |= Rautenhaus_ADR;
				}
				xmlFree(txt);
			}
		}
		else if (xmlStrcmp(child->name, BAD_CAST "ports") == 0)
		{
			portindex = 1;
			xmlNodePtr subchild = child->children;
			xmlChar *subtxt = NULL;
			while (subchild != NULL)
			{
                            if (xmlStrncmp(subchild->name, BAD_CAST "text", 4) == 0) {
                                /* just do nothing, it is only a comment */
                            }
                            else if (xmlStrcmp(subchild->name, BAD_CAST "port") == 0)
				{
					/* Check if on the second SX-bus */
					offset = 0;
					xmlChar* pOffset = xmlGetProp(subchild, BAD_CAST "sxbus");
					if (pOffset != NULL )
						if (atoi((char *) pOffset) == 1)
							offset = 128;
					free(pOffset);
					/* Get address */
					subtxt = xmlNodeListGetString(doc,
						subchild->xmlChildrenNode, 1);
					if (subtxt != NULL)
					{
						/* Store address plus SXbus number */
						__selectrix->fb_adresses[portindex] =
						atoi((char *) subtxt) + offset;
						DBG(busnumber, DBG_WARN,
							"Adding feedback port number %d with address %s.",
							portindex, subtxt + offset);
						xmlFree(subtxt);
						if (__selectrix->number_fb > portindex)
						{
							portindex++;
						}
					}
				}
				else
					DBG(busnumber, DBG_WARN,
						"WARNING, unknown sub tag found: \"%s\"!\n",
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
	    __selectrix->number_fb < __selectrix->number_adres)
		{
		if (init_GL(busnumber, __selectrix->number_gl))
		{
		     __selectrix->number_gl = 0;
			DBG(busnumber, DBG_ERROR, "Can't create array for locomotives");
		}

		if (init_GA(busnumber, __selectrix->number_ga))
		{
		    __selectrix->number_ga = 0;
			DBG(busnumber, DBG_ERROR, "Can't create array for assesoirs");
		}
		if (init_FB(busnumber, __selectrix->number_fb * 8))
		{
		     __selectrix->number_fb = 0;
			DBG(busnumber, DBG_ERROR, "Can't create array for feedback");
		}
	}
	else
	{
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
		"found number of feedback modules %d", __selectrix->number_fb);
	if (portindex!= 0)
	{
		for (i=1; i<= portindex; i++) {
			DBG(busnumber, DBG_WARN,
				"found feedback port number %d with address %d.",
				i, __selectrix->fb_adresses[i]);
		}
	}
	return 1;
}


/*********************************************************************************
 * Opens a serial port for communication with a selectrix interface
 * On success the port handle is changed to a value <> -1
 *********************************************************************************/
int init_bus_Selectrix(bus_t busnumber)
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

/********************************************************************************
 * Closes a serial port. Restores old OS values
 *
 ********************************************************************************/
int term_bus_Selectrix(bus_t busnumber)
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
int init_gl_Selectrix(struct _GLSTATE *gl)
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
int init_ga_Selectrix(struct _GASTATE *ga)
{
	return (ga->protocol ==
			'S') ? SRCP_OK : SRCP_UNSUPPORTEDDEVICEPROTOCOL;
}

/* Feedback modules */
/* INIT <bus> FB <adr> S <index> */
int init_fb_Selectrix(bus_t busnumber, int adres,
						   const char protocol, int index)
{
	if (protocol == 'S')
	{
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
int readSXbus(bus_t busnumber)
{
	unsigned char rr;
	
	if (busses[busnumber].fd != -1 )
	{
		/* Wait until a character arrives */
		if (!read_port(busses[busnumber].fd, &rr))
		{
			rr= 0xff;			// If nothing received all set
		}
		DBG(busnumber, DBG_DEBUG,
			"Selectrix on bus %ld, read byte %d .",
			busnumber, rr);
		return rr;
	}
	else
	{
		return 0x100;				// If port closed > 255
	}
}

void commandreadSXbus(bus_t busnumber, int SXadres)
{
	if (busses[busnumber].fd != -1 )
	{
		/* write SX-address and the read command */
		write_port(busnumber, SXread + SXadres);
		/* extra byte for power to receive data */
		write_port(busnumber, 0xaa);
		/* receive data */
	}
	else
	{
		DBG(busnumber, DBG_DEBUG,
			"Selectrix on bus %ld, address %d not read.",
			busnumber, SXadres);
	}
}

/* Write data to the SX-bus (8bits) */
void writeSXbus(bus_t busnumber, int SXadres, int SXdata)
{
	if (busses[busnumber].fd != -1)
	{
		DBG(busnumber, DBG_DEBUG,
			"Selectrix on bus %ld, send byte %d to address %d.",
			busnumber, SXdata, SXadres);
		/* write SX-address and the write command */
		write_port(busnumber, SXwrite + SXadres);
		/* write data to the SX-bus */
		write_port(busnumber, SXdata);
	}
	else
	{
		DBG(busnumber, DBG_DEBUG,
			"Selectrix on bus %ld, byte %d to address %d not send.",
			busnumber, SXdata, SXadres);
	}
}

/*******************************************************
*     Rautenhaus setup
********************************************************/
/* Make configuration byte for address 126 */
void confRautenhaus(bus_t busnumber)
{
	int configuration;

	configuration = 0;
	if ((__selectrix->flags && Rautenhaus_FDBCK) == Rautenhaus_FDBCK)
	{
		configuration |= (cntrlON + fdbckON);
		if ((__selectrix->flags && CC2000_MODE) == CC2000_MODE)
		{
			/* Selectrix central unit, don't check address 111 */
			/* Don't check bus 0 */
			configuration |= clkOFF0;
			if ((__selectrix->flags && Rautenhaus_DBL) == Rautenhaus_DBL)
				/* Don't check bus 1 */
				configuration |= clkOFF1;
		}
	}
	else
		configuration |= (cntrlOFF + fdbckOFF);
	/* Write configuration to the device */
	writeSXbus(busnumber, RautenhsCC, configuration);
}

/*******************************************************
*     Command generation (Selectrix)
********************************************************/
void *thr_commandSelectrix(void *v)
{
	int addr, data, power, state;
	struct _GLSTATE gltmp;
	struct _GASTATE gatmp;
	bus_t busnumber = (bus_t) v;
	
	state = 0;
	busses[busnumber].watchdog = 0;
	DBG(busnumber, DBG_INFO, "Selectrix on bus #%ld command thread started.",
		busnumber);
	while (1)
	{
		busses[busnumber].watchdog = 1;
		/* Start/Stop */
		if (busses[busnumber].power_changed == 1)
		{
			state = 1;
			char msg[1000];
			busses[busnumber].watchdog = 2;
			power = ((busses[busnumber].power_state) ? 0x80 : 0x00);
			writeSXbus(busnumber, SXcontrol, power);
			infoPower(busnumber, msg);
			queueInfoMessage(msg);
			DBG(busnumber, DBG_DEBUG,
				"Selectrix on bus %ld had a power change.", busnumber);
			busses[busnumber].power_changed = 0;
			if ((__selectrix->flags && Rautenhaus_MODE) == Rautenhaus_MODE)
				confRautenhaus(busnumber);
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
			/* Address of the engine */
			addr = gltmp.id;
			/* Check if valid address */
			if (__selectrix->number_adres > addr)
			{
				/* Check: terminating the engine */
				if (gltmp.state == 2)
				{
					DBG(busnumber, DBG_DEBUG,
						"Selectrix on bus %ld, engine with address %d "
						"is removed", busnumber, addr);
					__selectrix->number_gl--;
				}
				/* Check: emergency stop */
				if (gltmp.direction == 2)
				{
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
					"Selectrix on bus %ld, engine with address %d has data %X.",
					busnumber, addr, data);
			}
			else
			{
				DBG(busnumber, DBG_DEBUG,
					"Selectrix on bus %ld, invalid address %d with engine",
					busnumber, addr);
			}
		}
		busses[busnumber].watchdog = 7;
		/* drives (solenoids and signals) */
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
					"Selectrix on bus %ld, address %d has old data %X.",
					busnumber, addr, data);
				/* Select the action to do */
				if (gatmp.action == 0)
				{
					/* Set pin to "0" */
					switch (gatmp.port)
					{
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
							"Selectrix on bus %ld, invalid port number "
							"%d with switch/signal or ...",
							busnumber,  gatmp.port);
							break;
					}
				}
				else
				{
					/* Set pin to "1" */
					switch (gatmp.port)
					{
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
							"Selectrix on bus %ld, invalid port number "
							"%d with switch/signal or ...",
							busnumber, gatmp.port);
							break;
					}
				}
				writeSXbus(busnumber, addr, data);
				__selectrix->bus_data[addr] = data;
				DBG(busnumber, DBG_DEBUG,
					"Selectrix on bus %ld, address %d has new data %X.",
					busnumber, addr, data);
			}
			else
			{
				DBG(busnumber, DBG_DEBUG,
				"Selectrix on bus %ld, invalid address %d with "
				"switch/signal or ...", busnumber, addr);
			}
		}
		busses[busnumber].watchdog = 9;
		/* Feed back contacts */
		if ((__selectrix->number_fb > 0) && (__selectrix->startFB == 1))
		{
			state = 4;
			busses[busnumber].watchdog = 10;
			/* Fetch the module address */
			addr = __selectrix->fb_adresses[__selectrix->currentFB];
			//DBG(busnumber, DBG_DEBUG,
			//	 "Selectrix on bus %ld, feedback address %d.",
			//	 busnumber, addr);
			if (__selectrix->number_adres > addr) {
				/* Read the SX-bus */
				commandreadSXbus(busnumber, addr);
				 __selectrix->startFB = 2;
			}
			else
			{
				__selectrix->startFB = 0;
				DBG(busnumber, DBG_DEBUG,
				"Selectrix on bus %ld, invalid address %d with feedback index %d.",
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
*     Timed command generation (Selectrix)
********************************************************/
void *thr_feedbackSelectrix(void *v)
{
	int addr;

	bus_t busnumber = (bus_t) v;
	DBG(busnumber, DBG_INFO, "Selectrix on bus #%ld feedback thread started.",
		busnumber);
	__selectrix->currentFB = __selectrix->number_fb;
	while (1)
	{
		/* Feed back contacts */
		if (__selectrix->number_fb > 0)
		{
			if (__selectrix->startFB == 0)
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
				/* Fetch the module address */
				addr = __selectrix->fb_adresses[__selectrix->currentFB];
				if (__selectrix->number_adres > addr)
				{
					//DBG(busnumber, DBG_DEBUG,
					//	 "Selectrix on bus %ld, feedback address %d.",
					//	 busnumber, addr);
					/* Let thread process a feedback */
					__selectrix->startFB = 1;
					resumeThread(busnumber);
				}
				else
				{
					DBG(busnumber, DBG_INFO,
					"Selectrix on bus %ld, invalid address %d with feedback index %d.",
					busnumber, addr, __selectrix->currentFB);
				}
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

/*******************************************************
*     Command processing (Selectrix)
********************************************************/
void sig_processSelectrix(bus_t busnumber)
{
	int data, addr;
	int found;

	DBG(busnumber, DBG_INFO, "Selectrix on bus #%ld signal processed.",
		busnumber);
	/* Read the SX-bus */
	data = readSXbus(busnumber);
	if ((__selectrix->flags && Rautenhaus_FDBCK) == Rautenhaus_FDBCK)
	{
		if ((__selectrix->flags && Rautenhaus_ADR) == Rautenhaus_ADR)
		{ /* 1: data represents a received address */
			found = TRUE;
			__selectrix->currentFB = 1;
			while (found == TRUE)
			{
				if (__selectrix->fb_adresses[__selectrix->currentFB] == data)
				{
					found = FALSE;
				}
				else
				{
					++__selectrix->currentFB;
				}
			}
			__selectrix->flags &= ~Rautenhaus_ADR;
		}
		else
		{ /* 0: data represents received data*/
			addr = __selectrix->fb_adresses[__selectrix->currentFB];
			__selectrix->bus_data[addr] = data;
			/* Set the daemon global data */
			/* Use 1, 2, ... as address for feedback */
			setFBmodul(busnumber, __selectrix->currentFB, data);
			/* Use real address for feedback */
			//setFBmodul(busnumber, addr, data);
			DBG(busnumber, DBG_INFO,
				"Selectrix on bus %ld, address %d has feedback data %X.",
					busnumber, addr, data);
			__selectrix->flags |= Rautenhaus_ADR;
		}
	}
	else if (__selectrix->startFB == 2)
	{
		if (data < 0x100)
		{
			addr = __selectrix->fb_adresses[__selectrix->currentFB];
			__selectrix->bus_data[addr] = data;
			/* Set the daemon global data */
			/* Use 1, 2, ... as address for feedback */
			setFBmodul(busnumber, __selectrix->currentFB, data);
			/* Use real address for feedback */
			//setFBmodul(busnumber, addr, data);
			DBG(busnumber, DBG_INFO,
				"Selectrix on bus %ld, address %d has feedback data %X.",
					busnumber, addr, data);
			 __selectrix->startFB = 0;
		}
		else
		{
			DBG(busnumber, DBG_INFO,
				"Selectrix on bus %ld, discarded invalid data %X.",
				busnumber, data);
		}
	}
	else
	{
		DBG(busnumber, DBG_INFO,
			"Selectrix on bus %ld, discarded data %X.",
			busnumber, data);
	}
}

