/* +----------------------------------------------------------------------+ */
/* | DDL - Digital Direct for Linux                                       | */
/* +----------------------------------------------------------------------+ */
/* | Copyright (c) 2002 - 2003 Vogt IT                                    | */
/* +----------------------------------------------------------------------+ */
/* | This source file is subject of the GNU general public license 2,     | */
/* | that is bundled with this package in the file COPYING, and is        | */
/* | available at through the world-wide-web at                           | */
/* | http://www.gnu.org/licenses/gpl.txt                                  | */
/* | If you did not receive a copy of the PHP license and are unable to   | */
/* | obtain it through the world-wide-web, please send a note to          | */
/* | gpl-license@vogt-it.com so we can mail you a copy immediately.       | */
/* +----------------------------------------------------------------------+ */
/* | Authors:   Torsten Vogt vogt@vogt-it.com                             | */
/* |                                                                      | */
/* +----------------------------------------------------------------------+ */

/***************************************************************/
/* erddcd - Electric Railroad Direct Digital Command Daemon    */
/*    generates without any other hardware digital commands    */
/*    to control electric model railroads                      */
/*                                                             */
/* file: nmra.c                                                */
/* job : implements routines to compute data for the           */
/*       NMRA-DCC protocol and send this data to               */
/*       the serial device.                                    */
/*                                                             */
/* Torsten Vogt, may 1999                                      */
/*                                                             */
/* last changes: Torsten Vogt, september 2000                  */
/*               Torsten Vogt, january 2001                    */
/*               Torsten Vogt, april 2001                      */
/*                                                             */
/***************************************************************/

/**********************************************************************
 data format:

 look at the srcp specification

 protocol formats:

 NB: NMRA baseline decoder
 (implemented)

 N1: NMRA 4-function decoder, 7-bit address, 28 speed steps
 (implemented)

 N2: NMRA 4-function decoder, 7-bit address, 128 speed steps
 (implemented)

 N3: NMRA 4-function decoder, 14-bit address, 28 speed steps
 (implemented)

 N4: NMRA 4-function decoder, 14-bit address, 128 speed steps
 (implemented)

 NA: accessory digital decoders
 (implemented)

 service mode instruction packets for direct mode
 (verify cv, write cv, cv bit manipulation)
 (implemented)

 service mode instruction packets for adress-only mode
 (verify address contents, write address contents)
 (NOT implemented)

 service mode instruction packets for physical register addressing
 (verify register contents, write register contents)
 (implemented)

 service mode instruction packets paged cv addressing
 (verify/write register/cv contents)
 (NOT implemented)

 general notes:

   configuration of the serial port:

      startbit: 1
      stopbit : 1
      databits: 8
      baudrate: 19200

      ==> one serial bit takes 52.08 usec.

      ==> NMRA-0-Bit: 01         (52 usec low and 52 usec high)
          NMRA-1-Bit: 0011       (at least 100 usec low and high)

      serial stream (only start/stop bits):

      0_______10_______10_______10_______10_______10_______10___ ...

      problem: how to place the NMRA-0- and NMRA-1-Bits in the serial stream

      examples:

      0          0xF0     _____-----
      00         0xC6     __--___---
      01         0x78     ____----_-
      10         0xE1     _-____----
      001        0x66     __--__--_-
      010        0x96     __--_-__--
      011        0x5C     ___---_-_-
      100        0x99     _-__--__--
      101        0x71     _-___---_-
      110        0xC5     _-_-___---
      0111       0x56     __--_-_-_-
      1011       0x59     _-__--_-_-
      1101       0x65     _-_-__--_-
      1110       0x95     _-_-_-__--
      11111      0x55     _-_-_-_-_-
                          ^        ^
                          start-   stop-
                          bit      bit

   known bugs (of version 1 of the nmra dcc translation routine):
   (i hope version 2 don't have these bugs ;-) )

      following packets are not translateable:

        N1 031 1 06 0 0 0 0 0
        N1 047 0 07 0 0 0 0 0

        N2 031 0 091 0 0 0 0 0
        N2 031 1 085 0 0 0 0 0
        N2 031 1 095 0 0 0 0 0
        N2 047 0 107 0 0 0 0 0
        N2 047 1 103 0 0 0 0 0
        N2 047 1 111 0 0 0 0 0
        N2 048 1 112 0 0 0 0 0
        N2 051 1 115 0 0 0 0 0
        N2 053 1 117 0 0 0 0 0
        N2 056 0 124 0 0 0 0 0
        N2 057 1 113 0 0 0 0 0
        N2 058 1 114 0 0 0 0 0
        N2 059 1 115 0 0 0 0 0
        N2 060 1 116 0 0 0 0 0
        N2 061 1 117 0 0 0 0 0
        N2 062 1 118 0 0 0 0 0

     I think, that these are not really problems. The only consequence is
     e.g. that some addresses has 127 speed steps instead of 128. Thats
     life, don't worry.

     New: completely new algorithm to generate the nmra packet stream
     (i call it 'version 3' of the translate routines)

     The idea in this approach to generate nmra patterns is, to split the
     starting and ending bit in each pattern and share it with the next
     pattern. Therefore the patterns, which could be generated, are coded with
     h, H, l and L, lowercase describing the half bits. The longest possible
     pattern contains two half h bits and four H bits. For the access into
     the coding table, the index of course doesn't differentiate between half
     and full bits, because the first one is always half and the last one
     determined by this table. This table shows, which bit pattern will be
     replaced by which pattern on the serial line. There is only one pattern
     left, which could not be directly translated. This pattern starts with
     an h, so we have to look at the patterns, which end with an h, if we want
     to avoid this pattern. All of the patterns, we access in the first try,
     contain an l or an L, which could be enlarged, to get rid of at least on
     bit in this pattern and don't have our problem in the next pattern. With
     the only exception of hHHHHh, but this pattern simply moves our problem
     into one byte before or upto the beginning of the sequence. And there, we
     could always add a bit. So we are sure, to be able, to translate any
     given sequence of bits.

    Because only the case of hHHHHh realy requires 6 bits, the translationtable
    could be left at 32 entries. The other case, which has the same first five
    bits is our problem, so we have to handle it seperatly anyway.

    Of course the resulting sequence is not dc free. But if this is required,
    one could replace the bytes in the TranslateData by an index in another
    table which holds for each data containing at least an L or two l
    replacements with different dc components. This way one could get a dc
    free signal or at will a signal with a given dc part.

    #define ll          0xf0  _____-----

    #define lLl         0xcc  ___--__---    000000  000001  000010 000011 000100 000101 000110  000111

    #define lHl         0xe8  ____-_----

    #define lHLl        0x9a  __-_--__--    010000  010001  010010  010011

    #define lLHl        0xa6  __--__-_--    001000  001001  001010  001011

    #define lHHl        0xd4  ___-_-_---    011000  011001  011010  011011

    #define lHHHl       0xaa  __-_-_-_--



    #define lh          0x00  _________-

    #define lLh         0x1c  ___---___-

    #define lHh         0x40  _______-_-

    #define lLHh        0x4c  ___--__-_-    001100  001101  001110  001111

    #define lHLh        0x34  ___-_--__-    010100  010101  010110  010111

    #define lHHh        0x50  _____-_-_-    011100  011101

    #define lHHHh       0x54  ___-_-_-_-    011110  011111



    #define hLh         0x0f  _----____-

    #define hHLh        0x1d  _-_---___-    110100  110101

    #define hLHh        0x47  _---___-_-    101100  101101

    #define hHHLh       0x35  _-_-_--__-    111010  111011

    #define hHLHh       0x4d  _-_--__-_-    110110  110111

    #define hLHHh       0x53  _--__-_-_-      101110  101111

    #define hHHHHh      0x55  _-_-_-_-_-    111111



    #define hl          0xff  _---------

    #define hLl         0xc7  _---___---    100000  100001  100010  100011  100100  100101  100110  100111

    #define hHl         0xfd  _-_-------

    #define hHLl        0xcd  _-_--__---    110000  110001  110010  110011

    #define hLHl        0xD3        _--__-_---      101000  101001  101010  101011

    #define hHHl        0xF5        _-_-_-----      111000  111001

    #define hHHHl       0xd5  _-_-_-_---      111100  111101

    not directly translatetable     111110

****************************************************************/

