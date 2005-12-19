/* cvs: $Id$             */

/*
* Vorliegende Software unterliegt der General Public License,
* Version 1, 2005. (c) Gerard van der Sel, 2005-2006.
* Version 0.4: 20050521: Feedback responce
* Version 0.3: 20050521: Controlling a switch/signal
* Version 0.2: 20050514: Controlling a engine
* Version 0.1: 20050508: Connection with CC-2000 and power on/off
* Version 0.0: 20050501: Translated file from file M605X which compiles
*/

/*
 * This software does the translation for a selectrix centrol centre
 * A old centrol centre is the default selection
 * In the xml-file the control centre can be changed to the new CC-2000
 *   A CC-2000 can program a engine
 *   (Control centre of MUT and Uwe Magnus are CC-2000 compatible)
 */

/* Die Konfiguration des seriellen Ports von M6050emu (D. Schaefer)   */
/* wenngleich etwas ver�dert, mea culpa..                            */

#include "stdincludes.h"
#include "io.h"
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

/*******************************************************
*  readconfig_selectrix: liest den Teilbaum der xml Configuration und parametriert
*  den busspezifischen Datenteil, wird von register_bus() aufgerufen 
********************************************************/
int readconfig_Selectrix(xmlDocPtr doc, xmlNodePtr node, int busnumber)
{
	int i;
	
	busses[busnumber].type= SERVER_SELECTRIX;
	busses[busnumber].baudrate= B9600;
	busses[busnumber].init_func= &init_bus_Selectrix;
	busses[busnumber].term_func= &term_bus_Selectrix;
	busses[busnumber].thr_func= &thr_sendrec_Selectrix;
	busses[busnumber].init_gl_func= &init_gl_Selectrix;
	busses[busnumber].init_ga_func= &init_ga_Selectrix;
	busses[busnumber].init_fb_func= &init_fb_Selectrix;
	busses[busnumber].driverdata= malloc(sizeof(struct _SELECTRIX_DATA));
	__selectrix->number_gl= SXmax;
	__selectrix->number_ga= SXmax;
	__selectrix->number_fb= SXmax;
	__selectrix->flags= 0;
	__selectrix->number_adres = SXmax;
        
	for (i = 0; i < SXmax; i++)
	{
		__selectrix->bus_data[i]= 0;         /* Set all outputs to 0 */
		__selectrix->fb_adresses[i]= 255;    /* Set invalid adresses */
	}
        
	strcpy(busses[busnumber].description, "GA GL FB POWER LOCK DESCRIPTION");

	xmlNodePtr child = node->children;
        xmlChar *txt = NULL;

        while (child)
        {
            if (xmlStrncmp(child->name, BAD_CAST "text", 4) == 0)
            {
                child = child->next;
                continue;
            }

            if (xmlStrcmp(child->name, BAD_CAST "number_fb") == 0)
            {
                txt = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
                if (txt != NULL) {
                    __selectrix->number_fb = atoi((char*) txt);
                    xmlFree(txt);
                }
            }

            if (xmlStrcmp(child->name, BAD_CAST "number_gl") == 0)
            {
                txt = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
                if (txt != NULL) {
                    __selectrix->number_gl = atoi((char*) txt);
                    xmlFree(txt);
                }
            }

            if (xmlStrcmp(child->name, BAD_CAST "number_ga") == 0)
            {
                txt = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
                if (txt != NULL) {
                    __selectrix->number_ga = atoi((char*) txt);
                    xmlFree(txt);
                }
            }

            if (xmlStrcmp(child->name, BAD_CAST "mode_cc2000") == 0)
            {
                txt = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
                if (txt != NULL) {
                    if (xmlStrcmp(txt, BAD_CAST "yes") == 0)
                    {
                        __selectrix->flags |= CC2000_MODE;
                        __selectrix->number_adres = SXcc2000;     /* Last 8 adresses for the CC2000 */
                        __selectrix->number_gl= SXcc2000;
                        __selectrix->number_ga= SXcc2000;
                        __selectrix->number_fb= SXcc2000;	
                        strcpy(busses[busnumber].description, "GA GL FB SM POWER LOCK DESCRIPTION");
                    }
                    xmlFree(txt);
                }
            }
            child = child->next;
        }
        
	if (init_GL(busnumber, __selectrix->number_gl))
	{
		__selectrix->number_gl= 0;
		DBG(busnumber, DBG_ERROR, "Can't create array for locomotivs");
	}
        
	if (init_GA(busnumber, __selectrix->number_ga))
	{
		__selectrix->number_ga= 0;
		DBG(busnumber, DBG_ERROR, "Can't create array for assesoirs");
	}
        
	if (init_FB(busnumber, __selectrix->number_fb* 8))
	{
		__selectrix->number_fb= 0;
		DBG(busnumber, DBG_ERROR, "Can't create array for feedback");
	}

	return 1;
}


