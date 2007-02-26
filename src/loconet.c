/* $Id$ */

/*
 * loconet: loconet/srcp gateway
 */

#include "stdincludes.h"

#include "config-srcpd.h"
#include "io.h"
#include "loconet.h"
#include "srcp-fb.h"
#include "srcp-ga.h"
#include "srcp-gl.h"
#include "srcp-sm.h"
#include "srcp-power.h"
#include "srcp-srv.h"
#include "srcp-info.h"
#include "srcp-session.h"
#include "srcp-error.h"

#ifdef HAVE_LINUX_SERIAL_H
#include "linux/serial.h"
#else
#warning "MS100 support for linux only!"
#endif

#define __loconet ((LOCONET_DATA*)busses[busnumber].driverdata)

static int init_gl_LOCONET(struct _GLSTATE *);
static int init_ga_LOCONET(struct _GASTATE *);

int readConfig_LOCONET(xmlDocPtr doc, xmlNodePtr node, bus_t busnumber)
{
    xmlNodePtr child = node->children;
    xmlChar *txt;
    busses[busnumber].type = SERVER_LOCONET;
    busses[busnumber].init_func = &init_bus_LOCONET;
    busses[busnumber].term_func = &term_bus_LOCONET;
    busses[busnumber].thr_func = &thr_sendrec_LOCONET;
    busses[busnumber].init_gl_func = &init_gl_LOCONET;
    busses[busnumber].init_ga_func = &init_ga_LOCONET;
    busses[busnumber].driverdata = malloc(sizeof(struct _LOCONET_DATA));
    /*TODO: what happens if malloc returns NULL?*/

    __loconet->number_fb = 2048;        /* max addr for OPC_INPUT_REP (10+1 bit) */
    __loconet->number_ga = 2048;        /* max addr for OPC_SW_REQ */
    __loconet->loconetID = 0x50;        /* Loconet ID */
    __loconet->flags = LN_FLAG_ECHO;

    busses[busnumber].baudrate = B57600;

    strcpy(busses[busnumber].description, "GA FB POWER");

    while (child != NULL) {
        if (xmlStrncmp(child->name, BAD_CAST "text", 4) == 0) {
            /* just do nothing, it is only a comment */
        }
        
        else if (xmlStrcmp(child->name, BAD_CAST "loconetID") == 0) {
            txt = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
            if (txt != NULL) {
                __loconet->loconetID = (unsigned char) atoi((char *) txt);
                xmlFree(txt);
            }
        }
        
        else if (xmlStrcmp(child->name, BAD_CAST "ms100") == 0) {
#ifdef HAVE_LINUX_SERIAL_H
            txt = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);
            if (txt != NULL) {
                if (xmlStrcmp(txt, BAD_CAST "yes") == 0) {
                    __loconet->flags |= LN_FLAG_MS100;
		    __loconet->flags &= ~LN_FLAG_ECHO;
		}
                xmlFree(txt);
            }
#endif
        }

        else
            DBG(busnumber, DBG_WARN,
                    "WARNING, unknown tag found: \"%s\"!\n",
                    child->name);;
        
        child = child->next;
    }                           // while

    if (init_FB(busnumber, __loconet->number_fb)) {
        DBG(busnumber, DBG_ERROR, "Can't create array for feedback");
    }

    if (init_GA(busnumber, __loconet->number_ga)) {
        DBG(busnumber, DBG_ERROR, "Can't create array for GA");
    }

    return (1);
}