#define ll          0xf0
#define lLl         0xcc
#define lHl         0xe8
#define lHLl        0x9a
#define lLHl        0xa6
#define lHHl        0xd4
#define lHHHl       0xaa
#define lh          0x00
#define lLh         0x1c
#define lHh         0x40
#define lLHh        0x4c
#define lHLh        0x34
#define lHHh        0x50
#define lHHHh       0x54
#define hLh         0x0f
#define hHLh        0x1d
#define hLHh        0x47
#define hHHLh       0x35
#define hHLHh       0x4d
#define hLHHh       0x53
#define hHHHHh      0x55
#define hl          0xff
#define hLl         0xc7
#define hHl         0xfd
#define hHLl        0xcd
#define hLHl        0xD3
#define hHHl        0xF5
#define hHHHl       0xd5


#include "stdincludes.h"
#include "ddl.h"
#include "ddl_nmra.h"

typedef struct {
    char *pattern;
    int patternlength;
    int value;
} tTranslateData;

typedef struct {
    int value;
    int patternlength;
} tTranslateData_v3;

static const tTranslateData TranslateData[] = {
    {"0", 1, 0xF0},
    {"00", 2, 0xC6},
    {"01", 2, 0x78},
    {"10", 2, 0xE1},
    {"001", 3, 0x66},
    {"010", 3, 0x96},
    {"011", 3, 0x5C},
    {"100", 3, 0x99},
    {"101", 3, 0x71},
    {"110", 3, 0xC5},
    {"0111", 4, 0x56},
    {"1011", 4, 0x59},
    {"1101", 4, 0x65},
    {"1110", 4, 0x95},
    {"11111", 5, 0x55}
};

// number of translatable patterns
static int DataCnt = sizeof(TranslateData) / sizeof(TranslateData[0]);

static const tTranslateData_v3 TranslateData_v3[32][2] = {
    {{lLl, 2}, {ll, 1}},
    {{lLl, 2}, {ll, 1}},
    {{lLl, 2}, {ll, 1}},
    {{lLl, 2}, {ll, 1}},
    {{lLHl, 3}, {lLh, 2}},
    {{lLHl, 3}, {lLh, 2}},
    {{lLHh, 3}, {lLh, 2}},
    {{lLHh, 3}, {lLh, 2}},
    {{lHLl, 3}, {lHl, 2}},
    {{lHLl, 3}, {lHl, 2}},
    {{lHLh, 3}, {lHl, 2}},
    {{lHLh, 3}, {lHl, 2}},
    {{lHHl, 3}, {lHh, 2}},
    {{lHHl, 3}, {lHh, 2}},
    {{lHHh, 3}, {lHh, 2}},
    {{lHHHh, 4}, {lHHh, 3}},
    {{hLl, 2}, {hl, 1}},
    {{hLl, 2}, {hl, 1}},
    {{hLl, 2}, {hl, 1}},
    {{hLl, 2}, {hl, 1}},
    {{hLHl, 3}, {hLh, 2}},
    {{hLHl, 3}, {hLh, 2}},
    {{hLHh, 3}, {hLh, 2}},
    {{hLHHh, 4}, {hLHh, 3}},
    {{hHLl, 3}, {hHl, 2}},
    {{hHLl, 3}, {hHl, 2}},
    {{hHLh, 3}, {hHl, 2}},
    {{hHLHh, 4}, {hHLh, 3}},
    {{hHHl, 3}, {hHHl, 3}},
    {{hHHLh, 4}, {hHHl, 3}},
    {{hHHHl, 4}, {hHHHl, 4}},
    {{hHHHHh, 5}, {hHHHHh, 5}}
};

static char *preamble = "111111111111111";
static const int NMRA_STACKSIZE = 200;
static const int BUFFERSIZE = 200;

int translateabel(char *bs)
{                               /* the result is only an index, no warranty */
    int i, size;
    char *pbs;
    size = strlen(bs);
    for (i = (DataCnt - 1); i >= 0; i--) {
        pbs = bs + (size - TranslateData[i].patternlength);
        if (strcmp(pbs, TranslateData[i].pattern) == 0)
            return 1;
    }
    return 0;
}

int read_next_six_bits(char *Bitstream)
{

    int i, bits = 0;
    for (i = 0; i < 6; i++)
        bits = (bits << 1) | (*Bitstream++ == '0' ? 0 : 1);
    return bits;
}

int translateBitstream2Packetstream_v1(char *Bitstream, char *Packetstream,
                                       int force_translation)
{

    char Buffer[BUFFERSIZE];
    char *pBs = Buffer;
    int i;                      /* decision of each recursion level          */
    int j = 0;                  /* index of Packetstream, level of recursion */
    int found;                  /* flag                                      */
    int stack[NMRA_STACKSIZE];  /* stack for the i's                         */
    int pstack = 0;             /* stack pointer                             */
    int correction = 0;
    int bufsize = 0;
    int highest_level = 0;      /* highest recursion level reached during algo.  */
    const int max_level_delta = 7;  /* additional recursion base, speeds up */

    pBs = strncpy(Buffer, Bitstream, BUFFERSIZE - 1);
    memset(Packetstream, 0, PKTSIZE);
    i = DataCnt - 1;
    if (!translateabel(Buffer)) {
        pBs[strlen(pBs) - 1] = 0;     /* The last bit of the bitstream is always '1'. */
        correction = 1;
    }
    bufsize = strlen(Buffer);
    while (*pBs) {
        found = 0;
        while (i >= 0) {
            if (strncmp(pBs, TranslateData[i].pattern,
                        TranslateData[i].patternlength) == 0) {
                found = 1;
                break;
            }
            i--;
        }
        if (!found) {           /* now backtracking    */
            pstack--;           /* back to last level  */
            if (pstack >= 0) {  /* last level avail.?  */
                i = stack[pstack];
                pBs -= TranslateData[i].patternlength;  /* corrections */
                j--;
                if (((highest_level - j) >= max_level_delta) &&
                    (!force_translation))
                    j = 0;
                i--;            /* next try            */
            }
        }
        else {
            Packetstream[j] = (unsigned char) TranslateData[i].value;
            j++;
            if (j > highest_level)
                highest_level = j;
            pBs += TranslateData[i].patternlength;
            stack[pstack] = i;
            pstack++;
            i = DataCnt - 1;
        }
        if (j >= PKTSIZE - 1 || bufsize == BUFFERSIZE) {
            syslog(LOG_INFO,
                   "cannot translate bitstream '%s' to NMRA packet",
                   Bitstream);
            return 0;
        }
        if (j <= 0 || pstack < 0 || pstack > NMRA_STACKSIZE - 1) {
            /* it's nasty, but a try: */
            strcat(Buffer, "1");        /* leading 1's don't make problems */
            bufsize++;
            pBs = Buffer;
            j = 0;
            pstack = 0;
            i = DataCnt - 1;
            correction = 0;     /* correction also done               */
            memset(Packetstream, 0, PKTSIZE);
        }
    }
    if (correction) {
        Packetstream[j] = (unsigned char) 0x99; /* Now the handling of the */
        j++;                    /* final '1'. See above.          */
    }
    return j + 1;               /* return nr of bytes in packetstream */
}

