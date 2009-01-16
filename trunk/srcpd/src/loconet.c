/* $Id$ */

/*
 * loconet: loconet/srcp gateway
 */

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <sys/ioctl.h>

#include "config.h"
#ifdef HAVE_LINUX_SERIAL_H
#include "linux/serial.h"
#else
#warning "MS100 support for Linux only!"
#endif

#include "config-srcpd.h"
#include "io.h"
#include "loconet.h"
#include "srcp-fb.h"
#include "srcp-ga.h"
#include "srcp-gl.h"
#include "srcp-sm.h"
#include "srcp-power.h"
#include "srcp-server.h"
#include "srcp-info.h"
#include "srcp-session.h"
#include "srcp-error.h"
#include "syslogmessage.h"

#define __loconet ((LOCONET_DATA*)buses[busnumber].driverdata)
#define __loconett ((LOCONET_DATA*)buses[btd->bus].driverdata)

static int init_gl_LOCONET(gl_state_t *);
static int init_ga_LOCONET(ga_state_t *);
/**
 * Read and analyze the XML subtree for the <loconet> configuration.
 *
 */
int readConfig_LOCONET(xmlDocPtr doc, xmlNodePtr node, bus_t busnumber)
{
    xmlNodePtr child = node->children;
    xmlChar *txt;

    buses[busnumber].driverdata = malloc(sizeof(struct _LOCONET_DATA));

    if (buses[busnumber].driverdata == NULL) {
        syslog_bus(busnumber, DBG_ERROR,
                   "Memory allocation error in module '%s'.", node->name);
        return 0;
    }

    buses[busnumber].type = SERVER_LOCONET;
    buses[busnumber].init_func = &init_bus_LOCONET;
    buses[busnumber].thr_func = &thr_sendrec_LOCONET;
    buses[busnumber].init_gl_func = &init_gl_LOCONET;
    buses[busnumber].init_ga_func = &init_ga_LOCONET;

    __loconet->number_fb = 2048;        /* max address for OPC_INPUT_REP (10+1 bit) */
    __loconet->number_ga = 2048;        /* max address for OPC_SW_REQ */
    __loconet->number_gl = 9999;        /* DCC address range */
    __loconet->loconetID = 0x50;        /* Loconet ID */
    memset(__loconet->slotmap, 0, sizeof(__loconet->slotmap) );
    __loconet->flags = LN_FLAG_ECHO;

    strcpy(buses[busnumber].description, "GA FB POWER DESCRIPTION");

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
            syslog_bus(busnumber, DBG_WARN,
                       "WARNING, unknown tag found: \"%s\"!\n",
                       child->name);;

        child = child->next;
    }                           /* while */

    if (init_FB(busnumber, __loconet->number_fb)) {
        syslog_bus(busnumber, DBG_ERROR,
                   "Can't create array for feedback");
    }

    if (init_GA(busnumber, __loconet->number_ga)) {
        syslog_bus(busnumber, DBG_ERROR, "Can't create array for GA");
    }

    if (init_GL(busnumber, __loconet->number_gl)) {
        syslog_bus(busnumber, DBG_ERROR, "Can't create array for GL");
    }

    return (1);
}