static int init_lineLOCONET(bus_t busnumber)
{
    int fd;
    struct termios interface;

    fd = open(busses[busnumber].filename.path, O_RDWR | O_NDELAY | O_NOCTTY);
    if (fd == -1) {
        DBG(busnumber, DBG_ERROR, "Sorry, couldn't open device.\n");
        return 1;
    }
    busses[busnumber].fd = fd;
#ifdef HAVE_LINUX_SERIAL_H
    if( (__loconet->flags & LN_FLAG_MS100) == 1 ) {
	  struct serial_struct serial;
	  struct termios tios;
	  int rc;
	  unsigned int cm;

	ioctl(fd, TIOCGSERIAL, &serial);
	serial.custom_divisor=7;
	serial.flags &= ~ASYNC_USR_MASK;
	serial.flags |= ASYNC_SPD_CUST | ASYNC_LOW_LATENCY;
	rc=ioctl(fd, TIOCSSERIAL, &serial);
	tcgetattr(fd, &tios);
        tios.c_iflag = IGNBRK | IGNPAR;
	tios.c_oflag = 0;
        tios.c_cflag = CS8 | CREAD | CLOCAL;
	tios.c_lflag = 0;
        cfsetospeed(&tios, busses[busnumber].baudrate);
	tcsetattr(fd, TCSANOW, &tios);

        tcflow(fd, TCOON);
	tcflow(fd, TCION);

        ioctl(fd, TIOCMGET, &cm);
	cm &= ~TIOCM_DTR;
        cm |= TIOCM_RTS | TIOCM_CTS;
	ioctl(fd, TIOCMSET, &cm);

        tcflush(fd, TCOFLUSH);
        tcflush(fd, TCIFLUSH);
    } else {
#endif
        tcgetattr(fd, &interface);
	interface.c_oflag = ONOCR;
        interface.c_cflag = CS8 | CRTSCTS | CSTOPB | CLOCAL | CREAD | HUPCL;
        interface.c_iflag = IGNBRK;
        interface.c_lflag = IEXTEN;
        interface.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
        cfsetispeed(&interface, busses[busnumber].baudrate);
        cfsetospeed(&interface, busses[busnumber].baudrate);
        interface.c_cc[VMIN] = 0;
        interface.c_cc[VTIME] = 0;

        tcsetattr(fd, TCSANOW, &interface);
#ifdef HAVE_LINUX_SERIAL_H
    }
#endif
    return 1;
}

int term_bus_LOCONET(bus_t busnumber)
{
    DBG(busnumber, DBG_INFO, "loconet bus %d terminating", busnumber);
    DBG(busnumber, DBG_INFO,
        "loconet bus %d: %u packets sent, %u packets received", busnumber,
        __loconet->sent_packets, __loconet->recv_packets);
    return 0;
}

/**
 * initGL: modifies the gl data used to initialize the device

 */
static int init_gl_LOCONET(struct _GLSTATE *gl)
{
    return SRCP_OK;
}

/**
 * initGA: modifies the ga data used to initialize the device

 */
static int init_ga_LOCONET(struct _GASTATE *ga)
{
    return SRCP_OK;
}

int init_bus_LOCONET(bus_t busnumber)
{
    __loconet->sent_packets = __loconet->recv_packets = 0;
    DBG(busnumber, DBG_INFO, "loconet init: bus #%d, debug %d", busnumber,
        busses[busnumber].debuglevel);
    if (busses[busnumber].debuglevel <= 5) {
        DBG(busnumber, DBG_INFO, "loconet bus %d open device %s",
            busnumber, busses[busnumber].filename.path);
        init_lineLOCONET(busnumber);
    }
    else {
        busses[busnumber].fd = -1;
    }
    DBG(busnumber, DBG_INFO, "loconet bus %d init done", busnumber);
    return 0;
}

static unsigned char ln_checksum(const unsigned char *cmd, int len)
{
    unsigned char chksum = 0xff;
    int i;
    for (i = 0; i < len; i++) {
        chksum ^= cmd[i];
    }
    return chksum;
}

static int ln_isecho(bus_t busnumber, const unsigned char *ln_packet,
                     unsigned char ln_packetlen)
{
    int i;
    /* do we check for echos? */
    if( (__loconet->flags & LN_FLAG_ECHO) == 0) {
	return FALSE;
    }

    if (__loconet->ln_msglen == 0)
        return FALSE;
    for (i = 0; i < ln_packetlen; i++) {
        if (ln_packet[i] != __loconet->ln_message[i])
            return FALSE;
    }

    return TRUE;
}