int translateBitstream2Packetstream_v2(char *Bitstream, char *Packetstream)
{

    int i = DataCnt - 1;        /* decision of each recursion level           */
    int j = 0;                  /* index of Packetstream, level of recursion  */
    int found;                  /* flag                                       */
    int stack[PKTSIZE];         /* stack for the i's                          */

    memset(Packetstream, 0, PKTSIZE);
    while (*Bitstream) {
        /* Check if the end of the buffer contains only 1s */
        if (strlen(Bitstream) <= 5) {
            if (strncmp(Bitstream, "11111", strlen(Bitstream)) == 0) {
                // This is the end
                Packetstream[j++] = (unsigned char) 0x55;
                return j + 1;
            }
        }

        found = 0;
        while (i >= 0) {
            if (strncmp
                (Bitstream, TranslateData[i].pattern,
                 TranslateData[i].patternlength) == 0) {
                found = 1;
                break;
            }
            i--;
        }
        if (!found) {           /* now backtracking    */
            if (j > 0) {        /* last level avail.?  */
                j--;            /* go back             */
                i = stack[j];
                Bitstream -= TranslateData[i].patternlength;    /* corrections      */
                i--;

            }
            else {
                syslog(LOG_INFO,
                       "cannot translate bitstream '%s' to NMRA packet",
                       Bitstream);
                return 0;
            }
        }
        else {
            Packetstream[j] = (unsigned char) TranslateData[i].value;
            Bitstream += TranslateData[i].patternlength;
            stack[j] = i;
            j++;
            i = DataCnt - 1;
        }
        // Check buffer size
        if (j >= PKTSIZE) {
            syslog(LOG_INFO, "Oops buffer too small - bitstream '%s'",
                   Bitstream);
            return 0;
        }
    }
    return j + 1;               /* return nr of bytes in packetstream */
}

int translateBitstream2Packetstream_v3(char *Bitstream, char *Packetstream)
{

    /* this routine assumes, that any Bitstream starts with a 1 Bit. */
    /* This could be changed, if necessary */

    char Buffer[BUFFERSIZE + 20];       /* keep room for additional pre and postamble */
    char *read_ptr = Buffer + 1;        /* here the real sequence starts */

    char *restart_read = Buffer;        /* one more 1 in the beginning for successful restart */
    char *last_restart = Buffer - 1;    /* this necessary, only to verify our assumptions */

    char *buf_end;

    int restart_packet = 0;
    int generate_packet = 0;

    int second_try = FALSE;
    int act_six;

    read_ptr = strcpy(Buffer, "11");

    /* one bit, because we start with a half-bit, so we have to put in the left half */
    /* one bit, to be able, to back up one bit, if we run into a 111110 pattern */

    strncat(Buffer, Bitstream, BUFFERSIZE - 1);

    buf_end = Buffer + strlen(Buffer);  /* for simply testing, whether our job is done */

    strcat(Buffer, "111111");

    /* at most six trailing bits are possibly necessary */

    memset(Packetstream, 0, PKTSIZE);

    while (generate_packet < PKTSIZE && read_ptr < buf_end) {
        act_six = read_next_six_bits(read_ptr);
        if (act_six == 0x3e /* 111110 */ ) {
            /*did we reach an untranslateble value */
            /* try again from last position, where a shorter translation */
            /* could be choosen                                          */
            second_try = TRUE;
            generate_packet = restart_packet;
            if (restart_read == last_restart)
                syslog(LOG_INFO, "Sorry, restart algorithm doesn't "
                        "work as expected for NMRA-Packet %s", Bitstream);
            last_restart = restart_read;
            read_ptr = restart_read;
            act_six = read_next_six_bits(read_ptr);
        }

        Packetstream[generate_packet] =
            TranslateData_v3[act_six >> 1][second_try ? 1 : 0].value;

        if (act_six < 0x3e /* 111110 */ ) {
            /* is translation fixed upto here ? */
            restart_packet = generate_packet;
            restart_read = read_ptr;
        }
        read_ptr +=
            TranslateData_v3[act_six >> 1][second_try ? 1 : 0].
            patternlength;
        generate_packet++;
        second_try = FALSE;
    }

    return generate_packet;     /* return nr of bytes in packetstream */
}


int translateBitstream2Packetstream(int NMRADCC_TR_V, char *Bitstream,
                                    char *Packetstream,
                                    int force_translation)
{
    switch (NMRADCC_TR_V) {
        case 1:
            return translateBitstream2Packetstream_v1(Bitstream,
                                                      Packetstream,
                                                      force_translation);
        case 2:
            return translateBitstream2Packetstream_v2(Bitstream,
                                                      Packetstream);
        case 3:
            return translateBitstream2Packetstream_v3(Bitstream,
                                                      Packetstream);
        default:
            return 0;
    }
}

/*** some useful functions to calculate NMRA-DCC bytes (char arrays) ***/

void calc_7bit_address_byte(char *byte, int address)
{
    /* calculating address byte: 0AAAAAAA */

    int i, j;

    memset(byte, 0, 9);
    byte[0] = '0';
    for (i = 7; i > 0; i--) {
        j = address % 2;
        address = address / 2;
        switch (j) {
            case 0:
                byte[i] = '0';
                break;
            case 1:
                byte[i] = '1';
                break;
        }
    }
}

void calc_14bit_address_byte(char *byte1, char *byte2, int address)
{
    /* calculating address bytes: 11AAAAAA AAAAAAAA */

    int i, j;

    memset(byte1, 0, 9);
    memset(byte2, 0, 9);
    byte1[0] = '1';
    byte1[1] = '1';
    for (i = 13; i >= 0; i--) {
        j = address % 2;
        address = address / 2;
        if (i >= 6) {
            switch (j) {        // set bit 7 to bit 0 of addr-byte 2
                case 0:
                    byte2[i - 6] = '0';
                    break;
                case 1:
                    byte2[i - 6] = '1';
                    break;
            }
        }
        else {
            switch (j) {        // set bit 7 to bit 2 of addr-byte 1
                case 0:
                    byte1[2 + i] = '0';
                    break;
                case 1:
                    byte1[2 + i] = '1';
                    break;
            }
        }
    }
}