static int init_lineLOCONET_serial(bus_t busnumber)
{
    int fd;
    int result;
    struct termios interface;

    fd = open(buses[busnumber].device.file.path,
              O_RDWR | O_NDELAY | O_NOCTTY);
    if (fd == -1) {
        syslog_bus(busnumber, DBG_ERROR,
                   "Device open failed: %s (errno = %d). "
                   "Terminating...\n", strerror(errno), errno);
        return 1;
    }
    buses[busnumber].device.file.fd = fd;
#ifdef HAVE_LINUX_SERIAL_H
    if ((__loconet->flags & LN_FLAG_MS100) == 1) {
        struct serial_struct serial;
        struct termios tios;
        unsigned int cm;

        result = ioctl(fd, TIOCGSERIAL, &serial);
        if (result == -1) {
            syslog_bus(busnumber, DBG_ERROR,
                       "ioctl() failed: %s (errno = %d)\n",
                       strerror(errno), errno);
        }

        serial.custom_divisor = 7;
        serial.flags &= ~ASYNC_USR_MASK;
        serial.flags |= ASYNC_SPD_CUST | ASYNC_LOW_LATENCY;

        result = ioctl(fd, TIOCSSERIAL, &serial);
        if (result == -1) {
            syslog_bus(busnumber, DBG_ERROR,
                       "ioctl() failed: %s (errno = %d)\n",
                       strerror(errno), errno);
        }

        tcgetattr(fd, &tios);
        tios.c_iflag = IGNBRK | IGNPAR;
        tios.c_oflag = 0;
        tios.c_cflag = CS8 | CREAD | CLOCAL;
        tios.c_lflag = 0;
        cfsetospeed(&tios, buses[busnumber].device.file.baudrate);
        tcsetattr(fd, TCSANOW, &tios);

        tcflow(fd, TCOON);
        tcflow(fd, TCION);

        result = ioctl(fd, TIOCMGET, &cm);
        if (result == -1) {
            syslog_bus(busnumber, DBG_ERROR,
                       "ioctl() failed: %s (errno = %d)\n",
                       strerror(errno), errno);
        }

        cm &= ~TIOCM_DTR;
        cm |= TIOCM_RTS | TIOCM_CTS;
        result = ioctl(fd, TIOCMSET, &cm);
        if (result == -1) {
            syslog_bus(busnumber, DBG_ERROR,
                       "ioctl() failed: %s (errno = %d)\n",
                       strerror(errno), errno);
        }

        tcflush(fd, TCOFLUSH);
        tcflush(fd, TCIFLUSH);
    }
    else {
#endif
        tcgetattr(fd, &interface);
        interface.c_oflag = ONOCR;
        interface.c_cflag =
            CS8 | CRTSCTS | CSTOPB | CLOCAL | CREAD | HUPCL;
        interface.c_iflag = IGNBRK;
        interface.c_lflag = IEXTEN;
        interface.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
        cfsetispeed(&interface, buses[busnumber].device.file.baudrate);
        cfsetospeed(&interface, buses[busnumber].device.file.baudrate);
        interface.c_cc[VMIN] = 0;
        interface.c_cc[VTIME] = 0;

        tcsetattr(fd, TCSANOW, &interface);
#ifdef HAVE_LINUX_SERIAL_H
    }
#endif
    return 1;

}

static int init_lineLOCONET_lbserver(bus_t busnumber)
{
    int sockfd;
    struct sockaddr_in serv_addr;
    struct hostent *server;
    ssize_t sresult;

    char msg[256];
    sockfd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sockfd == -1) {
        syslog_bus(busnumber, DBG_ERROR,
                   "Socket creation failed: %s (errno = %d).\n",
                   strerror(errno), errno);
        /*TODO: What to do now? Return some error value? */
    }
    server = gethostbyname(buses[busnumber].device.net.hostname);
    if(NULL == server) {
        server = gethostbyaddr(buses[busnumber].device.net.hostname,
	strlen(buses[busnumber].device.net.hostname), AF_INET);
	if(NULL==server) {
	   syslog_bus(busnumber, DBG_ERROR,
                   "cannot resolve address: %s.\n",
                   buses[busnumber].device.net.hostname);
        }
    }
    memcpy((char *) &serv_addr.sin_addr,
           (char *) server->h_addr,
            server->h_length);
    serv_addr.sin_port = htons(buses[busnumber].device.net.port);
    serv_addr.sin_family = AF_INET;

    alarm(30);

    if (connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr))
        < 0) {
        syslog_bus(busnumber, DBG_ERROR, "ERROR connecting to %s:%d %d", 
	buses[busnumber].device.net.hostname, buses[busnumber].device.net.port,
	errno);
	/*TODO: What to do now? Return some error value? */
    }
    alarm(0);

    sresult = socket_readline(sockfd, msg, sizeof(msg) - 1);

    /* client terminated connection */
    if (0 == sresult) {
        shutdown(sockfd, SHUT_RDWR);
        return 0;
    }

    /* read errror */
    if (-1 == sresult) {
        syslog_bus(busnumber, DBG_ERROR,
                   "Socket read failed: %s (errno = %d)\n",
                   strerror(errno), errno);
        return (-1);
    }

    syslog_bus(busnumber, DBG_INFO, "connected to %s", msg);
    buses[busnumber].device.net.sockfd = sockfd;
    return 1;

}

static int init_lineLOCONET(bus_t busnumber)
{
    switch (buses[busnumber].devicetype) {
        case HW_FILENAME:
            return init_lineLOCONET_serial(busnumber);
            break;
        case HW_NETWORK:
            return init_lineLOCONET_lbserver(busnumber);
            break;
    }
    return 0;
}