/*******************************************************
*     SERIELLE SCHNITTSTELLE KONFIGURIEREN
********************************************************/
int init_lineSelectrix(int busnumber)
{
	int FD;
	struct termios interface;

	if (busses[busnumber].debuglevel>0)
	{
		DBG(busnumber, DBG_INFO, "Opening Selectrix: %s", busses[busnumber].device);
	}
	if ((FD = open(busses[busnumber].device, O_RDWR | O_NONBLOCK)) == -1)
	{
		DBG(busnumber, DBG_FATAL, "couldn't open device %s.", busses[busnumber].device);
		return -1;
	}
	tcgetattr(FD, &interface);
#ifdef linux
	interface.c_cflag = CS8 | CRTSCTS | CREAD | CSTOPB;
	interface.c_oflag = ONOCR | ONLRET;
	interface.c_oflag &= ~(OLCUC | ONLCR | OCRNL);
	interface.c_iflag = IGNBRK | IGNPAR;
	interface.c_iflag &= ~(ISTRIP | IXON | IXOFF | IXANY);
	interface.c_lflag = NOFLSH | IEXTEN;
	interface.c_lflag &= ~(ISIG | ICANON | ECHO | ECHOE | TOSTOP | PENDIN);

	cfsetospeed(&interface, busses[busnumber].baudrate );
	cfsetispeed(&interface, busses[busnumber].baudrate );
#else
	cfmakeraw(&interface);
	interface.c_ispeed = interface.c_ospeed = busses[busnumber].baudrate ;
	interface.c_cflag = CREAD  | HUPCL | CS8 | CSTOPB | CRTSCTS;
#endif
	tcsetattr(FD, TCSANOW, &interface);
	DBG(busnumber, DBG_INFO, "Opening Selectrix succeeded FD=%d", FD);
	return FD;
}

int init_bus_Selectrix(int busnumber)
{
	DBG(busnumber, DBG_INFO, "Selectrix init: debug %d", busses[busnumber].debuglevel);
	if(busses[busnumber].debuglevel<= DBG_DEBUG)
	{
		busses[busnumber].fd = init_lineSelectrix(busnumber);
	}
	else
	{
		busses[busnumber].fd = -1;
	}
	DBG(busnumber, DBG_INFO, "Selectrix init done, fd=%d", busses[busnumber].fd);
	DBG(busnumber, DBG_INFO, "Selectrix description: %s", busses[busnumber].description);
	DBG(busnumber, DBG_INFO, "Selectrix flags: %d", busses[busnumber].flags);
	return 0;
}

int term_bus_Selectrix(int busnumber)
{
	DBG(busnumber, DBG_INFO, "Selectrix bus term done, fd=%d",  busses[busnumber].fd);
	return 0;
}

/*******************************************************
*     Device initialisation
********************************************************/
/* Engines */
int init_gl_Selectrix(struct _GLSTATE *gl)
{
	if (gl->protocol!= 'S')
	{
		return SRCP_UNSUPPORTEDDEVICEPROTOCOL;
	}
      return ((gl->n_fs== 31)&& (gl->protocolversion== 1)&& (gl->n_func== 2))? SRCP_OK: SRCP_WRONGVALUE;
}