void calc_baseline_speed_byte(char *byte, int direction, int speed)
{
    /* calculating speed byte2: 01DUSSSS  */

    int i, j;

    memset(byte, 0, 9);
    byte[0] = '0';
    byte[1] = '1';
    byte[3] = '1';
    if (direction == 1)
        byte[2] = '1';
    else
        byte[2] = '0';

    for (i = 7; i > 3; i--) {
        j = speed % 2;
        speed = speed / 2;
        switch (j) {
            case 0:
                byte[i] = '0';
                break;
            case 1:
                byte[i] = '1';
                break;
        }
    }
}

void calc_28spst_speed_byte(char *byte, int direction, int speed)
{
    /* calculating speed byte: 01DSSSSS */

    int i, j;

    memset(byte, 0, 9);
    byte[0] = '0';
    byte[1] = '1';
    if (direction == 1)
        byte[2] = '1';
    else
        byte[2] = '0';
    if (speed > 1) {
        if (speed % 2 == 1) {
            byte[3] = '1';
            speed = (speed + 1) / 2;
        }
        else {
            byte[3] = '0';
            speed = (speed + 2) / 2;
        }
    }
    else {
        byte[3] = '0';
    }
    for (i = 7; i > 3; i--) {
        j = speed % 2;
        speed = speed / 2;
        switch (j) {
            case 0:
                byte[i] = '0';
                break;
            case 1:
                byte[i] = '1';
                break;
        }
    }
}

void calc_function_group_one_byte(char *byte, int f[5])
{
    /* calculating function byte: 100FFFFF */

    memset(byte, '0', 9);
    byte[0] = '1';
    byte[1] = '0';
    byte[2] = '0';
    if (f[0] == 1)
        byte[3] = '1';
    if (f[1] == 1)
        byte[7] = '1';
    if (f[2] == 1)
        byte[6] = '1';
    if (f[3] == 1)
        byte[5] = '1';
    if (f[4] == 1)
        byte[4] = '1';
    byte[8] = 0;
}

void calc_128spst_adv_op_bytes(char *byte1, char *byte2,
                               int direction, int speed)
{

    int i, j;

    memset(byte1, 0, 9);
    memset(byte2, 0, 9);
    strcpy(byte1, "00111111");
    if (direction == 1)
        byte2[0] = '1';
    else
        byte2[0] = '0';
    for (i = 7; i > 0; i--) {
        j = speed % 2;
        speed = speed / 2;
        switch (j) {
            case 0:
                byte2[i] = '0';
                break;
            case 1:
                byte2[i] = '1';
                break;
        }
    }
}

void calc_acc_address_byte(char *byte, char *rest, int address)
{
    /* calculating address byte: 10AAAAAA , returning rest */

    int i, j;
    char dummy[10];

    memset(dummy, 0, 10);
    for (i = 8; i >= 0; i--) {
        j = address % 2;
        address = address / 2;
        switch (j) {
            case 0:
                dummy[i] = '0';
                break;
            case 1:
                dummy[i] = '1';
                break;
        }
    }
    memset(byte, 0, 9);
    byte[0] = '1';
    byte[1] = '0';
    for (i = 8; i > 2; i--) {
        byte[i - 1] = dummy[i];
    }
    memset(rest, 0, 3);
    for (i = 2; i >= 0; i--)
        rest[i] = dummy[i];
}

void calc_acc_instr_byte(char *byte, char *rest, int activate, int pairnr,
                         int output)
{

    int i;

    memset(byte, 0, 9);
    if (output)
        byte[7] = '1';
    else
        byte[7] = '0';
    if (activate)
        byte[4] = '1';
    else
        byte[4] = '0';
    switch (pairnr) {
        case 0:
            byte[6] = '0';
            byte[5] = '0';
            break;
        case 1:
            byte[6] = '1';
            byte[5] = '0';
            break;
        case 2:
            byte[6] = '0';
            byte[5] = '1';
            break;
        case 3:
            byte[6] = '1';
            byte[5] = '1';
            break;
        default:
            byte[6] = '0';
            byte[5] = '0';
            break;
    }
    for (i = 3; i > 0; i--) {
        switch (rest[i - 1]) {
            case '0':
                byte[i] = '1';
                break;
            case '1':
                byte[i] = '0';
                break;
            default:
                byte[i] = '1';
                break;
        }
    }
    byte[0] = '1';
}

void xor_two_bytes(char *byte, char *byte1, char *byte2)
{

    int i;

    memset(byte, 0, 9);
    for (i = 0; i < 8; i++) {
        if (byte1[i] == byte2[i])
            byte[i] = '0';
        else
            byte[i] = '1';
    }
}

/*** functions to generate NMRA-DCC data packets ***/

int comp_nmra_accessory(long int busnumber, int nr, int output,
                        int activate)
{
    /* command: NA <nr [0001-4096]> <outp [0,1]> <activate [0,1]>
       example: NA 0012 0 1  */

    char byte1[9];
    char byte2[9];
    char byte3[9];
    char bitstream[BUFFERSIZE];
    char packetstream[PKTSIZE];
    char *p_packetstream;

    int address = 0;            /* of the decoder                           */
    char rest[3];
    int pairnr = 0;             /* decoders have pair of outputs            */

    int j;

    DBG(busnumber, DBG_DEBUG,
        "command for NMRA protocol for accessory decoders (NA) received");

    /* no special error handling, it's job of the clients */
    if (nr < 1 || nr > 4096 || output < 0 || output > 1 ||
        activate < 0 || activate > 1)
        return 1;
#if 0                           // GA Packet Cache
    /* get the calculated packet if available */
    j = getNMRAGaPacket(nr, output, activate, &p_packetstream);
    if (j == 0) {
#endif
        /* packet is not available */
        p_packetstream = packetstream;

        /*calculate the real address of the decoder and the pairnr of the switch */
        address = ((nr - 1) / 4) + 1;   /* valid decoder addresses: 1..1023 */
        pairnr = (nr - 1) % 4;

        calc_acc_address_byte(byte1, rest, address);
        calc_acc_instr_byte(byte2, rest, activate, pairnr, output);
        xor_two_bytes(byte3, byte2, byte1);

        /* putting all together in a 'bitstream' (char array) */
        memset(bitstream, 0, 100);
        strcat(bitstream, preamble);
        strcat(bitstream, "0");
        strcat(bitstream, byte1);
        strcat(bitstream, "0");
        strcat(bitstream, byte2);
        strcat(bitstream, "0");
        strcat(bitstream, byte3);
        strcat(bitstream, "1");

        j = translateBitstream2Packetstream(busnumber, bitstream,
                                            packetstream, TRUE);
#if 0                           // GA Packet Cache
    }
#endif
    if (j > 0) {
        queue_add(busnumber, address, p_packetstream, QNBACCPKT, j);
#if 0                           // GA Packet Cache
        updateNMRAGaPacketPool(nr, output, activate, p_packetstream, j);
#endif
        return 0;
    }

    return 1;
}

