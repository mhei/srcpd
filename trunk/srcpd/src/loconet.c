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

static long int init_gl_LOCONET(struct _GLSTATE *);
static long int init_ga_LOCONET(struct _GASTATE *);
static void DisplayMessage(long int busnumber,
                           const unsigned char *msgBuf, int msgSize);


int readConfig_LOCONET(xmlDocPtr doc, xmlNodePtr node, long int busnumber)
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

static long int init_lineLOCONET(long int busnumber)
{
    int fd;
    struct termios interface;

    fd = open(busses[busnumber].device, O_RDWR | O_NDELAY | O_NOCTTY);
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
        cfsetospeed(&tios, B38400);
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

long int term_bus_LOCONET(long int busnumber)
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
static long int init_gl_LOCONET(struct _GLSTATE *gl)
{
    return SRCP_OK;
}

/**
 * initGA: modifies the ga data used to initialize the device

 */
static long int init_ga_LOCONET(struct _GASTATE *ga)
{
    return SRCP_OK;
}

long int init_bus_LOCONET(long int busnumber)
{
    __loconet->sent_packets = __loconet->recv_packets = 0;
    DBG(busnumber, DBG_INFO, "loconet init: bus #%d, debug %d", busnumber,
        busses[busnumber].debuglevel);
    if (busses[busnumber].debuglevel <= 5) {
        DBG(busnumber, DBG_INFO, "loconet bus %d open device %s",
            busnumber, busses[busnumber].device);
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

static int ln_isecho(long int busnumber, const unsigned char *ln_packet,
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

static int ln_read(long int busnumber, unsigned char *cmd, int len)
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
        DisplayMessage(busnumber, cmd, pktlen);
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


static int ln_write(long int busnumber, const unsigned char *cmd,
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


static int ln_opc_peer_xfer_read(long int busnumber,
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
    long int busnumber = (long int) v;
    unsigned char ln_packet[128];       /* max length is coded with 7 bit */
    unsigned char ln_packetlen = 2;
    unsigned int addr, timeoutcnt;
    int value;
    char msg[110];
    DBG(busnumber, DBG_INFO, "loconet started, bus #%d, %s", busnumber,
        busses[busnumber].device);
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
            case OPC_INPUT_REP:
                addr =
                    ((unsigned int) ln_packet[1] & 0x007f) |
                    (((unsigned int) ln_packet[2] & 0x000f) << 7);
                addr = 1 + addr * 2 +
                    ((((unsigned int) ln_packet[2] & 0x0020) >> 5));
                value = (ln_packet[2] & 0x10) >> 4;
                updateFB(busnumber, addr, value);
                break;
            case 0xe5:         /* OPC_PEER_XFER */
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
		ln_packet[0] = OPC_INPUT_REP;
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

/****************************************************************************
*
*  ConvertToMixed
*
*  REMARKS
*
*     This function creates a string representation of the loco address in
*     addressLow & addressHigh in a form appropriate for the type of address
*     (2 or 4 digit) using the Digitrax 'mixed mode' if necessary.
*
*  RETURN VALUE
*
*     Nothing.
*/

static void ConvertToMixed(byte addressLow,
                           byte addressHigh, char *locoAdrStr)
{
    /* put the full 4 digit address in the string */
    sprintf(locoAdrStr, "%5d", LOCO_ADR(addressHigh, addressLow));

    /* if we have a 2 digit decoder address and proceed accordingly */
    if (addressHigh == 0) {
        if (addressLow >= 120) {
            sprintf(locoAdrStr, "c%d (%d)", addressLow - 120, addressLow);
        }
        else if (addressLow >= 110) {
            sprintf(locoAdrStr, "b%d (%d)", addressLow - 110, addressLow);
        }
        else if (addressLow >= 100) {
            sprintf(locoAdrStr, "a%d (%d)", addressLow - 100, addressLow);
        }
    }
}


/****************************************************************************
*
*  DisplayMessage
*
*  REMARKS
*
*     Displays a (valid) message in human readable form and, possibly,
*     hex and/or decimal. Unknown messages, or those whose definition is
*     not available in the public domain, are printed in hex with an
*     appropriate notice.
*
*  RETURN VALUE
*
*     Nothing.
*/

static void DisplayMessage(long int busnumber,
                           const unsigned char *msgBuf, int msgSize)
{
    int forceHex = FALSE;       /* force hex display line             */
    int showStatus = FALSE;     /* show track status in this message? */
    char mixedAdrStr[32];       /* (possible) mixed mode loco address */
    int hours, minutes;
    char logString[1024];

    int showTrackStatus = TRUE; /* if TRUE, show track status on every slot read    */
    int trackStatus = -1;       /* most recent track status value                   */

    /* message structure pointers for ease of coding/debugging */
    locoAdrMsg *locoAdr;
    switchAckMsg *switchMsg;
    slotReqMsg *slotReq;
    slotMoveMsg *slotMove;
    slotLinkMsg *slotLink;
    consistFuncMsg *consistFunc;
    slotStatusMsg *slotStatus;
    longAckMsg *longAck;
    inputRepMsg *inputReport;
    swRepMsg *swReport;
    swReqMsg *swRequest;
    locoSndMsg *locoSnd;
    locoDirfMsg *locoDirf;
    locoSpdMsg *locoSpd;
    rwSlotDataMsg *rwSlotData;
    fastClockMsg *fastClock;
    peerXferMsg *peerXfer;
    sendPktMsg *sendPkt;

    switch (msgBuf[0]) {
/***************************
* ; 2 Byte MESSAGE OPCODES *
* ; FORMAT = <OPC>,<CKSUM> *
* ;                        *
***************************/

/*************************************************
* OPC_BUSY         0x81   ;MASTER busy code, NUL *
*************************************************/
    case OPC_GPBUSY:           /* page 8 of Loconet PE */
        sprintf(logString, "Master is busy\n");
        break;

/****************************************************
* OPC_GPOFF        0x82   ;GLOBAL power OFF request *
****************************************************/
    case OPC_GPOFF:            /* page 8 of Loconet PE */
        sprintf(logString, "Global Power OFF\n");
        break;

/***************************************************
* OPC_GPON         0x83   ;GLOBAL power ON request *
***************************************************/
    case OPC_GPON:             /* page 8 of Loconet PE */
        sprintf(logString, "Global Power ON\n");
        break;

/**********************************************************************
* OPC_IDLE         0x85   ;FORCE IDLE state, Broadcast emergency STOP *
**********************************************************************/
    case OPC_IDLE:             /* page 8 of Loconet PE */
        sprintf(logString, "Force Idle, Emergency STOP\n");
        break;

/*****************************************
* ; 4 byte MESSAGE OPCODES               *
* ; FORMAT = <OPC>,<ARG1>,<ARG2>,<CKSUM> *
* :                                      *
*  CODES 0xA8 to 0xAF have responses     *
*  CODES 0xB8 to 0xBF have responses     *
*****************************************/

/***************************************************************************
* OPC_LOCO_ADR     0xBF   ; REQ loco ADR                                   *
*                         ; Follow on message: <E7>SLOT READ               *
*                         ; <0xBF>,<0>,<ADR>,<CHK> REQ loco ADR            *
*                         ; DATA return <E7>, is SLOT#, DATA that ADR was  *
*                         : found in.                                      *
*                         ; IF ADR not found, MASTER puts ADR in FREE slot *
*                         ; and sends DATA/STATUS return <E7>......        *
*                         ; IF no FREE slot, Fail LACK,0 is returned       *
*                         ; [<B4>,<3F>,<0>,<CHK>]                          *
***************************************************************************/
    case OPC_LOCO_ADR:         /* page 8 of Loconet PE */
        locoAdr = (locoAdrMsg *) msgBuf;

        ConvertToMixed(locoAdr->adr_lo, locoAdr->adr_hi, mixedAdrStr);

        sprintf(logString,
                "Request slot information for loco address %s\n",
                mixedAdrStr);
        break;

/*****************************************************************************
* OPC_SW_ACK       0xBD   ; REQ SWITCH WITH acknowledge function (not DT200) *
*                         ; Follow on message: LACK                          *
*                         ; <0xBD>,<SW1>,<SW2>,<CHK> REQ SWITCH function     *
*                         ;       <SW1> =<0,A6,A5,A4- A3,A2,A1,A0>           *
*                         ;               7 ls adr bits.                     *
*                         ;               A1,A0 select 1 of 4 input pairs    *
*                         ;               in a DS54                          *
*                         ;       <SW2> =<0,0,DIR,ON- A10,A9,A8,A7>          *
*                         ;               Control bits and 4 MS adr bits.    *
*                         ;               DIR=1 for Closed/GREEN             *
*                         ;                  =0 for Thrown/RED               *
*                         ;               ON=1 for Output ON                 *
*                         ;                 =0 FOR output OFF                *
*                         ; response is:                                     *
*                         ; <0xB4><3D><00> if DCS100 FIFO is full, rejected. *
*                         ; <0xB4><3D><7F> if DCS100 accepted                *
*****************************************************************************/
    case OPC_SW_ACK:           /* page 8 of Loconet PE */
        switchMsg = (switchAckMsg *) msgBuf;
        sprintf(logString,
                "Request switch %d %s (Output %s) with Acknowledge\n",
                SENSOR_ADR(switchMsg->sw1, switchMsg->sw2),
                (switchMsg->
                 sw2 & OPC_SW_ACK_CLOSED) ? "Closed/Green" : "Thrown/Red",
                (switchMsg->sw2 & OPC_SW_ACK_OUTPUT) ? "On" : "Off");
        break;

/*************************************************************************
* OPC_SW_STATE     0xBC   ; REQ state of SWITCH                          *
*                         ; Follow on message: LACK                      *
*                         ; <0xBC>,<SW1>,<SW2>,<CHK> REQ state of SWITCH *
*************************************************************************/
    case OPC_SW_STATE:         /* page 8 of Loconet PE */
        switchMsg = (switchReqMsg *) msgBuf;
        sprintf(logString,
                "Request state of switch %d\n",
                SENSOR_ADR(switchMsg->sw1, switchMsg->sw2));
        break;

/************************************************************************************
* OPC_RQ_SL_DATA   0xBB   ; Request SLOT DATA/status block                          *
*                         ; Follow on message: <E7>SLOT READ                        *
*                         ; <0xBB>,<SLOT>,<0>,<CHK> Request SLOT DATA/status block. *
************************************************************************************/
    case OPC_RQ_SL_DATA:       /* page 8 of Loconet PE */
        slotReq = (slotReqMsg *) msgBuf;
        sprintf(logString,
                "Request data/status for slot %3d\n", slotReq->slot);
        break;

/*******************************************************************************
* OPC_MOVE_SLOTS   0xBA   ; MOVE slot SRC to DEST                              *
*                         ; Follow on message: <E7>SLOT READ                   *
*                         ; <0xBA>,<SRC>,<DEST>,<CHK> Move SRC to DEST if      *
*                         ; SRC or LACK etc is NOT IN_USE, clr SRC             *
*                         ; SPECIAL CASES:                                     *
*                         ; If SRC=0 ( DISPATCH GET) , DEST=dont care,         *
*                         ;    Return SLOT READ DATA of DISPATCH Slot          *
*                         ; IF SRC=DEST (NULL move) then SRC=DEST is set to    *
*                         ;    IN_USE , if legal move.                         *
*                         ; If DEST=0, is DISPATCH Put, mark SLOT as DISPATCH  *
*                         ;    RETURN slot status <0xE7> of DESTINATION slot   *
*                         ;       DEST if move legal                           *
*                         ;    RETURN Fail LACK code if illegal move           *
*                         ;       <B4>,<3A>,<0>,<chk>, illegal to move to/from *
*                         ;       slots 120/127                                *
*******************************************************************************/
    case OPC_MOVE_SLOTS:       /* page 8 of Loconet PE */
        slotMove = (slotMoveMsg *) msgBuf;

        /* check special cases */
        if (slotMove->src == 0) {       /* DISPATCH GET */
            sprintf(logString, "Get most recently dispatched slot\n");
        }
        else if (slotMove->src == slotMove->dest) {     /* IN USE       */
            sprintf(logString,
                    "Set status of slot %d to IN_USE\n", slotMove->src);
        }
        else if (slotMove->dest == 0) { /* DISPATCH PUT */
            sprintf(logString,
                    "Mark slot %3d as DISPATCHED\n", slotMove->src);
        }
        else {                  /* general move */

            sprintf(logString,
                    "Move data in slot %3d to slot %3d\n",
                    slotMove->src, slotMove->dest);
        }

        break;

/********************************************************************************
* OPC_LINK_SLOTS   0xB9   ; LINK slot ARG1 to slot ARG2                         *
*                         ; Follow on message: <E7>SLOT READ                    *
*                         ; <0xB9>,<SL1>,<SL2>,<CHK> SLAVE slot SL1 to slot SL2 *
*                         ; Master LINKER sets the SL_CONUP/DN flags            *
*                         ; appropriately. Reply is return of SLOT Status       *
*                         ; <0xE7>. Inspect to see result of Link, invalid      *
*                         ; Link will return Long Ack Fail <B4>,<39>,<0>,<CHK>  *
********************************************************************************/
    case OPC_LINK_SLOTS:       /* page 9 of Loconet PE */
        slotLink = (slotLinkMsg *) msgBuf;
        sprintf(logString,
                "Consist loco in slot %3d to loco in slot %3d\n",
                slotLink->src, slotLink->dest);
        break;

/*******************************************************************************************
* OPC_UNLINK_SLOTS 0xB8   ;UNLINK slot ARG1 from slot ARG2                                 *
*                         ; Follow on message: <E7>SLOT READ                               *
*                         ; <0xB8>,<SL1>,<SL2>,<CHK> UNLINK slot SL1 from SL2              *
*                         ; UNLINKER executes unlink STRATEGY and returns new SLOT#        *
*                         ; DATA/STATUS of unlinked LOCO . Inspect data to evaluate UNLINK *
*******************************************************************************************/
    case OPC_UNLINK_SLOTS:     /* page 9 of Loconet PE */
        slotLink = (slotLinkMsg *) msgBuf;
        sprintf(logString,
                "Remove loco in slot %3d from consist with loco in slot %3d\n",
                slotLink->src, slotLink->dest);
        break;

/*************************************************************************************
* OPC_CONSIST_FUNC 0xB6   ; SET FUNC bits in a CONSIST uplink element                *
*                         ; <0xB6>,<SLOT>,<DIRF>,<CHK> UP consist FUNC bits          *
*                         ; NOTE this SLOT adr is considered in UPLINKED slot space. *
*************************************************************************************/
    case OPC_CONSIST_FUNC:     /* page 9 of Loconet PE */
        consistFunc = (consistFuncMsg *) msgBuf;
        sprintf(logString,
                "Set consist in slot %3d direction to %s, F0=%s F1=%s F2=%s F3=%s F4=%s\n",
                consistFunc->slot,
                (consistFunc->dirf & DIRF_DIR) ? "REV" : "FWD",
                (consistFunc->dirf & DIRF_F0) ? "On, " : "Off,",
                (consistFunc->dirf & DIRF_F1) ? "On, " : "Off,",
                (consistFunc->dirf & DIRF_F2) ? "On, " : "Off,",
                (consistFunc->dirf & DIRF_F3) ? "On, " : "Off,",
                (consistFunc->dirf & DIRF_F4) ? "On" : "Off");
        break;

/********************************************************************
* OPC_SLOT_STAT1   0xB5   ; WRITE slot stat1                        *
*                         ; <0xB5>,<SLOT>,<STAT1>,<CHK> WRITE stat1 *
********************************************************************/
    case OPC_SLOT_STAT1:       /* page 9 of Loconet PE */
        slotStatus = (slotStatusMsg *) msgBuf;
        sprintf(logString,
                "Write slot %3d with status value %3d (0x%02x) - Loco is %s, %s,\n"
                "\tand operating in %s speed step mode\n",
                slotStatus->slot,
                slotStatus->stat,
                slotStatus->stat,
                CONSIST_STAT(slotStatus->stat),
                LOCO_STAT(slotStatus->stat), DEC_MODE(slotStatus->stat));
        break;

/*******************************************************************************
* OPC_LONG_ACK     0xB4   ; Long acknowledge                                   *
*                         ; <0xB4>,<LOPC>,<ACK1>,<CHK> Long acknowledge        *
*                         ; <LOPC> is COPY of OPCODE responding to (msb=0).    *
*                         ; LOPC=0 (unused OPC) is also VALID fail code        *
*                         ; <ACK1> is appropriate response code for the OPCode *
*******************************************************************************/
    case OPC_LONG_ACK:         /* page 9 of Loconet PE */
        longAck = (longAckMsg *) msgBuf;

        switch (longAck->opcode | 0x80) {
        case (OPC_LOCO_ADR):   /* response for OPC_LOCO_ADR */
            sprintf(logString, "No free slot\n");
            break;

        case (OPC_LINK_SLOTS): /* response for OPC_LINK_SLOTS */
            sprintf(logString, "Invalid consist\n");
            break;

        case (OPC_SW_ACK):     /* response for OPC_SW_ACK   */
            if (longAck->ack1 == 0) {
                sprintf(logString,
                        "The DCS-100 FIFO is full, the switch command was rejected\n");
            }
            else if (longAck->ack1 == 0x7f) {
                sprintf(logString,
                        "The DCS-100 accepted the switch command\n");
            }
            else {
                sprintf(logString,
                        "Unknown response to 'Request Switch with ACK' command, 0x%02x\n",
                        msgBuf[2]);

                forceHex = TRUE;
            }

            break;

        case (OPC_SW_REQ):     /* response for OPC_SW_REQ */
            sprintf(logString, "Switch request Failed!\n");
            break;

        case (OPC_WR_SL_DATA):
            if (longAck->ack1 == 0) {
                sprintf(logString,
                        "The Slot Write command was rejected\n");
            }
            else if (longAck->ack1 == 0x01) {
                sprintf(logString,
                        "The Slot Write command was accepted\n");
            }
            else if (longAck->ack1 == 0x40) {
                sprintf(logString,
                        "The Slot Write command was accepted blind (no response will be sent)\n");
            }
            else if (longAck->ack1 == 0x7f) {
                sprintf(logString,
                        "The Slot Write command was accepted\n");
            }
            else {
                sprintf(logString,
                        "Unknown response to Write Slot Data message 0x%02x\n",
                        longAck->ack1);

                forceHex = TRUE;
            }

            break;

        case (OPC_SW_STATE):
            if (longAck->ack1 == 0) {
                sprintf(logString, "The Switch command was rejected\n");
            }
            else if (longAck->ack1 == 0x7f) {
                sprintf(logString, "The Switch command was accepted\n");
            }
            else {
                sprintf(logString,
                        "Unknown reponse to Switch command message 0x%02x\n",
                        longAck->ack1);

                forceHex = TRUE;
            }

            break;

        case (OPC_MOVE_SLOTS):
            if (longAck->ack1 == 0) {
                sprintf(logString,
                        "The Move Slots command was rejected\n");
            }
            else if (longAck->ack1 == 0x7f) {
                sprintf(logString,
                        "The Move Slots command was accepted\n");
            }
            else {
                sprintf(logString,
                        "Unknown reponse to Move Slots message 0x%02x\n",
                        longAck->ack1);

                forceHex = TRUE;
            }

            break;

        case OPC_IMM_PACKET:   /* special response to OPC_IMM_PACKET */
            if (longAck->ack1 == 0) {
                sprintf(logString,
                        "The Send IMM Packet command was rejected, the buffer is full/busy\n");
            }
            else if (longAck->ack1 == 0x7f) {
                sprintf(logString,
                        "The Send IMM Packet command was accepted\n");
            }
            else {
                sprintf(logString,
                        "Unknown reponse to Send IMM Packet message 0x%02x\n",
                        longAck->ack1);

                forceHex = TRUE;
            }

            break;


        case OPC_IMM_PACKET_2: /* special response to OPC_IMM_PACKET */
            sprintf(logString,
                    "The Lim Master responded to the Send IMM Packet command with %d (0x%0x)\n",
                    longAck->ack1, longAck->ack1);
            break;


        default:
            sprintf(logString,
                    "Response %d (0x%02x) to opcode 0x%02x not available in plain english\n",
                    longAck->ack1, longAck->ack1, longAck->opcode);

            forceHex = TRUE;
            break;
        }

        break;

/********************************************************************************************
* OPC_INPUT_REP    0xB2   ; General SENSOR Input codes                                      *
*                         ; <0xB2>, <IN1>, <IN2>, <CHK>                                     *
*                         ;   <IN1> =<0,A6,A5,A4- A3,A2,A1,A0>,                             *
*                         ;           7 ls adr bits.                                        *
*                         ;           A1,A0 select 1 of 4 inputs pairs in a DS54.           *
*                         ;   <IN2> =<0,X,I,L- A10,A9,A8,A7>,                               *
*                         ;           Report/status bits and 4 MS adr bits.                 *
*                         ;           "I"=0 for DS54 "aux" inputs                           *
*                         ;              =1 for "switch" inputs mapped to 4K SENSOR space.  *
*                         ;                                                                 *
*                         ;           (This is effectively a least significant adr bit when *
*                         ;            using DS54 input configuration)                      *
*                         ;                                                                 *
*                         ;           "L"=0 for input SENSOR now 0V (LO),                   *
*                         ;              =1 for Input sensor >=+6V (HI)                     *
*                         ;           "X"=1, control bit,                                   *
*                         ;              =0 is RESERVED for future!                         *
********************************************************************************************/
    case OPC_INPUT_REP:        /* page 9 of Loconet PE */
        inputReport = (inputRepMsg *) msgBuf;

        sprintf(logString,
                "Sensor input report: %s at %d is %s %s\n",
                (inputReport->
                 in2 & OPC_INPUT_REP_SW) ? "Switch" : "DS54 Aux input",
                SENSOR_ADR(inputReport->in1, inputReport->in2),
                (inputReport->in2 & OPC_INPUT_REP_HI) ? "Hi" : "Lo",
                (inputReport->
                 in2 & OPC_INPUT_REP_CB) ? "(RESERVED CONTROL BIT SET!)" :
                "");

        break;

/***************************************************************************************
* OPC_SW_REP       0xB1   ; Turnout SENSOR state REPORT                                *
*                         ; <0xB1>,<SN1>,<SN2>,<CHK> SENSOR state REPORT               *
*                         ;   <SN1> =<0,A6,A5,A4- A3,A2,A1,A0>,                        *
*                         ;           7 ls adr bits.                                   *
*                         ;           A1,A0 select 1 of 4 input pairs in a DS54        *
*                         ;   <SN2> =<0,1,I,L- A10,A9,A8,A7>                           *
*                         ;           Report/status bits and 4 MS adr bits.            *
*                         ;           this <B1> opcode encodes input levels            *
*                         ;           for turnout feedback                             *
*                         ;           "I" =0 for "aux" inputs (normally not feedback), *
*                         ;               =1 for "switch" input used for               *
*                         ;                  turnout feedback for DS54                 *
*                         ;                  ouput/turnout # encoded by A0-A10         *
*                         ;           "L" =0 for this input 0V (LO),                   *
*                         ;               =1 this input > +6V (HI)                     *
*                         ;                                                            *
*                         ;   alternately;                                             *
*                         ;                                                            *
*                         ;   <SN2> =<0,0,C,T- A10,A9,A8,A7>                           *
*                         ;           Report/status bits and 4 MS adr bits.            *
*                         ;           this <B1> opcode encodes current OUTPUT levels   *
*                         ;           "C" =0 if "Closed" ouput line is OFF,            *
*                         ;               =1 "closed" output line is ON                *
*                         ;                  (sink current)                            *
*                         ;           "T" =0 if "Thrown" output line is OFF,           *
*                         ;               =1 "thrown" output line is ON                *
*                         ;                  (sink I)                                  *
***************************************************************************************/
    case OPC_SW_REP:           /* page 9 of Loconet PE */
        swReport = (swRepMsg *) msgBuf;

        if (swReport->sn2 & OPC_SW_REP_INPUTS) {
            sprintf(logString,
                    "Turnout Sensor Input report: %s at %d is %s \n",
                    (swReport->
                     sn2 & OPC_SW_REP_SW) ? "Switch" : "Aux input",
                    SENSOR_ADR(swReport->sn1, swReport->sn2),
                    (swReport->sn2 & OPC_SW_REP_HI) ? "Hi" : "Lo");
        }
        else {
            sprintf(logString,
                    "Turnout Sensor Output report for Address %d: "
                    "\t  Closed output is %s, Thrown output is %s\n",
                    SENSOR_ADR(swReport->sn1, swReport->sn2),
                    (swReport->sn2 & OPC_SW_REP_CLOSED) ? "ON" : "OFF",
                    (swReport->sn2 & OPC_SW_REP_THROWN) ? "ON" : "OFF");
        }

        break;

/*******************************************************************************************
* OPC_SW_REQ       0xB0   ; REQ SWITCH function                                            *
*                         ; <0xB0>,<SW1>,<SW2>,<CHK> REQ SWITCH function                   *
*                         ;   <SW1> =<0,A6,A5,A4- A3,A2,A1,A0>,                            *
*                         ;           7 ls adr bits.                                       *
*                         ;           A1,A0 select 1 of 4 input pairs in a DS54            *
*                         ;   <SW2> =<0,0,DIR,ON- A10,A9,A8,A7>                            *
*                         ;           Control bits and 4 MS adr bits.                      *
*                         ;   DIR  =1 for Closed,/GREEN,                                   *
*                         ;        =0 for Thrown/RED                                       *
*                         ;   ON   =1 for Output ON,                                       *
*                         ;        =0 FOR output OFF                                       *
*                         ;                                                                *
*                         ;   Note-Immediate response of <0xB4><30><00> if command failed, *
*                         ;        otherwise no response "A" CLASS codes                   *
*******************************************************************************************/
    case OPC_SW_REQ:           /* page 9 of Loconet PE */
        swRequest = (swReqMsg *) msgBuf;

        sprintf(logString,
                "Requesting Switch at %d to %s (output %s)\n",
                SENSOR_ADR(swRequest->sw1, swRequest->sw2),
                (swRequest->sw2 & OPC_SW_REQ_DIR) ? "Closed" : "Thrown",
                (swRequest->sw2 & OPC_SW_REQ_OUT) ? "On" : "Off");
        break;

/****************************************************
* OPC_LOCO_SND     0xA2   ;SET SLOT sound functions *
****************************************************/
    case OPC_LOCO_SND:         /* page 10 of Loconet PE */
        locoSnd = (locoSndMsg *) msgBuf;

        sprintf(logString,
                "Set loco in slot %3d Sound1/F5=%s, Sound2/F6=%s, Sound3/F7=%s, Sound4/F8=%s\n",
                locoSnd->slot,
                (locoSnd->snd & SND_F5) ? "On" : "Off",
                (locoSnd->snd & SND_F6) ? "On" : "Off",
                (locoSnd->snd & SND_F7) ? "On" : "Off",
                (locoSnd->snd & SND_F8) ? "On" : "Off");
        break;

/****************************************************
* OPC_LOCO_DIRF    0xA1   ;SET SLOT dir, F0-4 state *
****************************************************/
    case OPC_LOCO_DIRF:        /* page 10 of Loconet PE */
        locoDirf = (locoDirfMsg *) msgBuf;

        sprintf(logString,
                "Set loco in slot %3d direction to %s, F0=%s F1=%s F2=%s F3=%s F4=%s\n",
                locoDirf->slot,
                (locoDirf->dirf & DIRF_DIR) ? "REV" : "FWD",
                (locoDirf->dirf & DIRF_F0) ? "On, " : "Off,",
                (locoDirf->dirf & DIRF_F1) ? "On, " : "Off,",
                (locoDirf->dirf & DIRF_F2) ? "On, " : "Off,",
                (locoDirf->dirf & DIRF_F3) ? "On, " : "Off,",
                (locoDirf->dirf & DIRF_F4) ? "On" : "Off");
        break;

/***********************************************************************
* OPC_LOCO_SPD     0xA0   ;SET SLOT speed e.g. <0xA0><SLOT#><SPD><CHK> *
***********************************************************************/
    case OPC_LOCO_SPD:         /* page 10 of Loconet PE */
        locoSpd = (locoSpdMsg *) msgBuf;

        if (locoSpd->spd == OPC_LOCO_SPD_ESTOP) {       /* emergency stop */
            sprintf(logString,
                    "Set speed of loco in slot %3d to EMERGENCY STOP!\n",
                    locoSpd->slot);
        }
        else {
            sprintf(logString,
                    "Set speed of loco in slot %3d to %3d\n",
                    locoSpd->slot, locoSpd->spd);
        }

        break;

/********************************************************************
* ; VARIABLE Byte MESSAGE OPCODES                                   *
* ; FORMAT = <OPC>,<COUNT>,<ARG2>,<ARG3>,...,<ARG(COUNT-3)>,<CKSUM> *
********************************************************************/

/**********************************************************************************************
* OPC_WR_SL_DATA   0xEF   ; WRITE SLOT DATA, 10 bytes                                         *
*                         ; Follow on message: LACK                                           *
*                         ; <0xEF>,<0E>,<SLOT#>,<STAT>,<ADR>,<SPD>,<DIRF>,                    *
*                         ;        <TRK>,<SS2>,<ADR2>,<SND>,<ID1>,<ID2>,<CHK>                 *
*                         ; SLOT DATA WRITE, 10 bytes data /14 byte MSG                       *
***********************************************************************************************
* OPC_SL_RD_DATA   0xE7   ; SLOT DATA return, 10 bytes                                        *
*                         ; <0xE7>,<0E>,<SLOT#>,<STAT>,<ADR>,<SPD>,<DIRF>,                    *
*                         ;        <TRK>,<SS2>,<ADR2>,<SND>,<ID1>,<ID2>,<CHK>                 *
*                         ; SLOT DATA READ, 10 bytes data /14 byte MSG                        *
*                         ;                                                                   *
*                         ; NOTE; If STAT2.2=0 EX1/EX2 encodes an ID#,                        *
*                         ;       [if STAT2.2=1 the STAT.3=0 means EX1/EX2                    *
*                         ;        are ALIAS]                                                 *
*                         ;                                                                   *
*                         ; ID1/ID2 are two 7 bit values encoding a 14 bit                    *
*                         ;         unique DEVICE usage ID.                                   *
*                         ;                                                                   *
*                         ;   00/00 - means NO ID being used                                  *
*                         ;                                                                   *
*                         ;   01/00 - ID shows PC usage.                                      *
*                         ;    to         Lo nibble is TYP PC#                                *
*                         ;   7F/01       (PC can use hi values)                              *
*                         ;                                                                   *
*                         ;   00/02 -SYSTEM reserved                                          *
*                         ;    to                                                             *
*                         ;   7F/03                                                           *
*                         ;                                                                   *
*                         ;   00/04 -NORMAL throttle RANGE                                    *
*                         ;    to                                                             *
*                         ;   7F/7E                                                           *
***********************************************************************************************
* Notes:                                                                                      *
* The SLOT DATA bytes are, in order of TRANSMISSION for <E7> READ or <EF> WRITE.              *
* NOTE SLOT 0 <E7> read will return MASTER config information bytes.                          *
*                                                                                             *
* 0) SLOT NUMBER:                                                                             *
*                                                                                             *
* ; 0-7FH, 0 is special SLOT,                                                                 *
*                     ; 070H-07FH DIGITRAX reserved:                                          *
*                                                                                             *
* 1) SLOT STATUS1:                                                                            *
*                                                                                             *
*     D7-SL_SPURGE    ; 1=SLOT purge en,                                                      *
*                     ; ALSO adrSEL (INTERNAL use only) (not seen on NET!)                    *
*                                                                                             *
*     D6-SL_CONUP     ; CONDN/CONUP: bit encoding-Control double linked Consist List          *
*                     ;    11=LOGICAL MID CONSIST , Linked up AND down                        *
*                     ;    10=LOGICAL CONSIST TOP, Only linked downwards                      *
*                     ;    01=LOGICAL CONSIST SUB-MEMBER, Only linked upwards                 *
*                     ;    00=FREE locomotive, no CONSIST indirection/linking                 *
*                     ; ALLOWS "CONSISTS of CONSISTS". Uplinked means that                    *
*                     ; Slot SPD number is now SLOT adr of SPD/DIR and STATUS                 *
*                     ; of consist. i.e. is ;an Indirect pointer. This Slot                   *
*                     ; has same BUSY/ACTIVE bits as TOP of Consist. TOP is                   *
*                     ; loco with SPD/DIR for whole consist. (top of list).                   *
*                     ; BUSY/ACTIVE: bit encoding for SLOT activity                           *
*                                                                                             *
*     D5-SL_BUSY      ; 11=IN_USE loco adr in SLOT -REFRESHED                                 *
*                                                                                             *
*     D4-SL_ACTIVE    ; 10=IDLE loco adr in SLOT -NOT refreshed                               *
*                     ; 01=COMMON loco adr IN SLOT -refreshed                                 *
*                     ; 00=FREE SLOT, no valid DATA -not refreshed                            *
*                                                                                             *
*     D3-SL_CONDN     ; shows other SLOT Consist linked INTO this slot, see SL_CONUP          *
*                                                                                             *
*     D2-SL_SPDEX     ; 3 BITS for Decoder TYPE encoding for this SLOT                        *
*                                                                                             *
*     D1-SL_SPD14     ; 011=send 128 speed mode packets                                       *
*                                                                                             *
*     D0-SL_SPD28     ; 010=14 step MODE                                                      *
*                     ; 001=28 step. Generate Trinary packets for this                        *
*                     ;              Mobile ADR                                               *
*                     ; 000=28 step. 3 BYTE PKT regular mode                                  *
*                     ; 111=128 Step decoder, Allow Advanced DCC consisting                   *
*                     ; 100=28 Step decoder ,Allow Advanced DCC consisting                    *
*                                                                                             *
* 2) SLOT LOCO ADR:                                                                           *
*                                                                                             *
*     LOCO adr Low 7 bits (byte sent as ARG2 in ADR req opcode <0xBF>)                        *
*                                                                                             *
* 3) SLOT SPEED:                                                                              *
*     0x00=SPEED 0 ,STOP inertially                                                           *
*     0x01=SPEED 0 EMERGENCY stop                                                             *
*     0x02->0x7F increasing SPEED,0x7F=MAX speed                                              *
*     (byte also sent as ARG2 in SPD opcode <0xA0> )                                          *
*                                                                                             *
* 4) SLOT DIRF byte: (byte also sent as ARG2 in DIRF opcode <0xA1>)                           *
*                                                                                             *
*     D7-0        ; always 0                                                                  *
*     D6-SL_XCNT  ; reserved , set 0                                                          *
*     D5-SL_DIR   ; 1=loco direction FORWARD                                                  *
*     D4-SL_F0    ; 1=Directional lighting ON                                                 *
*     D3-SL_F4    ; 1=F4 ON                                                                   *
*     D2-SL_F3    ; 1=F3 ON                                                                   *
*     D1-SL_F2    ; 1=F2 ON                                                                   *
*     D0-SL_F1    ; 1=F1 ON                                                                   *
*                                                                                             *
*                                                                                             *
*                                                                                             *
*                                                                                             *
* 5) TRK byte: (GLOBAL system /track status)                                                  *
*                                                                                             *
*     D7-D4       Reserved                                                                    *
*     D3          GTRK_PROG_BUSY 1=Programming TRACK in this Master is BUSY.                  *
*     D2          GTRK_MLOK1     1=This Master IMPLEMENTS LocoNet 1.1 capability,             *
*                                0=Master is DT200                                            *
*     D1          GTRK_IDLE      0=TRACK is PAUSED, B'cast EMERG STOP.                        *
*     D0          GTRK_POWER     1=DCC packets are ON in MASTER, Global POWER up              *
*                                                                                             *
* 6) SLOT STATUS:                                                                             *
*                                                                                             *
*     D3          1=expansion IN ID1/2, 0=ENCODED alias                                       *
*     D2          1=Expansion ID1/2 is NOT ID usage                                           *
*     D0          1=this slot has SUPPRESSED ADV consist-7)                                   *
*                                                                                             *
* 7) SLOT LOCO ADR HIGH:                                                                      *
*                                                                                             *
* Locomotive address high 7 bits. If this is 0 then Low address is normal 7 bit NMRA SHORT    *
* address. If this is not zero then the most significant 6 bits of this address are used in   *
* the first LONG address byte ( matching CV17). The second DCC LONG address byte matches CV18 *
* and includes the Adr Low 7 bit value with the LS bit of ADR high in the MS postion of this  *
* track adr byte.                                                                             *
*                                                                                             *
* Note a DT200 MASTER will always interpret this as 0.                                        *
*                                                                                             *
* 8) SLOT SOUND:                                                                              *
*                                                                                             *
*     Slot sound/ Accesory Function mode II packets. F5-F8                                    *
*     (byte also sent as ARG2 in SND opcode)                                                  *
*                                                                                             *
*     D7-D4           reserved                                                                *
*     D3-SL_SND4/F8                                                                           *
*     D2-SL_SND3/F7                                                                           *
*     D1-SL_SND2/F6                                                                           *
*     D0-SL_SND1/F5   1= SLOT Sound 1 function 1active (accessory 2)                          *
*                                                                                             *
* 9) EXPANSION RESERVED ID1:                                                                  *
*                                                                                             *
*     7 bit ls ID code written by THROTTLE/PC when STAT2.4=1                                  *
*                                                                                             *
* 10) EXPANSION RESERVED ID2:                                                                 *
*                                                                                             *
*     7 bit ms ID code written by THROTTLE/PC when STAT2.4=1                                  *
**********************************************************************************************/
    case OPC_WR_SL_DATA:       /* page 10 of Loconet PE */
    case OPC_SL_RD_DATA:       /* page 10 of Loconet PE */
        {
            char mode[8];
            char locoAdrStr[32];

            rwSlotData = (rwSlotDataMsg *) msgBuf;

            /* build loco address string */
            ConvertToMixed(rwSlotData->adr, rwSlotData->adr2, mixedAdrStr);

            /* figure out the alias condition, and create the loco address string */
            if (rwSlotData->adr2 == 0x7f) {
                if ((rwSlotData->ss2 & STAT2_ALIAS_MASK) ==
                    STAT2_ID_IS_ALIAS) {
                    /* this is an aliased address and we have the alias */
                    sprintf(locoAdrStr, "%5d (Alias for loco %s)",
                            LOCO_ADR(rwSlotData->id2, rwSlotData->id1),
                            mixedAdrStr);
                }
                else {
                    /* this is an aliased address and we don't have the alias */
                    sprintf(mixedAdrStr, "%s (via Alias)", locoAdrStr);
                }
            }
            else {
                /* regular 4 digit address, 128 to 9983 */
                sprintf(locoAdrStr, "%s", mixedAdrStr);
            }

            /*
             *  These share a common data format with the only
             *  difference being whether we are reading or writing
             *  the slot data.
             */

            if (rwSlotData->command == OPC_WR_SL_DATA) {
                sprintf(mode, "Write");
            }
            else {
                sprintf(mode, "Read");
            }

            if (rwSlotData->slot == FC_SLOT) {
/**********************************************************************************************
* FAST Clock:                                                                                 *
* ===========                                                                                 *
* The system FAST clock and parameters are implemented in Slot#123 <7B>.                      *
*                                                                                             *
* Use <EF> to write new clock information, Slot read of 0x7B,<BB><7B>.., will return current  *
* System clock information, and other throttles will update to this SYNC. Note that all       *
* attached display devices keep a current clock calculation based on this SYNC read value,    *
* i.e. devices MUST not continuously poll the clock SLOT to generate time, but use this       *
* merely to restore SYNC and follow current RATE etc. This clock slot is typically "pinged"   *
* or read SYNC'd every 70 to 100 seconds , by a single user, so all attached devices can      *
* synchronise any phase drifts. Upon seeing a SYNC read, all devices should reset their local *
* sub-minute phase counter and invalidate the SYNC update ping generator.                     *
*                                                                                             *
* Clock Slot Format:                                                                          *
*                                                                                             *
* <0xEF>,<0E>,<7B>,<CLK_RATE>,<FRAC_MINSL>,<FRAC_MINSH>,<256-MINS_60>,                        *
* <TRK><256-HRS_24>,<DAYS>,<CLK_CNTRL>,<ID1>,<1D2>,<CHK>                                      *
*                                                                                             *
*     <CLK_RATE>      0=Freeze clock,                                                         *
*                     1=normal 1:1 rate,                                                      *
*                     10=10:1 etc, max VALUE is 7F/128 to 1                                   *
*     <FRAC_MINSL>    FRAC mins hi/lo are a sub-minute counter , depending                    *
*                         on the CLOCK generator                                              *
*     <FRAC_MINSH>    Not for ext. usage. This counter is reset when valid                    *
*                         <E6><7B> SYNC msg seen                                              *
*     <256-MINS_60>   This is FAST clock MINUTES subtracted from 256. Modulo 0-59             *
*     <256-HRS_24>    This is FAST clock HOURS subtracted from 256. Modulo 0-23               *
*     <DAYS>          number of 24 Hr clock rolls, positive count                             *
*     <CLK_CNTRL>     Clock Control Byte                                                      *
*                         D6- 1=This is valid Clock information,                              *
*                             0=ignore this <E6><7B>, SYNC reply                              *
*     <ID1>,<1D2>     This is device ID last setting the clock.                               *
*                         <00><00> shows no set has happened                                  *
*     <7F><7x>        are reserved for PC access                                              *
**********************************************************************************************/

                /* make message easier to deal with internally */
                fastClock = (fastClockMsg *) msgBuf;

                /* recover hours and minutes values */
                minutes = ((256 - fastClock->mins_60) & 0x7f) % 60;
                hours = ((256 - fastClock->hours_24) & 0x7f) % 24;
                hours = (24 - hours) % 24;
                minutes = (60 - minutes) % 60;

                /* check track status value and display */
                if ((trackStatus != fastClock->track_stat)
                    || showTrackStatus) {
                    trackStatus = fastClock->track_stat;
                    showStatus = TRUE;
                }

                if (showStatus) {
                    sprintf(logString,
                            "%s Fast Clock: (Data is %s)\n"
                            "\t  %s, rate is %d:1. Day %d, %02d:%02d. Last set by ID 0x%02x%02x\n"
                            "\t  Master controller %s, Track Status is %s, Programming Track is %s,\n",
                            mode,
                            (fastClock->
                             clk_cntrl & 0x20) ? "Valid" :
                            "Invalid - ignore",
                            fastClock->clk_rate ? "Running" : "Frozen",
                            fastClock->clk_rate, fastClock->days, hours,
                            minutes, fastClock->id2, fastClock->id1,
                            (fastClock->
                             track_stat & GTRK_MLOK1) ?
                            "implements LocoNet 1.1" : "is a DT-200",
                            (fastClock->
                             track_stat & GTRK_IDLE) ? "On" : "Off",
                            (fastClock->
                             track_stat & GTRK_PROG_BUSY) ? "Busy" :
                            "Available");
                }
                else {
                    sprintf(logString,
                            "%s Fast Clock: (Data is %s)\n"
                            "\t  %s, rate is %d:1. Day %d, %02d:%02d. Last set by ID 0x%02x%02x\n",
                            mode,
                            (fastClock->
                             clk_cntrl & 0x20) ? "Valid" :
                            "Invalid - ignore",
                            fastClock->clk_rate ? "Frozen" : "Running",
                            fastClock->clk_rate, fastClock->days, hours,
                            minutes, fastClock->id2, fastClock->id1);
                }
            }
            else if (msgBuf[2] == PRG_SLOT) {

/**********************************************************************************************
* Programmer track:                                                                           *
* =================                                                                           *
* The programmer track is accessed as Special slot #124 ( $7C, 0x7C). It is a full            *
* asynchronous shared system resource.                                                        *
*                                                                                             *
* To start Programmer task, write to slot 124. There will be an immediate LACK acknowledge    *
* that indicates what programming will be allowed. If a valid programming task is started,    *
* then at the final (asynchronous) programming completion, a Slot read <E7> from slot 124     *
* will be sent. This is the final task status reply.                                          *
*                                                                                             *
* Programmer Task Start:                                                                      *
* ----------------------                                                                      *
* <0xEF>,<0E>,<7C>,<PCMD>,<0>,<HOPSA>,<LOPSA>,<TRK>;<CVH>,<CVL>,                              *
*        <DATA7>,<0>,<0>,<CHK>                                                                *
*                                                                                             *
* This OPC leads to immediate LACK codes:                                                     *
*     <B4>,<7F>,<7F>,<chk>    Function NOT implemented, no reply.                             *
*     <B4>,<7F>,<0>,<chk>     Programmer BUSY , task aborted, no reply.                       *
*     <B4>,<7F>,<1>,<chk>     Task accepted , <E7> reply at completion.                       *
*     <B4>,<7F>,<0x40>,<chk>  Task accepted blind NO <E7> reply at completion.                *
*                                                                                             *
* Note that the <7F> code will occur in Operations Mode Read requests if the System is not    *
* configured for and has no Advanced Acknowlegement detection installed.. Operations Mode     *
* requests can be made and executed whilst a current Service Mode programming task is keeping *
* the Programming track BUSY. If a Programming request is rejected, delay and resend the      *
* complete request later. Some readback operations can keep the Programming track busy for up *
* to a minute. Multiple devices, throttles/PC's etc, can share and sequentially use the       *
* Programming track as long as they correctly interpret the response messages. Any Slot RD    *
* from the master will also contain the Programmer Busy status in bit 3 of the <TRK> byte.    *
*                                                                                             *
* A <PCMD> value of <00> will abort current SERVICE mode programming task and will echo with  *
* an <E6> RD the command string that was aborted.                                             *
*                                                                                             *
* <PCMD> Programmer Command:                                                                  *
* --------------------------                                                                  *
* Defined as                                                                                  *
*     D7 -0                                                                                   *
*     D6 -Write/Read  1= Write,                                                               *
*                     0=Read                                                                  *
*     D5 -Byte Mode   1= Byte operation,                                                      *
*                     0=Bit operation (if possible)                                           *
*     D4 -TY1 Programming Type select bit                                                     *
*     D3 -TY0 Prog type select bit                                                            *
*     D2 -Ops Mode    1=Ops Mode on Mainlines,                                                *
*                     0=Service Mode on Programming Track                                     *
*     D1 -0 reserved                                                                          *
*     D0 -0-reserved                                                                          *
*                                                                                             *
* Type codes:                                                                                 *
* -----------                                                                                 *
*     Byte Mode   Ops Mode   TY1   TY0   Meaning                                              *
*        1           0        0     0    Paged mode byte Read/Write on Service Track          *
*        1           0        0     0    Paged mode byte Read/Write on Service Track          *
*        1           0        0     1    Direct mode byteRead/Write on Service Track          *
*        0           0        0     1    Direct mode bit Read/Write on Service Track          *
*        x           0        1     0    Physical Register byte Read/Write on Service Track   *
*        x           0        1     1    Service Track- reserved function                     *
*        1           1        0     0    Ops mode Byte program, no feedback                   *
*        1           1        0     1    Ops mode Byte program, feedback                      *
*        0           1        0     0    Ops mode Bit program, no feedback                    *
*        0           1        0     1    Ops mode Bit program, feedback                       *
*                                                                                             *
*     <HOPSA>Operations Mode Programming                                                      *
*         7 High address bits of Loco to program, 0 if Service Mode                           *
*     <LOPSA>Operations Mode Programming                                                      *
*         7 Low address bits of Loco to program, 0 if Service Mode                            *
*     <TRK> Normal Global Track status for this Master,                                       *
*         Bit 3 also is 1 WHEN Service Mode track is BUSY                                     *
*     <CVH> High 3 BITS of CV#, and ms bit of DATA.7                                          *
*         <0,0,CV9,CV8 - 0,0, D7,CV7>                                                         *
*     <CVL> Low 7 bits of 10 bit CV address.                                                  *
*         <0,CV6,CV5,CV4-CV3,CV2,CV1,CV0>                                                     *
*     <DATA7>Low 7 BITS OF data to WR or RD COMPARE                                           *
*         <0,D6,D5,D4 - D3,D2,D1,D0>                                                          *
*         ms bit is at CVH bit 1 position.                                                    *
*                                                                                             *
* Programmer Task Final Reply:                                                                *
* ----------------------------                                                                *
* (if saw LACK <B4>,<7F>,<1>,<chk> code reply at task start)                                  *
*                                                                                             *
* <0xE7>,<0E>,<7C>,<PCMD>,<PSTAT>,<HOPSA>,<LOPSA>,<TRK>;<CVH>,<CVL>,                          *
* <DATA7>,<0>,<0>,<CHK>                                                                       *
*                                                                                             *
*     <PSTAT> Programmer Status error flags. Reply codes resulting from                       *
*             completed task in PCMD                                                          *
*         D7-D4 -reserved                                                                     *
*         D3    -1= User Aborted this command                                                 *
*         D2    -1= Failed to detect READ Compare acknowledge response                        *
*                   from decoder                                                              *
*         D1    -1= No Write acknowledge response from decoder                                *
*         D0    -1= Service Mode programming track empty- No decoder detected                 *
*                                                                                             *
* This <E7> response is issued whenever a Programming task is completed. It echos most of the *
* request information and returns the PSTAT status code to indicate how the task completed.   *
* If a READ was requested <DATA7> and <CVH> contain the returned data, if the PSTAT indicates *
* a successful readback (typically =0). Note that if a Paged Read fails to detect a           *
* successful Page write acknowledge when first setting the Page register, the read will be    *
* aborted, showing no Write acknowledge flag D1=1.                                            *
**********************************************************************************************/
                {
                    char operation[32];
                    char progMode[128];
                    byte cvData;
                    byte opsMode = FALSE;
                    int cvNumber;

                    progTaskMsg *progTask;


                    progTask = (progTaskMsg *) msgBuf;
                    cvData = PROG_DATA(progTask);
                    cvNumber = PROG_CV_NUM(progTask);

                    /* generate loco address, mixed mode or true 4 digit */
                    ConvertToMixed(progTask->lopsa, progTask->hopsa,
                                   mixedAdrStr);

                    /* determine programming mode for printing */
                    if ((progTask->pcmd & PCMD_MODE_MASK) ==
                        PAGED_ON_SRVC_TRK) {
                        sprintf(progMode,
                                "Byte in Paged Mode on Service Track");
                    }
                    else if ((progTask->pcmd & PCMD_MODE_MASK) ==
                             DIR_BYTE_ON_SRVC_TRK) {
                        sprintf(progMode,
                                "Byte in Direct Mode on Service Track");
                    }
                    else if ((progTask->pcmd & PCMD_MODE_MASK) ==
                             DIR_BIT_ON_SRVC_TRK) {
                        sprintf(progMode,
                                "Bits in Direct Mode on Service Track");
                    }
                    else if (((progTask->
                               pcmd & ~PCMD_BYTE_MODE) & PCMD_MODE_MASK) ==
                             REG_BYTE_RW_ON_SRVC_TRK) {
                        sprintf(progMode,
                                "Byte in Physical Register R/W Mode on Service Track");
                    }
                    else if ((progTask->pcmd & PCMD_MODE_MASK) ==
                             OPS_BYTE_NO_FEEDBACK) {
                        sprintf(progMode,
                                "Byte in OP's Mode (NO feedback)");
                        opsMode = TRUE;
                    }
                    else if ((progTask->pcmd & PCMD_MODE_MASK) ==
                             OPS_BYTE_FEEDBACK) {
                        sprintf(progMode, "Byte in OP's Mode");
                        opsMode = TRUE;
                    }
                    else if ((progTask->pcmd & PCMD_MODE_MASK) ==
                             OPS_BIT_NO_FEEDBACK) {
                        sprintf(progMode,
                                "Bits in OP's Mode (NO feedback)");
                        opsMode = TRUE;
                    }
                    else if ((progTask->pcmd & PCMD_MODE_MASK) ==
                             OPS_BIT_FEEDBACK) {
                        sprintf(progMode, "Bits in OP's Mode");
                        opsMode = TRUE;
                    }
                    else if (((progTask->
                               pcmd & ~PCMD_BYTE_MODE) & PCMD_MODE_MASK) ==
                             SRVC_TRK_RESERVED) {
                        sprintf(progMode,
                                "SERVICE TRACK RESERVED MODE DETECTED!");
                    }
                    else {
                        sprintf(progMode, "Unknown mode %d (0x%02x)",
                                progTask->pcmd, progTask->pcmd);
                        forceHex = TRUE;
                    }

                    /* are we sending or receiving? */
                    if (progTask->pcmd & PCMD_RW) {
                        /* sending a command */
                        sprintf(operation, "Programming: Write");

                        /* printout based on whether we're doing Ops mode or not */
                        if (opsMode) {
                            sprintf(logString,
                                    "%s %s %s\n"
                                    "\t  Setting CV%d of Loco %s to %d (0x%02x)\n",
                                    mode, operation, progMode, cvNumber,
                                    mixedAdrStr, cvData, cvData);
                        }
                        else {
                            sprintf(logString,
                                    "%s %s %s\n"
                                    "\t  Setting CV%d to %d (0x%02x)\n",
                                    mode, operation, progMode, cvNumber,
                                    cvData, cvData);
                        }
                    }
                    else {
                        /* receiving a reply */
                        sprintf(operation, "Programming: Read");

                        /* printout based on whether we're doing Ops mode or not */
                        if (opsMode) {
                            sprintf(logString,
                                    "%s %s %s\n"
                                    "\t  CV%d of Loco %s is %d (0x%02x)\n",
                                    mode, operation, progMode, cvNumber,
                                    mixedAdrStr, cvData, cvData);
                        }
                        else {
                            sprintf(logString,
                                    "%s %s %s\n"
                                    "\t  CV%d is %d (0x%02x)\n",
                                    mode, operation, progMode, cvNumber,
                                    cvData, cvData);
                        }

                        /* if we're reading the slot back, check the status        */
                        /* this is supposed to be the Programming task final reply */
                        /* and will have the resulting status byte                 */

                        if (progTask->command == OPC_SL_RD_DATA) {
                            if (progTask->pstat) {
                                if (progTask->pstat & PSTAT_USER_ABORTED) {
                                    sprintf(&logString[strlen(logString)],
                                            "\t  Status = Failed, User Aborted\n");
                                }

                                if (progTask->pstat & PSTAT_READ_FAIL) {
                                    sprintf(&logString[strlen(logString)],
                                            "\t  Status = Failed, Read Compare Acknowledge not detected\n");
                                }

                                if (progTask->pstat & PSTAT_WRITE_FAIL) {
                                    sprintf(&logString[strlen(logString)],
                                            "\t  Status = Failed, No Write Acknowledge from decoder\n");
                                }

                                if (progTask->pstat & PSTAT_NO_DECODER) {
                                    sprintf(&logString[strlen(logString)],
                                            "\t  Status = Failed, Service Mode programming track empty\n");
                                }
                            }
                            else {
                                sprintf(&logString[strlen(logString)],
                                        "\t  Status = Success\n");
                            }
                        }
                    }
                }
            }
            else {
/**************************************************
* normal slot read/write message - see info above *
**************************************************/

                if ((trackStatus != rwSlotData->trk) || showTrackStatus) {
                    trackStatus = rwSlotData->trk;
                    showStatus = TRUE;
                }

                if (showStatus) {
                    if (CONSISTED(rwSlotData->stat)) {
                        sprintf(logString,
                                "%s slot %3d:\n"
                                "\t  Loco %s is %s to slot %d, %s, and operating in %s SS mode,\n"
                                "\t  F0=%s F1=%s F2=%s F3=%s F4=%s Sound1/F5=%s Sound2/F6=%s Sound3/F7=%s Sound4/F8=%s\n"
                                "\t  Master controller %s, Track Status is %s, Programming Track is %s,\n"
                                "\t  SS2=0x%02x, ID =0x%02x%02x\n",
                                mode, rwSlotData->slot,
                                locoAdrStr,
                                CONSIST_STAT(rwSlotData->stat),
                                rwSlotData->spd,
                                LOCO_STAT(rwSlotData->stat),
                                DEC_MODE(rwSlotData->stat),
                                (rwSlotData->
                                 dirf & DIRF_F0) ? "On, " : "Off,",
                                (rwSlotData->
                                 dirf & DIRF_F1) ? "On, " : "Off,",
                                (rwSlotData->
                                 dirf & DIRF_F2) ? "On, " : "Off,",
                                (rwSlotData->
                                 dirf & DIRF_F3) ? "On, " : "Off,",
                                (rwSlotData->
                                 dirf & DIRF_F4) ? "On, " : "Off,",
                                (rwSlotData->
                                 snd & SND_F5) ? "On, " : "Off,",
                                (rwSlotData->
                                 snd & SND_F6) ? "On, " : "Off,",
                                (rwSlotData->
                                 snd & SND_F7) ? "On, " : "Off,",
                                (rwSlotData->snd & SND_F8) ? "On" : "Off",
                                (rwSlotData->
                                 trk & GTRK_MLOK1) ?
                                "implements LocoNet 1.1" : "is a DT-200",
                                (rwSlotData->
                                 trk & GTRK_IDLE) ? "On" : "Off",
                                (rwSlotData->
                                 trk & GTRK_PROG_BUSY) ? "Busy" :
                                "Available", rwSlotData->ss2,
                                rwSlotData->id2, rwSlotData->id1);
                    }
                    else {
                        sprintf(logString,
                                "%s slot %3d:\n"
                                "\t  Loco %s is %s, %s, operating in %s SS mode, and is going %s at speed %d,\n"
                                "\t  F0=%s F1=%s F2=%s F3=%s F4=%s Sound1/F5=%s Sound2/F6=%s Sound3/F7=%s Sound4/F8=%s\n"
                                "\t  Master controller %s, Track Status is %s, Programming Track is %s,\n"
                                "\t  SS2=0x%02x, ID =0x%02x%02x\n",
                                mode, rwSlotData->slot,
                                locoAdrStr,
                                CONSIST_STAT(rwSlotData->stat),
                                LOCO_STAT(rwSlotData->stat),
                                DEC_MODE(rwSlotData->stat),
                                (rwSlotData->
                                 dirf & DIRF_DIR) ? "in Reverse" :
                                "Foward", rwSlotData->spd,
                                (rwSlotData->
                                 dirf & DIRF_F0) ? "On, " : "Off,",
                                (rwSlotData->
                                 dirf & DIRF_F1) ? "On, " : "Off,",
                                (rwSlotData->
                                 dirf & DIRF_F2) ? "On, " : "Off,",
                                (rwSlotData->
                                 dirf & DIRF_F3) ? "On, " : "Off,",
                                (rwSlotData->
                                 dirf & DIRF_F4) ? "On, " : "Off,",
                                (rwSlotData->
                                 snd & SND_F5) ? "On, " : "Off,",
                                (rwSlotData->
                                 snd & SND_F6) ? "On, " : "Off,",
                                (rwSlotData->
                                 snd & SND_F7) ? "On, " : "Off,",
                                (rwSlotData->snd & SND_F8) ? "On" : "Off",
                                (rwSlotData->
                                 trk & GTRK_MLOK1) ?
                                "implements LocoNet 1.1" : "is a DT-200",
                                (rwSlotData->
                                 trk & GTRK_IDLE) ? "On" : "Off",
                                (rwSlotData->
                                 trk & GTRK_PROG_BUSY) ? "Busy" :
                                "Available", rwSlotData->ss2,
                                rwSlotData->id2, rwSlotData->id1);
                    }
                }
                else {
                    if (CONSISTED(rwSlotData->stat)) {
                        sprintf(logString,
                                "%s slot %3d:\n"
                                "\t  Loco %s is %s to slot %d, %s, and operating in %s SS mode,\n"
                                "\t  F0=%s F1=%s F2=%s F3=%s F4=%s Sound1/F5=%s Sound2/F6=%s Sound3/F7=%s Sound4/F8=%s\n"
                                "\t  SS2=0x%02x, ID =0x%02x%02x\n",
                                mode, rwSlotData->slot,
                                locoAdrStr,
                                CONSIST_STAT(rwSlotData->stat),
                                rwSlotData->spd,
                                LOCO_STAT(rwSlotData->stat),
                                DEC_MODE(rwSlotData->stat),
                                (rwSlotData->
                                 dirf & DIRF_F0) ? "On, " : "Off,",
                                (rwSlotData->
                                 dirf & DIRF_F1) ? "On, " : "Off,",
                                (rwSlotData->
                                 dirf & DIRF_F2) ? "On, " : "Off,",
                                (rwSlotData->
                                 dirf & DIRF_F3) ? "On, " : "Off,",
                                (rwSlotData->
                                 dirf & DIRF_F4) ? "On, " : "Off,,",
                                (rwSlotData->
                                 snd & SND_F5) ? "On, " : "Off,",
                                (rwSlotData->
                                 snd & SND_F6) ? "On, " : "Off,",
                                (rwSlotData->
                                 snd & SND_F7) ? "On, " : "Off,",
                                (rwSlotData->snd & SND_F8) ? "On" : "Off",
                                rwSlotData->ss2, rwSlotData->id2,
                                rwSlotData->id1);
                    }
                    else {
                        sprintf(logString,
                                "%s slot %3d:\n"
                                "\t  Loco %s is %s, %s, operating in %s SS mode, and is going %s at speed %d,\n"
                                "\t  F0=%s F1=%s F2=%s F3=%s F4=%s Sound1/F5=%s Sound2/F6=%s Sound3/F7=%s Sound4/F8=%s\n"
                                "\t  SS2=0x%02x, ID =0x%02x%02x\n",
                                mode, rwSlotData->slot,
                                locoAdrStr,
                                CONSIST_STAT(rwSlotData->stat),
                                LOCO_STAT(rwSlotData->stat),
                                DEC_MODE(rwSlotData->stat),
                                (rwSlotData->
                                 dirf & DIRF_DIR) ? "Foward" :
                                "in Reverse", rwSlotData->spd,
                                (rwSlotData->
                                 dirf & DIRF_F0) ? "On, " : "Off,",
                                (rwSlotData->
                                 dirf & DIRF_F1) ? "On, " : "Off,",
                                (rwSlotData->
                                 dirf & DIRF_F2) ? "On, " : "Off,",
                                (rwSlotData->
                                 dirf & DIRF_F3) ? "On, " : "Off,",
                                (rwSlotData->
                                 dirf & DIRF_F4) ? "On, " : "Off,,",
                                (rwSlotData->
                                 snd & SND_F5) ? "On, " : "Off,",
                                (rwSlotData->
                                 snd & SND_F6) ? "On, " : "Off,",
                                (rwSlotData->
                                 snd & SND_F7) ? "On, " : "Off,",
                                (rwSlotData->snd & SND_F8) ? "On" : "Off",
                                rwSlotData->ss2, rwSlotData->id2,
                                rwSlotData->id1);
                    }
                }
            }

            break;
        }

/***********************************************************************************
* OPC_PEER_XFER    0xE5   ; move 8 bytes PEER to PEER, SRC->DST                    *
*                         ; Message has response                                   *
*                         ; <0xE5>,<10>,<SRC>,<DSTL><DSTH>,<PXCT1>,<D1>,           *
*                         ;        <D2>,<D3>,<D4>,<PXCT2>,<D5>,<D6>,<D7>,          *
*                         ;        <D8>,<CHK>                                      *
*                         ;   SRC/DST are 7 bit args. DSTL/H=0 is BROADCAST msg    *
*                         ;   SRC=0 is MASTER                                      *
*                         ;   SRC=0x70-0x7E are reserved                           *
*                         ;   SRC=7F is THROTTLE msg xfer,                         *
*                         ;        <DSTL><DSTH> encode ID#,                        *
*                         ;        <0><0> is THROT B'CAST                          *
*                         ;   <PXCT1>=<0,XC2,XC1,XC0 - D4.7,D3.7,D2.7,D1.7>        *
*                         ;        XC0-XC2=ADR type CODE-0=7 bit Peer TO Peer adrs *
*                         ;           1=<D1>is SRC HI,<D2>is DST HI                *
*                         ;   <PXCT2>=<0,XC5,XC4,XC3 - D8.7,D7.7,D6.7,D5.7>        *
*                         ;        XC3-XC5=data type CODE- 0=ANSI TEXT string,     *
*                         ;           balance RESERVED                             *
***********************************************************************************/
    case OPC_PEER_XFER:        /* page 10 of Loconet PE */
        peerXfer = (peerXferMsg *) msgBuf;

        sprintf(logString,
                "Peer to Peer transfer: SRC=0x%02x, DSTL=0x%02x, DSTH=0x%02x, PXCT1=0x%02x, PXCT2=0x%02x,\n"
                "\tD1=0x%02x, D2=0x%02x, D3=0x%02x, D4=0x%02x, D5=0x%02x, D6=0x%02x, D7=0x%02x, D8=0x%02x\n",
                peerXfer->src, peerXfer->dst_l, peerXfer->dst_h,
                peerXfer->pxct1, peerXfer->pxct2, peerXfer->d1,
                peerXfer->d2, peerXfer->d3, peerXfer->d4, peerXfer->d5,
                peerXfer->d6, peerXfer->d7, peerXfer->d8);
        break;

/**************************************************************************
* OPC_IMM_PACKET   0xED   ;SEND n-byte packet immediate LACK              *
*                         ; Follow on message: LACK                       *
*                         ; <0xED>,<0B>,<7F>,<REPS>,<DHI>,<IM1>,<IM2>,    *
*                         ;        <IM3>,<IM4>,<IM5>,<CHK>                *
*                         ;   <DHI>=<0,0,1,IM5.7-IM4.7,IM3.7,IM2.7,IM1.7> *
*                         ;   <REPS>  D4,5,6=#IM bytes,                   *
*                         ;           D3=0(reserved);                     *
*                         ;           D2,1,0=repeat CNT                   *
*                         ; IF Not limited MASTER then                    *
*                         ;   LACK=<B4>,<7D>,<7F>,<chk> if CMD ok         *
*                         ; IF limited MASTER then Lim Masters respond    *
*                         ;   with <B4>,<7E>,<lim adr>,<chk>              *
*                         ; IF internal buffer BUSY/full respond          *
*                         ;   with <B4>,<7D>,<0>,<chk>                    *
*                         ;   (NOT IMPLEMENTED IN DT200)                  *
**************************************************************************/
    case OPC_IMM_PACKET:       /* page 11 of Loconet PE */
        sendPkt = (sendPktMsg *) msgBuf;

        /* see if it really is a 'Send Packet' as defined in Loconet PE */
        if (sendPkt->val7f == 0x7f) {
            /* it is */
            sprintf(logString,
                    "Send packet immediate: %d bytes, repeat count %d, DHI=0%02x,"
                    "\tIM1=0x%02x, IM2=0x%02x, IM3=0x%02x, IM4=0x%02x, IM5=0x%02x\n",
                    ((sendPkt->reps & 0x70) >> 4),
                    (sendPkt->reps & 0x07),
                    sendPkt->dhi,
                    sendPkt->im1, sendPkt->im2, sendPkt->im3, sendPkt->im4,
                    sendPkt->im5);
        }
        else {
            /* Hmmmm... */
            sprintf(logString, "Weird Send Packet Immediate, 3rd byte "
                    "id 0x%02x not 0x7f\n", sendPkt->val7f);
            forceHex = TRUE;
        }

        break;

    default:
        forceHex = TRUE;

        if (msgBuf[1] == 6) {
                /*******************************************************
                * ; 6 Byte MESSAGE OPCODES                             *
                * ; FORMAT = <OPC>,<ARG1>,<ARG2>,<ARG3>,<ARG4>,<CKSUM> *
                * <reserved>                                           *
                *******************************************************/
            sprintf(logString,
                    "This is a six byte message and is RESERVED!\n");
        }
        else {
            sprintf(logString, "Command is not defined in Loconet "
                    "Personal Use Edition 1.0\n");
        }

        break;
    }

    DBG(busnumber, DBG_INFO, "%s", logString);
}