/**
 * cacheInitGL: modifies the gl data used to initialize the device

 */
static int init_gl_LOCONET(gl_state_t * gl)
{
    return SRCP_OK;
}

/**
 * initGA: modifies the ga data used to initialize the device

 */
static int init_ga_LOCONET(ga_state_t * ga)
{
    return SRCP_OK;
}

int init_bus_LOCONET(bus_t busnumber)
{
    static char* protocols = "N";
    buses[busnumber].protocols = protocols;
    __loconet->sent_packets = __loconet->recv_packets = 0;
    syslog_bus(busnumber, DBG_INFO, "Loconet init: bus #%d, debug %d",
               busnumber, buses[busnumber].debuglevel);
    if (buses[busnumber].debuglevel <= 5) {
        syslog_bus(busnumber, DBG_INFO, "Loconet bus %ld open device %s",
                   busnumber, buses[busnumber].device.file.path);
        init_lineLOCONET(busnumber);
        /*TODO: Check return value of line initialization and trigger
         * proper error action. */
    }
    syslog_bus(busnumber, DBG_INFO, "Loconet bus %ld init done",
               busnumber);
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

static int
ln_isecho(bus_t busnumber, const unsigned char *ln_packet,
          unsigned char ln_packetlen)
{
    int i;
    /* do we check for echos? */
    if ((__loconet->flags & LN_FLAG_ECHO) == 0) {
        return false;
    }

    if (__loconet->ln_msglen == 0)
        return false;
    for (i = 0; i < ln_packetlen; i++) {
        if (ln_packet[i] != __loconet->ln_message[i])
            return false;
    }

    return true;
}

static int ln_read_serial(bus_t busnumber, unsigned char *cmd, int len)
{
    int fd = buses[busnumber].device.file.fd;
    int index = 1;
    fd_set fds;
    struct timeval t = { 0, 0 };
    int retval;
    ssize_t result;

    FD_ZERO(&fds);
    FD_SET(fd, &fds);
    retval = select(fd + 1, &fds, NULL, NULL, &t);
    if (retval > 0 && FD_ISSET(fd, &fds)) {
        /* read data from locobuffer */
        int pktlen;
        unsigned char c;
        /* read exactly one Loconet packet; there must be at least one
           character due to select call result */
        /* a valid Loconet packet starts with a byte >= 0x080
           and contains one or more bytes <0x80.
         */
        do {
            result = read(fd, &c, 1);
            if (result == -1) {
                syslog_bus(busnumber, DBG_ERROR,
                           "read() failed: %s (errno = %d)\n",
                           strerror(errno), errno);
                /*TODO: appropriate action */
            }
        }
        while (c < 0x80);

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
                result = read(fd, &pktlen, 1);
                if (result == -1) {
                    syslog_bus(busnumber, DBG_ERROR,
                               "could not read number of bytes in loconet packet: %s (errno = %d)\n",
                               strerror(errno), errno);
                    /*TODO: appropriate action */
                }
                cmd[1] = pktlen;
                index = 2;
                break;
        }

        cmd[0] = c;

        result = read(fd, &cmd[index], pktlen - 1);
        if (result == -1) {
            syslog_bus(busnumber, DBG_ERROR,
                       "could not read loconet packet read() failed: %s (errno = %d)\n",
                       strerror(errno), errno);
            /*TODO: appropriate action */
        }

        retval = pktlen;
        if (ln_isecho(busnumber, cmd, pktlen)) {
            __loconet->ln_msglen = 0;
            syslog_bus(busnumber, DBG_DEBUG,
                       "this is the echo of the last "
                       "packet sent, clear to sent next command!");
            retval = 0;         /* we ignore echos */
        }
        else {
            __loconet->recv_packets++;
        }
    }
    else if (retval == -1) {
        syslog_bus(busnumber, DBG_ERROR,
                   "Select failed: %s (errno = %d)\n", strerror(errno),
                   errno);
    }

    return retval;
}