int comp_nmra_baseline(long int busnumber, int address, int direction,
                       int speed)
{

    char byte1[9];
    char byte2[9];
    char byte3[9];
    char bitstream[BUFFERSIZE];
    char packetstream[PKTSIZE];

    int adr = 0;
    int j;

    DBG(busnumber, DBG_DEBUG,
        "command for NMRA protocol baseline (NB) received");

    adr = address;

    /* no special error handling, it's job of the clients */
    if (address < 1 || address > 99 || direction < 0 || direction > 1 ||
        speed < 0 || speed > 15)
        return 1;

    calc_7bit_address_byte(byte1, address);
    calc_baseline_speed_byte(byte2, direction, speed);
    xor_two_bytes(byte3, byte2, byte1);

    /* putting all together in a 'bitstream' (char array) */
    memset(bitstream, 0, 100);
    strcat(bitstream, preamble);
    strcat(bitstream, "0");
    strcat(bitstream, byte1);
    strcat(bitstream, "0");
    strcat(bitstream, byte2);
    strcat(bitstream, "0");
    strcat(bitstream, byte3);
    strcat(bitstream, "1");

    j = translateBitstream2Packetstream(busnumber, bitstream, packetstream,
                                        FALSE);

    if (j > 0) {
        update_NMRAPacketPool(busnumber, adr, packetstream, j,
                              packetstream, j);
        queue_add(busnumber, adr, packetstream, QNBLOCOPKT, j);

        return 0;
    }

    return 1;
}

int comp_nmra_f4b7s28(long int busnumber, int address, int direction,
                      int speed, int func, int f1, int f2, int f3, int f4)
{
    /* 4-function-decoder with 7-bit address and 28 speed steps */
    /* N1 001 1 18 1 0 0 0 0                                    */

    char addrbyte[9];
    char spdrbyte[9];
    char funcbyte[9];
    char errdbyte[9];
    char bitstream[BUFFERSIZE];
    char bitstream2[BUFFERSIZE];
    char packetstream[PKTSIZE];
    char packetstream2[PKTSIZE];

    int adr = 0;
    int f[5];
    int i, j, jj;

    DBG(busnumber, DBG_DEBUG,
        "command for NMRA protocol 4f7b28fs (N1) received");

    adr = address;
    f[0] = func;
    f[1] = f1;
    f[2] = f2;
    f[3] = f3;
    f[4] = f4;

    /* no special error handling, it's job of the clients */
    if (address < 1 || address > 99 || direction < 0 || direction > 1 ||
        speed < 0 || speed > 28)
        return 1;
    for (i = 0; i < 5; i++)
        if (f[i] < 0 || f[i] > 1)
            return 1;

    calc_7bit_address_byte(addrbyte, address);
    calc_28spst_speed_byte(spdrbyte, direction, speed);
    calc_function_group_one_byte(funcbyte, f);
    xor_two_bytes(errdbyte, addrbyte, spdrbyte);

    /* putting all together in a 'bitstream' (char array) (speed & direction) */
    memset(bitstream, 0, 100);
    strcat(bitstream, preamble);
    strcat(bitstream, "0");
    strcat(bitstream, addrbyte);
    strcat(bitstream, "0");
    strcat(bitstream, spdrbyte);
    strcat(bitstream, "0");
    strcat(bitstream, errdbyte);
    strcat(bitstream, "1");

    xor_two_bytes(errdbyte, addrbyte, funcbyte);

    /* putting all together in a 'bitstream' (char array) (functions) */
    memset(bitstream2, 0, 100);
    strcat(bitstream2, preamble);
    strcat(bitstream2, "0");
    strcat(bitstream2, addrbyte);
    strcat(bitstream2, "0");
    strcat(bitstream2, funcbyte);
    strcat(bitstream2, "0");
    strcat(bitstream2, errdbyte);
    strcat(bitstream2, "1");

    j = translateBitstream2Packetstream(busnumber, bitstream, packetstream,
                                        FALSE);
    jj = translateBitstream2Packetstream(busnumber, bitstream2,
                                         packetstream2, FALSE);

    if (j > 0 && jj > 0) {
        update_NMRAPacketPool(busnumber, adr, packetstream, j,
                              packetstream2, jj);
        queue_add(busnumber, adr, packetstream, QNBLOCOPKT, j);
        queue_add(busnumber, adr, packetstream2, QNBLOCOPKT, jj);

        return 0;
    }

    return 1;
}

int comp_nmra_f4b7s128(long int busnumber, int address, int direction,
                       int speed, int func, int f1, int f2, int f3, int f4)
{
    /* 4-function-decoder with 7-bit address and 128 speed steps */
    /* N2 001 1 057 1 0 0 0 0                                    */

    char addrbyte[9];
    char spdrbyte1[9];
    char spdrbyte2[9];
    char funcbyte[9];
    char errdbyte[9];
    char dummy[9];
    char bitstream[BUFFERSIZE];
    char bitstream2[BUFFERSIZE];
    char packetstream[PKTSIZE];
    char packetstream2[PKTSIZE];

    int adr = 0;
    int f[5];
    int i, j, jj;

    DBG(busnumber, DBG_DEBUG,
        "command for NMRA protocol 4f7b128fs (N2) received");

    adr = address;
    f[0] = func;
    f[1] = f1;
    f[2] = f2;
    f[3] = f3;
    f[4] = f4;

    /* no special error handling, it's job of the clients */
    if (address < 1 || address > 99 || direction < 0 || direction > 1 ||
        speed < 0 || speed > 128)
        return 1;
    for (i = 0; i < 5; i++)
        if (f[i] < 0 || f[i] > 1)
            return 1;

    calc_7bit_address_byte(addrbyte, address);
    calc_128spst_adv_op_bytes(spdrbyte1, spdrbyte2, direction, speed);
    calc_function_group_one_byte(funcbyte, f);
    xor_two_bytes(dummy, addrbyte, spdrbyte1);
    xor_two_bytes(errdbyte, dummy, spdrbyte2);

    /* putting all together in a 'bitstream' (char array) (speed & direction) */
    memset(bitstream, 0, 100);
    strcat(bitstream, preamble);
    strcat(bitstream, "0");
    strcat(bitstream, addrbyte);
    strcat(bitstream, "0");
    strcat(bitstream, spdrbyte1);
    strcat(bitstream, "0");
    strcat(bitstream, spdrbyte2);
    strcat(bitstream, "0");
    strcat(bitstream, errdbyte);
    strcat(bitstream, "1");

    xor_two_bytes(errdbyte, addrbyte, funcbyte);

    /* putting all together in a 'bitstream' (char array) (functions) */
    memset(bitstream2, 0, 100);
    strcat(bitstream2, preamble);
    strcat(bitstream2, "0");
    strcat(bitstream2, addrbyte);
    strcat(bitstream2, "0");
    strcat(bitstream2, funcbyte);
    strcat(bitstream2, "0");
    strcat(bitstream2, errdbyte);
    strcat(bitstream2, "1");

    j = translateBitstream2Packetstream(busnumber, bitstream, packetstream,
                                        FALSE);
    jj = translateBitstream2Packetstream(busnumber, bitstream2,
                                         packetstream2, FALSE);

    if (j > 0 && jj > 0) {
        update_NMRAPacketPool(busnumber, adr, packetstream, j,
                              packetstream2, jj);
        queue_add(busnumber, adr, packetstream, QNBLOCOPKT, j);
        queue_add(busnumber, adr, packetstream2, QNBLOCOPKT, jj);

        return 0;
    }

    return 1;
}