/* Switches, signals, ... */
int init_ga_Selectrix(struct _GASTATE *ga)
{
	return (ga->protocol== 'S')? SRCP_OK: SRCP_UNSUPPORTEDDEVICEPROTOCOL;
}

/* Feedbacks */
int init_fb_Selectrix(int busnumber, int adres, const char protocol, int index)
{
	if (protocol== 'S')
	{
		if ((__selectrix->number_adres> adres)&& (__selectrix->number_fb>= index))
		{
			__selectrix->fb_adresses[index]= adres;
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
int readSXbus(int busnumber, int SXadres)
{
	unsigned char rr;
	int temp;

	/* Make sure the connection is empty */
	ioctl(busses[busnumber].fd, FIONREAD, &temp);
	while (temp> 0)
	{
		readByte(busnumber, 0, &rr);
		ioctl(busses[busnumber].fd, FIONREAD, &temp);
		DBG(busnumber, DBG_INFO, "Selectrix on bus %d, ignoring unread byte: %d", busnumber, rr);
	}
	/* write SXadres and the read command */
	writeByte(busnumber, SXread+ SXadres, 0);
	/* extra byte for power to receive data */
	writeByte(busnumber, 0xaa, 0);
	/* receive data */
	readByte(busnumber, 0, &rr);
	return rr;
}

/* Write data to the SX-bus (8bits) */
void writeSXbus(int busnumber, int SXadres, int SXdata)
{
	/* write SXadres and the write command */
	writeByte(busnumber, SXwrite+ SXadres, 0);
	/* write data to the SX-bus */
	writeByte(busnumber, SXdata, 0);
}

/*******************************************************
*     Command processing (Selectrix)
********************************************************/
void* thr_sendrec_Selectrix(void *v)
{
	int addr, data, power, actFB;
	struct _GLSTATE gltmp;
	struct _GASTATE gatmp;
	int busnumber = (int) v;

	actFB= 1;
	busses[busnumber].watchdog= 1;
	DBG(busnumber, DBG_INFO, "Selectrix on bus %d thread started.", busnumber);
	while (1)
	{
		busses[busnumber].watchdog= 1;
		/* Start/Stop */
		if (busses[busnumber].power_changed!= 0)
		{
			char msg[1000];
			busses[busnumber].watchdog= 2;
			power= ((busses[busnumber].power_state) ? 0x80 : 0x00);
			writeSXbus(busnumber, SXcontrol, power);
			infoPower(busnumber, msg);
			queueInfoMessage(msg);
			DBG(busnumber, DBG_INFO, "Selectrix on bus %d had a power change.", busnumber);
			busses[busnumber].power_changed= 0;
		}
		busses[busnumber].watchdog= 3;
		/* do nothing, if power off */
		if (busses[busnumber].power_state== 0)
		{
			busses[busnumber].watchdog= 4;
			usleep(1000);
			continue;
		}
		/* Lokdecoder */
		busses[busnumber].watchdog= 5;
		if (!queue_GL_isempty(busnumber))
		{
			busses[busnumber].watchdog= 6;
			unqueueNextGL(busnumber, &gltmp);
			/* Adres of the engine */
			addr= gltmp.id;
			/* Check if valid adres */
			if (__selectrix->number_adres> addr)
			{
				/* Check: terminating the engine */
				if (gltmp.state== 2)
				{
					DBG(busnumber, DBG_INFO,
                                                "Selectrix on bus %d, engine with address %d is removed", busnumber, addr);
					__selectrix->number_gl--;
				}
				/* Check: emergency stop */
				if (gltmp.direction== 2)
				{
					gltmp.speed= 0;
				}
				/*      Speed,                   Direction,                     Light                          Function           */
				data= gltmp.speed+ ((gltmp.direction )? 0: 0x20)+ ((gltmp.funcs & 0x01)? 0x40: 0)+ ((gltmp.funcs & 0x02)? 0x80: 0);
				writeSXbus(busnumber, addr, data);
				__selectrix->bus_data[addr]= data;
				setGL(busnumber, addr, gltmp);
				DBG(busnumber, DBG_INFO, "Selectrix on bus %d, engine with address %d has data %X.", busnumber, addr, data);
			}
			else
			{
				DBG(busnumber, DBG_INFO, "Selectrix on bus %d, invalid address %d with engine", busnumber, addr);
			}
		}
		busses[busnumber].watchdog= 7;
		/* Antriebe (Weichen und Signale) */
		if (!queue_GA_isempty(busnumber))
		{
			busses[busnumber].watchdog= 8;
			unqueueNextGA(busnumber, &gatmp);
			addr= gatmp.id;
			if (__selectrix->number_adres> addr)
			{
				data= __selectrix->bus_data[addr];
				DBG(busnumber, DBG_INFO, "Selectrix on bus %d, address %d has old data %X.", busnumber, addr, data);
				/* Select the action to do */
				if (gatmp.action== 0)
				{
					/* Set pin to "0" */
					switch (gatmp.port)
					{
					case 1:
						data&= 0xfe;
						break;
					case 2:
						data&= 0xfd;
						break;
					case 3:
						data&= 0xfb;
						break;
					case 4:
						data&= 0xf7;
						break;
					case 5:
						data&= 0xef;
						break;
					case 6:
						data&= 0xdf;
						break;
					case 7:
						data&= 0xbf;
						break;
					case 8:
						data&= 0x7f;
						break;
					default:
						DBG(busnumber, DBG_INFO,
                                                        "Selectrix on bus %d, invalid portnumber %d with switch/signal or ...", busnumber, gatmp.port);
						break;
					}
				}
				else
				{
					/* Set pin to "1" */
					switch (gatmp.port)
					{
					case 1:
						data|= 0x01;
						break;
					case 2:
						data|= 0x02;
						break;
					case 3:
						data|= 0x04;
						break;
					case 4:
						data|= 0x08;
						break;
					case 5:
						data|= 0x10;
						break;
					case 6:
						data|= 0x20;
						break;
					case 7:
						data|= 0x40;
						break;
					case 8:
						data|= 0x80;
						break;
					default:
						DBG(busnumber, DBG_INFO,
                                                        "Selectrix on bus %d, invalid portnumber %d with switch/signal or ...", busnumber, gatmp.port);
						break;
					}
				}
				writeSXbus(busnumber, addr, data);
				__selectrix->bus_data[addr]= data;
				DBG(busnumber, DBG_INFO, "Selectrix on bus %d, address %d has new data %X.", busnumber, addr, data);
			}
			else
			{
				DBG(busnumber, DBG_INFO, "Selectrix on bus %d, invalid address %d with switch/signal or ...", busnumber, addr);
			}
		}
		busses[busnumber].watchdog= 9;
		/* Feed back contacts */
		if (__selectrix->number_fb> 0) 
		{
			busses[busnumber].watchdog= 10;
			/* Fetch the modul adres */
			addr= __selectrix->fb_adresses[actFB];
			//DBG(busnumber, DBG_INFO, "Selectrix on bus %d, feedbackadres %d.", busnumber, addr); 
			if (__selectrix->number_adres> addr)
			{
				/* Read the SXbus */
				data= readSXbus(busnumber, addr);
				__selectrix->bus_data[addr]= data;
				/* Set the deamon global data */
				setFBmodul(busnumber, actFB, data); /* Use 1, 2, ... as adres for feedback */
				//setFBmodul(busnumber, addr, data); /* Use real adres for feedback */
				DBG(busnumber, DBG_INFO, "Selectrix on bus %d, address %d has feedback data %X.", busnumber, addr, data);
			}
			else
			{
				DBG(busnumber, DBG_INFO, "Selectrix on bus %d, invalid address %d with feedback.", busnumber, addr);
			}
			// Select the next module
			if (actFB>= __selectrix->number_fb)
			{
				actFB= 1;   // Reset to start
			}
			else
			{
				actFB++;    // Next
			}
		}
	}
}