static int ln_read(bus_t busnumber, unsigned char *cmd, int len)
{
    int fd = busses[busnumber].fd;
    int index = 1;
    fd_set fds;
    struct timeval t = { 0, 0 };
    int retval;
    FD_ZERO(&fds);
    FD_SET(fd, &fds);
    retval = select(fd + 1, &fds, NULL, NULL, &t);
    if (retval > 0 && FD_ISSET(fd, &fds)) {
        /* read data from locobuffer */
        int pktlen;
        unsigned char c;
        /* read exactly one loconet packet; there must be at least one
           character due to select call result */
        /* a valid loconet packet starts with a byte >= 0x080
           and contains one or more bytes <0x80.
         */
        do {
            read(fd, &c, 1);
        } while (c < 0x80);
        switch (c & 0xe0) {
        case 0x80:
            pktlen = 2;
            break;
        case 0xa0:
            pktlen = 4;
            break;
        case 0xc0:
            pktlen = 6;
            break;
        case 0xe0:
            read(fd, &pktlen, 1);
            cmd[1] = pktlen;
            index = 2;
            break;
        }
        cmd[0] = c;
        read(fd, &cmd[index], pktlen - 1);
        retval = pktlen;
        if (ln_isecho(busnumber, cmd, pktlen)) {
            __loconet->ln_msglen = 0;
            DBG(busnumber, DBG_DEBUG, "this is the echo of the last "
                    "packet sent, clear to sent next command!");
            retval = 0;         /* we ignore echos */
        }
        else {
            __loconet->recv_packets++;
        }
    }

    return retval;
}


static int ln_write(bus_t busnumber, const unsigned char *cmd,
                    unsigned char len)
{
    unsigned char i;
    for (i = 0; i < len; i++) {
        writeByte(busnumber, cmd[i], 0);
    }
    DBG(busnumber, DBG_DEBUG,
        "sent loconet packet with OPC 0x%02x, %d bytes", cmd[0], len);
    __loconet->sent_packets++;
    __loconet->ln_msglen = len;
    memcpy(__loconet->ln_message, cmd, len);
    return 0;
}


static int ln_opc_peer_xfer_read(bus_t busnumber,
                                 const unsigned char *ln_packet)
{
    DBG(busnumber, DBG_DEBUG,
        "0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x",
        ln_packet[0], ln_packet[1], ln_packet[2], ln_packet[3],
        ln_packet[4], ln_packet[5], ln_packet[6], ln_packet[7],
        ln_packet[8], ln_packet[9], ln_packet[10], ln_packet[11],
        ln_packet[12], ln_packet[13], ln_packet[14], ln_packet[15]);

/*  srcaddr = (lnpacket[4] & 0x01 << 7) | lnpacket[5];
    srcaddr = srcaddr | ( ((lnpacket[4] & 0x02 << 6) | lnpacket[6]) << 8) ;
    if (lnpacket[1]==0x41 && lnpacket[SRC]==__loconet->lnpacket[DST]) {
        DBG(bus, DBG_INFO,
            "SM GET ANSWER: error %d, cv %d, val %d", error, cv, val);
*/
    session_endwait(busnumber,
                    ln_packet[12] | ((ln_packet[10] & 0x02 >> 1) << 7));
    return 1;
}