int comp_nmra_f4b14s28(long int busnumber, int address, int direction,
                       int speed, int func, int f1, int f2, int f3, int f4)
{
    /* 4-function-decoder with 14-bit address and 28 speed steps */
    /* N3 0001 1 18 1 0 0 0 0                                    */

    char addrbyte1[9];
    char addrbyte2[9];
    char spdrbyte[9];
    char funcbyte[9];
    char errdbyte[9];
    char dummy[9];
    char bitstream[BUFFERSIZE];
    char bitstream2[BUFFERSIZE];
    char packetstream[PKTSIZE];
    char packetstream2[PKTSIZE];

    int adr = 0;
    int f[5];
    int i, j, jj;

    DBG(busnumber, DBG_DEBUG,
        "command for NMRA protocol 4f14b28fs (N3) received");

    adr = address;
    f[0] = func;
    f[1] = f1;
    f[2] = f2;
    f[3] = f3;
    f[4] = f4;

    /* no special error handling, it's job of the clients */
    if (address < 1 || address > 10239 || direction < 0 || direction > 1 ||
        speed < 0 || speed > 28)
        return 1;
    for (i = 0; i < 5; i++)
        if (f[i] < 0 || f[i] > 1)
            return 1;

    calc_14bit_address_byte(addrbyte1, addrbyte2, address);
    calc_28spst_speed_byte(spdrbyte, direction, speed);
    calc_function_group_one_byte(funcbyte, f);

    xor_two_bytes(dummy, addrbyte1, addrbyte2);
    xor_two_bytes(errdbyte, dummy, spdrbyte);

    /* putting all together in a 'bitstream' (char array) (speed & direction) */
    memset(bitstream, 0, 100);
    strcat(bitstream, preamble);
    strcat(bitstream, "0");
    strcat(bitstream, addrbyte1);
    strcat(bitstream, "0");
    strcat(bitstream, addrbyte2);
    strcat(bitstream, "0");
    strcat(bitstream, spdrbyte);
    strcat(bitstream, "0");
    strcat(bitstream, errdbyte);
    strcat(bitstream, "1");

    xor_two_bytes(dummy, addrbyte1, addrbyte2);
    xor_two_bytes(errdbyte, dummy, funcbyte);

    /* putting all together in a 'bitstream' (char array) (functions) */
    memset(bitstream2, 0, 100);
    strcat(bitstream2, preamble);
    strcat(bitstream2, "0");
    strcat(bitstream2, addrbyte1);
    strcat(bitstream2, "0");
    strcat(bitstream2, addrbyte2);
    strcat(bitstream2, "0");
    strcat(bitstream2, funcbyte);
    strcat(bitstream2, "0");
    strcat(bitstream2, errdbyte);
    strcat(bitstream2, "1");

    j = translateBitstream2Packetstream(busnumber, bitstream, packetstream,
                                        FALSE);
    jj = translateBitstream2Packetstream(busnumber, bitstream2,
                                         packetstream2, FALSE);

    if (j > 0 && jj > 0) {
        update_NMRAPacketPool(busnumber, adr + ADDR14BIT_OFFSET,
                              packetstream, j, packetstream2, jj);
        queue_add(busnumber, adr + ADDR14BIT_OFFSET, packetstream,
                  QNBLOCOPKT, j);
        queue_add(busnumber, adr + ADDR14BIT_OFFSET, packetstream2,
                  QNBLOCOPKT, jj);

        return 0;
    }

    return 1;
}

int comp_nmra_f4b14s128(long int busnumber, int address, int direction,
                        int speed, int func, int f1, int f2, int f3,
                        int f4)
{
    /* 4-function-decoder with 14-bit address and 128 speed steps */
    /* N4 001 1 057 1 0 0 0 0                                    */

    char addrbyte1[9];
    char addrbyte2[9];
    char spdrbyte1[9];
    char spdrbyte2[9];
    char funcbyte[9];
    char errdbyte[9];
    char dummy[9];
    char bitstream[BUFFERSIZE];
    char bitstream2[BUFFERSIZE];
    char packetstream[PKTSIZE];
    char packetstream2[PKTSIZE];

    int adr = 0;
    int f[5];
    int i, j, jj;

    DBG(busnumber, DBG_DEBUG,
        "command for NMRA protocol 4f14b128fs (N4) received");

    adr = address;
    f[0] = func;
    f[1] = f1;
    f[2] = f2;
    f[3] = f3;
    f[4] = f4;

    /* no special error handling, it's job of the clients */
    if (address < 1 || address > 10239 || direction < 0 || direction > 1 ||
        speed < 0 || speed > 128)
        return 1;
    for (i = 0; i < 5; i++)
        if (f[i] < 0 || f[i] > 1)
            return 1;

    calc_14bit_address_byte(addrbyte1, addrbyte2, address);
    calc_128spst_adv_op_bytes(spdrbyte1, spdrbyte2, direction, speed);
    calc_function_group_one_byte(funcbyte, f);
    xor_two_bytes(errdbyte, addrbyte1, addrbyte2);
    xor_two_bytes(dummy, errdbyte, spdrbyte1);
    xor_two_bytes(errdbyte, dummy, spdrbyte2);

    /* putting all together in a 'bitstream' (char array) (speed & direction) */
    memset(bitstream, 0, 100);
    strcat(bitstream, preamble);
    strcat(bitstream, "0");
    strcat(bitstream, addrbyte1);
    strcat(bitstream, "0");
    strcat(bitstream, addrbyte2);
    strcat(bitstream, "0");
    strcat(bitstream, spdrbyte1);
    strcat(bitstream, "0");
    strcat(bitstream, spdrbyte2);
    strcat(bitstream, "0");
    strcat(bitstream, errdbyte);
    strcat(bitstream, "1");

    xor_two_bytes(dummy, addrbyte1, addrbyte2);
    xor_two_bytes(errdbyte, dummy, funcbyte);

    /* putting all together in a 'bitstream' (char array) (functions) */
    memset(bitstream2, 0, 100);
    strcat(bitstream2, preamble);
    strcat(bitstream2, "0");
    strcat(bitstream2, addrbyte1);
    strcat(bitstream2, "0");
    strcat(bitstream2, addrbyte2);
    strcat(bitstream2, "0");
    strcat(bitstream2, funcbyte);
    strcat(bitstream2, "0");
    strcat(bitstream2, errdbyte);
    strcat(bitstream2, "1");

    j = translateBitstream2Packetstream(busnumber, bitstream, packetstream,
                                        FALSE);
    jj = translateBitstream2Packetstream(busnumber, bitstream2,
                                         packetstream2, FALSE);

    if (j > 0 && jj > 0) {
        update_NMRAPacketPool(busnumber, adr + ADDR14BIT_OFFSET,
                              packetstream, j, packetstream2, jj);
        queue_add(busnumber, adr + ADDR14BIT_OFFSET, packetstream,
                  QNBLOCOPKT, j);
        queue_add(busnumber, adr + ADDR14BIT_OFFSET, packetstream2,
                  QNBLOCOPKT, jj);

        return 0;
    }

    return 1;
}

/**
 * The following function(s) supports the implementation of NMRA-
 * programmers. It is recommended to use a programming track to  
 * programm your locos. In every case it is useful to stop the   
 * refresh-cycle on the track when using one of the following    
 * servicemode functions.
 **/

static int sm_initialized = FALSE;

static char resetstream[PKTSIZE];
static int rs_size = 0;
static char idlestream[PKTSIZE];
static int is_size = 0;
static char pagepresetstream[PKTSIZE];
static int ps_size = 0;