static int ln_read_lbserver(bus_t busnumber, unsigned char *cmd, int len)
{
    int fd = buses[busnumber].device.net.sockfd;
    fd_set fds;
    struct timeval t = { 0, 0 };
    int retval = 0;
    char line[256];
    ssize_t result;

    FD_ZERO(&fds);
    FD_SET(fd, &fds);
    retval = select(fd + 1, &fds, NULL, NULL, &t);

    if (retval > 0 && FD_ISSET(fd, &fds)) {
        result = socket_readline(fd, line, sizeof(line) - 1);

        /* client terminated connection */
        /*if (0 == result) {
           shutdown(fd, SHUT_RDWR);
           return 0;
           } */

        /* read errror */
        /*else */ if (-1 == result) {
            syslog_bus(busnumber, DBG_ERROR,
                       "Socket read failed: %s (errno = %d)\n",
                       strerror(errno), errno);
            return (-1);
        }

        /* line may begin with
           SENT message: last command was sent (or not)
           RECEIVE message: new message from Loconet
           VERSION text: VERSION information about the server */

        if (strstr(line, "RECEIVE ")) {
            /* we have a fixed format */
            size_t len = strlen(line) - 7;
            int pktlen = len / 3;
            int i;
            char *d;
            syslog_bus(busnumber, DBG_DEBUG, " * message '%s' %d bytes",
                       line + 7, pktlen);
            for (i = 0; i < pktlen; i++) {
                cmd[i] = strtol(line + 7 + 3 * i, &d, 16);
                /* syslog_bus(busnumber, DBG_DEBUG, " * %d %d ", i, cmd[i]); */
            }
            retval = pktlen;
        }
        __loconet->recv_packets++;
    }
    else if (retval == -1) {
        syslog_bus(busnumber, DBG_ERROR,
                   "Select failed: %s (errno = %d)\n", strerror(errno),
                   errno);
    }
    return retval;
}

static int ln_read(bus_t busnumber, unsigned char *cmd, int len)
{
    switch (buses[busnumber].devicetype) {
        case HW_FILENAME:
            return ln_read_serial(busnumber, cmd, len);
            break;
        case HW_NETWORK:
            return ln_read_lbserver(busnumber, cmd, len);
            break;
    }
    return 0;
}


static int
ln_write_lbserver(long int busnumber, const unsigned char *cmd,
                  unsigned char len)
{
    unsigned char i;
    ssize_t result;
    char msg[256], tmp[10];
    sprintf(msg, "SEND");
    for (i = 0; i < len; i++) {
        sprintf(tmp, " %02X", cmd[i]);
        strcat(msg, tmp);
    }
    strcat(msg, "\r\n");
    result = writen(buses[busnumber].device.net.sockfd, msg, strlen(msg));
    if (result == -1)
        syslog_bus(busnumber, DBG_ERROR,
                   "Socket write failed: %s (errno = %d)\n",
                   strerror(errno), errno);

    syslog_bus(busnumber, DBG_DEBUG,
               "sent Loconet packet with OPC 0x%02x, %d bytes (%s)",
               cmd[0], len, msg);
    __loconet->sent_packets++;
    __loconet->ln_msglen = 0;
    return 0;
}


static int
ln_write_serial(bus_t busnumber, const unsigned char *cmd,
                unsigned char len)
{
    unsigned char i;
    for (i = 0; i < len; i++) {
        writeByte(busnumber, cmd[i], 0);
    }
    syslog_bus(busnumber, DBG_DEBUG,
               "sent Loconet packet with OPC 0x%02x, %d bytes", cmd[0],
               len);
    __loconet->sent_packets++;
    __loconet->ln_msglen = len;
    memcpy(__loconet->ln_message, cmd, len);
    return 0;
}

static int
ln_write(bus_t busnumber, const unsigned char *cmd, unsigned char len)
{
    switch (buses[busnumber].devicetype) {
        case HW_FILENAME:
            return ln_write_serial(busnumber, cmd, len);
            break;
        case HW_NETWORK:
            return ln_write_lbserver(busnumber, cmd, len);
            break;
    }
    return 0;
}

/*thread cleanup routine for this bus*/
static void end_bus_thread(bus_thread_t * btd)
{
    int result;

    syslog_bus(btd->bus, DBG_INFO, "Loconet bus terminated.");

    switch (buses[btd->bus].devicetype) {
        case HW_FILENAME:
            close(buses[btd->bus].device.file.fd);
            break;
        case HW_NETWORK:
            shutdown(buses[btd->bus].device.net.sockfd, SHUT_RDWR);
            close(buses[btd->bus].device.net.sockfd);
            break;
    }

    syslog_bus(btd->bus, DBG_INFO,
               "Loconet bus: %u packets sent, %u packets received",
               __loconett->sent_packets, __loconett->recv_packets);

    result = pthread_mutex_destroy(&buses[btd->bus].transmit_mutex);
    if (result != 0) {
        syslog_bus(btd->bus, DBG_WARN,
                   "pthread_mutex_destroy() failed: %s (errno = %d).",
                   strerror(result), result);
    }

    result = pthread_cond_destroy(&buses[btd->bus].transmit_cond);
    if (result != 0) {
        syslog_bus(btd->bus, DBG_WARN,
                   "pthread_mutex_init() failed: %s (errno = %d).",
                   strerror(result), result);
    }

    free(buses[btd->bus].driverdata);
    free(btd);
}