void *thr_sendrec_LOCONET(void *v)
{
    bus_t busnumber = (bus_t) v;
    unsigned char ln_packet[128];       /* max length is coded with 7 bit */
    unsigned char ln_packetlen = 2;
    unsigned int addr, timeoutcnt;
    int value;
    char msg[110];
    DBG(busnumber, DBG_INFO, "loconet started, bus #%d, %s", busnumber,
        busses[busnumber].filename.path);
    timeoutcnt = 0;
    while (1) {
        busses[busnumber].watchdog = 1;
        memset(ln_packet, 0, sizeof(ln_packet));
        /* first is always to read _from_ loconet */
        if (((ln_packetlen =
              ln_read(busnumber, ln_packet, sizeof(ln_packet))) > 0)) {

            switch (ln_packet[0]) {
            case OPC_GPOFF:
                busses[busnumber].power_state = 0;
                strcpy(busses[busnumber].power_msg, "from loconet");
                infoPower(busnumber, msg);
                queueInfoMessage(msg);
                break;
            case OPC_GPON:
                busses[busnumber].power_state = 1;
                strcpy(busses[busnumber].power_msg, "from loconet");
                infoPower(busnumber, msg);
                queueInfoMessage(msg);
                break;
	    case OPC_SW_REP:
		break;
            case OPC_INPUT_REP:
                addr =
                    ((unsigned int) ln_packet[1] & 0x007f) |
                    (((unsigned int) ln_packet[2] & 0x000f) << 7);
                addr = 1 + addr * 2 +
                    ((((unsigned int) ln_packet[2] & 0x0020) >> 5));
                value = (ln_packet[2] & 0x10) >> 4;
                updateFB(busnumber, addr, value);
                break;
            case OPC_PEER_XFER:
                /* this one is difficult */
                ln_opc_peer_xfer_read(busnumber, ln_packet);
                break;
            default:
                /* unkown loconet packet received, ignored */
                break;
            }
        }
        if (__loconet->ln_msglen == 0) {
            /* now we process the way back _to_ loconet */
            ln_packet[0] = OPC_IDLE;
            ln_packetlen = 2;
            if (busses[busnumber].power_changed == 1) {
                ln_packet[0] = 0x82 + busses[busnumber].power_state;
                ln_packetlen = 2;
                busses[busnumber].power_changed = 0;
                infoPower(busnumber, msg);
                queueInfoMessage(msg);
            }
            else if (!queue_GA_isempty(busnumber)) {
                struct _GASTATE gatmp;
                unqueueNextGA(busnumber, &gatmp);
                addr = gatmp.id;
		ln_packetlen = 4;
		ln_packet[0] = OPC_SW_REQ;
		ln_packet[1] = (unsigned short int) addr & 0x0007f;
		ln_packet[2] = (unsigned short int) ( addr >> 7) & 0x000f;
		ln_packet[2] |= (unsigned short int) gatmp.port;
		ln_packet[2] |= ((unsigned short int) gatmp.action) << 4;
		
		if(gatmp.action == 1) {
        	    gettimeofday(&gatmp.tv[gatmp.port], NULL);
        	}
        	setGA(busnumber, addr, gatmp);
                DBG(busnumber, DBG_DEBUG, "loconet: GA SET #%d %02X",
                        gatmp.id, gatmp.action);
            }

            else if (!queue_SM_isempty(busnumber)) {
                struct _SM smtmp;
                session_processwait(busnumber);
                unqueueNextSM(busnumber, &smtmp);
                addr = smtmp.addr;
                switch (smtmp.command) {
                case SET:
                    DBG(busnumber, DBG_DEBUG, "loconet: SM SET #%d %02X",
                        smtmp.addr, smtmp.value);
                    break;
                case GET:
                    DBG(busnumber, DBG_DEBUG, "loconet SM GET #%d[%d]",
                        smtmp.addr, smtmp.typeaddr);
                    ln_packetlen = 16;
                    ln_packet[0] = 0xe5;        /* OPC_PEER_XFER, old fashioned */
                    ln_packet[1] = ln_packetlen;
                    ln_packet[2] = __loconet->loconetID;        /* sender ID */
                    ln_packet[3] = (unsigned char) smtmp.addr;  /* dest addr */
                    ln_packet[4] = 0x01;
                    ln_packet[5] = 0x10;
                    ln_packet[6] = 0x02;
                    ln_packet[7] = (unsigned char) smtmp.typeaddr;
                    ln_packet[8] = 0x00;
                    ln_packet[9] = 0x00;
                    ln_packet[10] = 0x00;
                    break;
                }
            }
            ln_packet[ln_packetlen - 1] =
                ln_checksum(ln_packet, ln_packetlen - 1);
            if (ln_packet[0] != OPC_IDLE) {
                ln_write(busnumber, ln_packet, ln_packetlen);
                timeoutcnt = 0;
            }
        }
        else {
            DBG(busnumber, DBG_DEBUG,
                "Still waiting for echo of last command (%d)", timeoutcnt);
            usleep(100000);
            timeoutcnt++;
            if (timeoutcnt > 10) {
                DBG(busnumber, DBG_DEBUG,
                    "time out for reading echo, giving up");
                __loconet->ln_msglen = 0;
            }
        }
        usleep(1000);
    }
}