static char *longpreamble = "111111111111111111111111111111";
static char reset_packet[] =
    "11111111111111111111111111111100000000000000000000000000010";
static char page_preset_packet[] =
    "11111111111111111111111111111100111110100000000100111110010";
static char idle_packet[] =
    "11111111111111111111111111111101111111100000000001111111110";

void sm_init(long int busnumber)
{
    memset(resetstream, 0, PKTSIZE);
    rs_size =
        translateBitstream2Packetstream(busnumber, reset_packet,
                                        resetstream, FALSE);
    memset(idlestream, 0, PKTSIZE);
    is_size =
        translateBitstream2Packetstream(busnumber, idle_packet, idlestream,
                                        FALSE);
    memset(pagepresetstream, 0, PKTSIZE);
    ps_size =
        translateBitstream2Packetstream(busnumber, page_preset_packet,
                                        pagepresetstream, TRUE);
    sm_initialized = TRUE;
}

int scanACK(long int busnumber)
{
    int result, arg;
    result = ioctl(busses[busnumber].fd, TIOCMGET, &arg);
    if ((result >= 0) && (!(arg & TIOCM_RI)))
        return 1;
    return 0;
}

int waitUARTempty_scanACK(long int busnumber)
{
    int result;
    int ack = 0;
    do {                        /* wait until UART is empty */
        if (scanACK(busnumber))
            ack = 1;            /* scan ACK */
#if linux
        ioctl(busses[busnumber].fd, TIOCSERGETLSR, &result);
#else
        ioctl(busses[busnumber].fd, TCSADRAIN, &result);
#endif
    } while (!result);
    return ack;
}

void handleACK(long int busnumber, int sckt, int ack)
{

    char buf[80];

    //set_SerialLine(SL_RI,ON);
    usleep(1000);
    if ((ack == 1) && (scanACK(busnumber) == 1))
        sprintf(buf, "INFO GL SM 2\n"); // ack not supported
    else
        sprintf(buf, "INFO GL SM %d\n", ack);   // ack supported ==> send to client

    write(sckt, buf, strlen(buf));
}

void protocol_nmra_sm_direct_cvbyte(long int busnumber, int sckt, int cv,
                                    int value, int verify)
{
    /* direct cv access */

    char byte2[9];
    char byte3[9];
    char byte4[9];
    char byte5[9];
    char bitstream[100];
    char packetstream[PKTSIZE];
    char SendStream[2048];

    int i, j, l, ack;

    DBG(busnumber, DBG_DEBUG,
        "command for NMRA service mode instruction (SMDWY) received");

    /* no special error handling, it's job of the clients */
    if (cv < 0 || cv > 1024 || value < 0 || value > 255)
        return;

    if (!sm_initialized)
        sm_init(busnumber);

    /* calculating byte3: AAAAAAAA (rest of CV#) */
    memset(byte3, 0, 9);
    for (i = 7; i >= 0; i--) {
        j = cv % 2;
        cv = cv / 2;
        switch (j) {
            case 0:
                byte3[i] = '0';
                break;
            case 1:
                byte3[i] = '1';
                break;
        }
    }

    /* calculating byte2: 011111AA (instruction byte1) */
    memset(byte2, 0, 9);
    if (verify)
        strcpy(byte2, "01110100");
    else
        strcpy(byte2, "01111100");
    for (i = 7; i >= 6; i--) {
        j = cv % 2;
        cv = cv / 2;
        switch (j) {
            case 0:
                byte2[i] = '0';
                break;
            case 1:
                byte2[i] = '1';
                break;
        }
    }

    /* calculating byte4: DDDDDDDD (data) */
    memset(byte4, 0, 9);
    for (i = 7; i >= 0; i--) {
        j = value % 2;
        value = value / 2;
        switch (j) {
            case 0:
                byte4[i] = '0';
                break;
            case 1:
                byte4[i] = '1';
                break;
        }
    }

    /* calculating byte5: EEEEEEEE (error detection byte) */
    memset(byte5, 0, 9);
    for (i = 0; i < 8; i++) {
        if (byte2[i] == byte3[i])
            byte5[i] = '0';
        else
            byte5[i] = '1';
        if (byte4[i] == byte5[i])
            byte5[i] = '0';
        else
            byte5[i] = '1';
    }

    /* putting all together in a 'bitstream' (char array) */
    memset(bitstream, 0, 100);
    strcat(bitstream, longpreamble);
    strcat(bitstream, "0");
    strcat(bitstream, byte2);
    strcat(bitstream, "0");
    strcat(bitstream, byte3);
    strcat(bitstream, "0");
    strcat(bitstream, byte4);
    strcat(bitstream, "0");
    strcat(bitstream, byte5);
    strcat(bitstream, "1");

    j = translateBitstream2Packetstream(busnumber, bitstream, packetstream,
                                        FALSE);

    memset(SendStream, 0, 2048);

    if (!verify) {
        for (l = 0; l < 50; l++)
            strcat(SendStream, idlestream);
        for (l = 0; l < 15; l++)
            strcat(SendStream, resetstream);
        for (l = 0; l < 20; l++)
            strcat(SendStream, packetstream);
        l = 50 * is_size + 15 * rs_size + 20 * j;
    }
    else {
        for (l = 0; l < 15; l++)
            strcat(SendStream, idlestream);
        for (l = 0; l < 5; l++)
            strcat(SendStream, resetstream);
        for (l = 0; l < 11; l++)
            strcat(SendStream, packetstream);
        l = 15 * is_size + 5 * rs_size + 11 * j;
    }

    setSerialMode(busnumber, SDM_NMRA);
    tcflow(busses[busnumber].fd, TCOON);
    write(busses[busnumber].fd, SendStream, l);
    ack = waitUARTempty_scanACK(busnumber);
    tcflow(busses[busnumber].fd, TCOOFF);
    setSerialMode(busnumber, SDM_DEFAULT);

    handleACK(busnumber, sckt, ack);
}

void protocol_nmra_sm_write_cvbyte(long int bus, int sckt, int cv,
                                   int value)
{
    protocol_nmra_sm_direct_cvbyte(bus, sckt, cv, value, FALSE);
}

void protocol_nmra_sm_verify_cvbyte(long int bus, int sckt, int cv,
                                    int value)
{
    protocol_nmra_sm_direct_cvbyte(bus, sckt, cv, value, TRUE);
}