void *thr_sendrec_LOCONET(void *v)
{
    unsigned char ln_packet[128];       /* max length is coded with 7 bit */
    unsigned char ln_packetlen = 2;
    unsigned int addr, timeoutcnt;
    /*int code, src, dst, data[8], i;*/
    int value, port, speed, busnumber;
    char msg[110];
    ga_state_t gatmp;
    int last_cancel_state, last_cancel_type;

    bus_thread_t *btd = (bus_thread_t *) malloc(sizeof(bus_thread_t));
    if (btd == NULL)
        pthread_exit((void *) 1);
    btd->bus = (bus_t) v;
    btd->fd = -1;
    busnumber = btd->bus; /* to keep the __loconet macro happy */
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &last_cancel_state);
    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, &last_cancel_type);

    /*register cleanup routine */
    pthread_cleanup_push((void *) end_bus_thread, (void *) btd);

    timeoutcnt = 0;

    while (1) {
        pthread_testcancel();
        buses[btd->bus].watchdog = 1;
        memset(ln_packet, 0, sizeof(ln_packet));
        /* first action is always a read _from_ Loconet */
        if ((ln_packetlen = ln_read(btd->bus, ln_packet,
                                    sizeof(ln_packet))) > 0) {

            switch (ln_packet[0]) {
                    /* basic operations, 2byte Commands on Loconet */
                case OPC_GPOFF:
                    buses[btd->bus].power_state = 0;
                    strcpy(buses[btd->bus].power_msg, "from Loconet");
                    infoPower(btd->bus, msg);
                    enqueueInfoMessage(msg);
                    break;
                case OPC_GPON:
                    buses[btd->bus].power_state = 1;
                    strcpy(buses[btd->bus].power_msg, "from Loconet");
                    infoPower(btd->bus, msg);
                    enqueueInfoMessage(msg);
                    break;
                    /* */
		case OPC_LONG_ACK:
                    syslog_bus(btd->bus, DBG_DEBUG,
                               "Infomational: LONG ACK for command 0x%0x: 0x%0x",
                               ln_packet[1], ln_packet[2]);
		    break;
                case OPC_SW_REQ:       /* B0 */
                    addr = (ln_packet[1] | ((ln_packet[2] & 0x0f) << 7)) + 1;
                    value = (ln_packet[2] & 0x10) >> 4;
                    port = (ln_packet[2] & 0x20) >> 5;
                    getGA(btd->bus, addr, &gatmp);
                    gatmp.action = value;
                    gatmp.port = port;
                    syslog_bus(btd->bus, DBG_DEBUG,
                               "Infomational: switch request (OPC_SW_REQ: /* B0 */)  #%d:%d -> %d",
                               addr, port, value);

                    setGA(btd->bus, addr, gatmp);
                    break;
                    /* some commands on the loconet,  */
                case OPC_RQ_SL_DATA:   /* BB, E7 Message follows */
                    addr = ln_packet[1];
                    syslog_bus(btd->bus, DBG_DEBUG,
                               "Infomational: Request SLOT DATA (OPC_RQ_SL_DATA: /* BB */)  #%d",
                               addr);
                    break;
                case OPC_LOCO_ADR:     /* BF, E7 Message follows */
                    addr = (ln_packet[1] << 7) | ln_packet[2];
                    syslog_bus(btd->bus, DBG_DEBUG,
                               "Informational: request loco address (OPC_LOCO_ADR:  /* BF */)  #%d",
                               addr);
                    break;
                    /* loco data, unfortunatly with slot addresses and not decoder addresses */
                case OPC_LOCO_SPD:     /* A0 */
                    addr = ln_packet[1];
                    speed = ln_packet[2];
                    syslog_bus(btd->bus, DBG_DEBUG,
                               "Set loco speed (OPC_LOCO_SPD:  /* A0 */) %d: %d",
                               addr, speed);
		    if(__loconet->slotmap[addr]==0) {
                        syslog_bus(btd->bus, DBG_DEBUG,
                               "slot %d still unknown", addr);
		    } else {
                        syslog_bus(btd->bus, DBG_DEBUG,
                               "decoder address %d found in slot %d", __loconet->slotmap[addr], addr);
		    }
                    break;
                case OPC_LOCO_DIRF:    /* A1 */
                    addr = ln_packet[1];
                    speed = ln_packet[2];
                    syslog_bus(btd->bus, DBG_DEBUG,
                               "New flags for loco in slot (OPC_LOCO_DIRF:  /* A1 */) #%d: %d",
                               addr, speed);
                    break;

                case OPC_SW_REP:       /* B1 */
                    break;
                case OPC_INPUT_REP:    /* B2 */
                    addr = ln_packet[1] | ((ln_packet[2] & 0x000f) << 7);
                    addr = 1 + addr * 2 + ((ln_packet[2] & 0x0020) >> 5);
                    value = (ln_packet[2] & 0x10) >> 4;
                    updateFB(btd->bus, addr, value);
                    break;
                case OPC_SL_RD_DATA:   /* E7 */
                    switch (ln_packet[1]) {
                        case 0x0e:
                            addr = ln_packet[4] | (ln_packet[9] << 7);
                            speed = ln_packet[5];
                            syslog_bus(btd->bus, DBG_DEBUG,
                                       "OPC_SL_RD_DATA: /* E7 %0X */ slot #%d: status=0x%0x addr=%d speed=%d",
                                       ln_packet[1], ln_packet[2],
                                       ln_packet[3], addr, speed);
			    __loconet -> slotmap[ln_packet[2]] = addr;
                            break;
                        default:
                            syslog_bus(btd->bus, DBG_DEBUG,
                                       "unknown OPC_SL_RD_DATA: /* E7 0x%0X */",
                                       ln_packet[1]);
                    }
                    break;
                default:
                    syslog_bus(btd->bus, DBG_DEBUG,
                               "Unkown Loconet Message (%x)",
                               ln_packet[0]);
                    /* unknown Loconet packet received, ignored */
                    break;
            }
        }
        if (__loconett->ln_msglen == 0) {
            /* now we process the way back _to_ Loconet */
            ln_packet[0] = OPC_IDLE;
            ln_packetlen = 2;
            if (buses[btd->bus].power_changed == 1) {
                ln_packet[0] = 0x82 + buses[btd->bus].power_state;
                ln_packetlen = 2;
                buses[btd->bus].power_changed = 0;
                infoPower(btd->bus, msg);
                enqueueInfoMessage(msg);
            }
            else if (!queue_GA_isempty(btd->bus)) {
                ga_state_t gatmp;
                dequeueNextGA(btd->bus, &gatmp);
                addr = gatmp.id - 1;
                ln_packetlen = 4;
                ln_packet[0] = OPC_SW_REQ;

                ln_packet[1] = (unsigned short int) (addr & 0x0007f);
                ln_packet[2] = (unsigned short int) ((addr >> 7) & 0x000f);
                ln_packet[2] |=
                    (unsigned short int) ((gatmp.port & 0x0001) << 5);
                ln_packet[2] |=
                    (unsigned short int) ((gatmp.action & 0x0001) << 4);

                if (gatmp.action == 1) {
                    gettimeofday(&gatmp.tv[gatmp.port], NULL);
                }
                setGA(btd->bus, gatmp.id, gatmp);
                syslog_bus(btd->bus, DBG_DEBUG, "Loconet: GA SET #%d %02X",
                           gatmp.id, gatmp.action);
            }

            ln_packet[ln_packetlen - 1] =
                ln_checksum(ln_packet, ln_packetlen - 1);
            if (ln_packet[0] != OPC_IDLE) {
                ln_write(btd->bus, ln_packet, ln_packetlen);
                timeoutcnt = 0;
            }
        }
        else {
            syslog_bus(btd->bus, DBG_DEBUG,
                       "Waiting for echo of last command (%d ms timeoutcount)",
                       timeoutcnt);
            usleep(100000);
            timeoutcnt++;
            if (timeoutcnt > 10) {
                syslog_bus(btd->bus, DBG_DEBUG,
                           "time out for reading echo, giving up");
                __loconett->ln_msglen = 0;
            }
        }
        usleep(1000);
    }
    /*run the cleanup routine */
    pthread_cleanup_pop(1);
    return NULL;
}