void protocol_nmra_sm_write_cvbit(long int bus, int sckt, int cv, int bit,
                                  int value)
{
    /* direct cv access */

    char byte2[9];
    char byte3[9];
    char byte4[9];
    char byte5[9];
    char bitstream[100];
    char packetstream[PKTSIZE];
    char SendStream[2048];

    int i, j, l, ack;

    DBG(bus, DBG_DEBUG,
        "command for NMRA service mode instruction (SMDWB) received");

    /* no special error handling, it's job of the clients */
    if (cv < 0 || cv > 1023 || bit < 0 || bit > 7 || value < 0
        || value > 1)
        return;

    if (!sm_initialized)
        sm_init(bus);

    /* calculating byte3: AAAAAAAA (rest of CV#) */
    memset(byte3, 0, 9);
    for (i = 7; i >= 0; i--) {
        j = cv % 2;
        cv = cv / 2;
        switch (j) {
            case 0:
                byte3[i] = '0';
                break;
            case 1:
                byte3[i] = '1';
                break;
        }
    }

    /* calculating byte2: 011110AA (instruction byte1) */
    memset(byte2, 0, 9);
    strcpy(byte2, "01111000");
    for (i = 7; i >= 6; i--) {
        j = cv % 2;
        cv = cv / 2;
        switch (j) {
            case 0:
                byte2[i] = '0';
                break;
            case 1:
                byte2[i] = '1';
                break;
        }
    }

    /* calculating byte4: 111KDBBB (data) */
    memset(byte4, 0, 9);
    strcpy(byte4, "11110000");
    if (value == 1)
        byte4[4] = '1';
    else
        byte4[4] = '0';
    for (i = 7; i >= 5; i--) {
        j = bit % 2;
        bit = bit / 2;
        switch (j) {
            case 0:
                byte4[i] = '0';
                break;
            case 1:
                byte4[i] = '1';
                break;
        }
    }

    /* calculating byte5: EEEEEEEE (error detection byte) */
    memset(byte5, 0, 9);
    for (i = 0; i < 8; i++) {
        if (byte2[i] == byte3[i])
            byte5[i] = '0';
        else
            byte5[i] = '1';
        if (byte4[i] == byte5[i])
            byte5[i] = '0';
        else
            byte5[i] = '1';
    }

    /* putting all together in a 'bitstream' (char array) */
    memset(bitstream, 0, 100);
    strcat(bitstream, longpreamble);
    strcat(bitstream, "0");
    strcat(bitstream, byte2);
    strcat(bitstream, "0");
    strcat(bitstream, byte3);
    strcat(bitstream, "0");
    strcat(bitstream, byte4);
    strcat(bitstream, "0");
    strcat(bitstream, byte5);
    strcat(bitstream, "1");

    j = translateBitstream2Packetstream(bus, bitstream, packetstream,
                                        FALSE);

    memset(SendStream, 0, 2048);
    for (l = 0; l < 50; l++)
        strcat(SendStream, idlestream);
    for (l = 0; l < 15; l++)
        strcat(SendStream, resetstream);
    for (l = 0; l < 20; l++)
        strcat(SendStream, packetstream);
    l = 50 * is_size + 15 * rs_size + 20 * j;

    setSerialMode(bus, SDM_NMRA);
    tcflow(busses[bus].fd, TCOON);
    write(busses[bus].fd, SendStream, l);
    ack = waitUARTempty_scanACK(bus);
    tcflow(busses[bus].fd, TCOOFF);
    setSerialMode(bus, SDM_DEFAULT);

    handleACK(bus, sckt, ack);
}

void protocol_nmra_sm_phregister(long int bus, int sckt, int reg,
                                 int value, int verify)
{
    /* physical register addressing */

    char byte1[9];
    char byte2[9];
    char byte3[9];
    char bitstream[100];
    char packetstream[PKTSIZE];
    char SendStream[4096];

    int i, j, l, y, ack;

    DBG(bus, DBG_DEBUG,
        "command for NMRA service mode instruction (SMPRA) received");

    /* no special error handling, it's job of the clients */
    if (reg < 1 || reg > 8 || value < 0 || value > 255)
        return;

    if (!sm_initialized)
        sm_init(bus);

    /* calculating byte1: 0111CRRR (instruction and nr of register) */
    memset(byte1, 0, 9);
    if (verify)
        strcpy(byte1, "01110");
    else
        strcpy(byte1, "01111");
    switch (reg) {
        case 1:
            strcat(byte1, "000");
            break;
        case 2:
            strcat(byte1, "001");
            break;
        case 3:
            strcat(byte1, "010");
            break;
        case 4:
            strcat(byte1, "011");
            break;
        case 5:
            strcat(byte1, "100");
            break;
        case 6:
            strcat(byte1, "101");
            break;
        case 7:
            strcat(byte1, "110");
            break;
        case 8:
            strcat(byte1, "111");
            break;
    }

    /* calculating byte2: DDDDDDDD (data) */
    memset(byte2, 0, 9);
    for (i = 7; i >= 0; i--) {
        j = value % 2;
        value = value / 2;
        switch (j) {
            case 0:
                byte2[i] = '0';
                break;
            case 1:
                byte2[i] = '1';
                break;
        }
    }

    /* calculating byte3 (error detection byte) */
    xor_two_bytes(byte3, byte1, byte2);

    /* putting all together in a 'bitstream' (char array) */
    memset(bitstream, 0, 100);
    strcat(bitstream, longpreamble);
    strcat(bitstream, "0");
    strcat(bitstream, byte1);
    strcat(bitstream, "0");
    strcat(bitstream, byte2);
    strcat(bitstream, "0");
    strcat(bitstream, byte3);
    strcat(bitstream, "10");

    memset(packetstream, 0, PKTSIZE);
    j = translateBitstream2Packetstream(bus, bitstream, packetstream,
                                        TRUE);

    memset(SendStream, 0, 4096);

    if (!verify) {
        /* power-on cycle, at least 20 valid packets */
        for (l = 0; l < 50; l++)
            strcat(SendStream, idlestream);
        /* 3 or more reset packets */
        for (l = 0; l < 6; l++)
            strcat(SendStream, resetstream);
        /* 5 or more page preset packets */
        for (l = 0; l < 10; l++)
            strcat(SendStream, pagepresetstream);
        /* 6 or more reset packets */
        for (l = 0; l < 12; l++)
            strcat(SendStream, resetstream);
        /* 3 or more reset packets */
        for (l = 0; l < 12; l++)
            strcat(SendStream, resetstream);
        /* 5 or more write packets */
        for (l = 0; l < 20; l++)
            strcat(SendStream, packetstream);
        /* 6 or more reset or identical write packets */
        for (l = 0; l < 24; l++)
            strcat(SendStream, packetstream);
        /* 3 or more reset packets */
        for (l = 0; l < 24; l++)
            strcat(SendStream, resetstream);

        y = 50 * is_size + 54 * rs_size + 44 * j + 10 * ps_size;
    }
    else {
        /* power-on cycle, at least 20 valid packets */
        for (l = 0; l < 15; l++)
            strcat(SendStream, idlestream);
        /* 3 or more reset packets */
        for (l = 0; l < 5; l++)
            strcat(SendStream, resetstream);
        /* 7 or more verify packets */
        for (l = 0; l < 11; l++)
            strcat(SendStream, packetstream);
        y = 15 * is_size + 5 * rs_size + 11 * j + 0;
    }

    setSerialMode(bus, SDM_NMRA);
    tcflow(busses[bus].fd, TCOON);
    l = write(busses[bus].fd, SendStream, y);
    ack = waitUARTempty_scanACK(bus);
    tcflow(busses[bus].fd, TCOOFF);
    setSerialMode(bus, SDM_DEFAULT);

    handleACK(bus, sckt, ack);
}

void protocol_nmra_sm_write_phregister(long int bus, int sckt, int reg,
                                       int value)
{
    protocol_nmra_sm_phregister(bus, sckt, reg, value, FALSE);
}

void protocol_nmra_sm_verify_phregister(long int bus, int sckt, int reg,
                                        int value)
{
    protocol_nmra_sm_phregister(bus, sckt, reg, value, TRUE);
}
