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
/* file: maerklin.h                                            */
/* job : exports the functions from maerklin.c                 */
/*                                                             */
/* Torsten Vogt, january 1999                                  */
/*                                                             */
/* last changes: Torsten Vogt, march 2000                      */
/*               Torsten Vogt, january 2001                    */
/*                                                             */ 
/***************************************************************/

#ifndef __MAERKLIN_H__
#define __MAERKLIN_H__ 

#define LO (char)63
#define HI (char)0

#define LO_38K (char)55
#define HI_38K (char)4
#define OP_38K (char)52

#define WAIT_TRASH           1000
#define WAIT_BETWEEN_19K     3750        /* 3750 */
#define WAIT_END_19K         3750        /* 3750 */
#define WAIT_BETWEEN_38K     2570        /* 1875 */
#define WAIT_END_38K         2570        /* 1875 */  

/* signal generating functions for maerklin */
int comp_maerklin_1(int bus, int address, int direction, int speed, int func);
int comp_maerklin_2(int bus, int address, int direction, int speed, int func,
                    int f1, int f2, int f3, int f4);
int comp_maerklin_3(int bus, int address, int direction, int speed, int func,
                    int f1, int f2, int f3, int f4);
int comp_maerklin_4(int bus, int address, int direction, int speed, int func,
                    int f1, int f2, int f3, int f4);
int comp_maerklin_5(int bus, int address, int direction, int speed, int func,
                    int f1, int f2, int f3, int f4);
int comp_maerklin_ms(int bus, int address, int port, int action);
int comp_maerklin_mf(int bus, int address, 
                     int f1, int f2, int f3, int f4);

typedef struct _tMotorolaCodes {
   int  rAdr;
   int  bAdr;
   char Code[5];
} tMotorolaCodes;

static tMotorolaCodes MotorolaCodes[256] = {
    {   0, 85,"OOOO" },
    {   1,  3,"HLLL" },
    {   2,  1,"OLLL" },
    {   3, 12,"LHLL" },
    {   4, 15,"HHLL" },
    {   5, 13,"OHLL" },
    {   6,  4,"LOLL" },
    {   7,  7,"HOLL" },
    {   8,  5,"OOLL" },
    {   9, 48,"LLHL" },
    {  10, 51,"HLHL" },
    {  11, 49,"OLHL" },
    {  12, 60,"LHHL" },
    {  13, 63,"HHHL" },
    {  14, 61,"OHHL" },
    {  15, 52,"LOHL" },
    {  16, 55,"HOHL" },
    {  17, 53,"OOHL" },
    {  18, 16,"LLOL" },
    {  19, 19,"HLOL" },
    {  20, 17,"OLOL" },
    {  21, 28,"LHOL" },
    {  22, 31,"HHOL" },
    {  23, 29,"OHOL" },
    {  24, 20,"LOOL" },
    {  25, 23,"HOOL" },
    {  26, 21,"OOOL" },
    {  27,192,"LLLH" },
    {  28,195,"HLLH" },
    {  29,193,"OLLH" },
    {  30,204,"LHLH" },
    {  31,207,"HHLH" },
    {  32,205,"OHLH" },
    {  33,196,"LOLH" },
    {  34,199,"HOLH" },
    {  35,197,"OOLH" },
    {  36,240,"LLHH" },
    {  37,243,"HLHH" },
    {  38,241,"OLHH" },
    {  39,252,"LHHH" },
    {  40,255,"HHHH" },
    {  41,253,"OHHH" },
    {  42,244,"LOHH" },
    {  43,247,"HOHH" },
    {  44,245,"OOHH" },
    {  45,208,"LLOH" },
    {  46,211,"HLOH" },
    {  47,209,"OLOH" },
    {  48,220,"LHOH" },
    {  49,223,"HHOH" },
    {  50,221,"OHOH" },
    {  51,212,"LOOH" },
    {  52,215,"HOOH" },
    {  53,213,"OOOH" },
    {  54, 64,"LLLO" },
    {  55, 67,"HLLO" },
    {  56, 65,"OLLO" },
    {  57, 76,"LHLO" },
    {  58, 79,"HHLO" },
    {  59, 77,"OHLO" },
    {  60, 68,"LOLO" },
    {  61, 71,"HOLO" },
    {  62, 69,"OOLO" },
    {  63,112,"LLHO" },
    {  64,115,"HLHO" },
    {  65,113,"OLHO" },
    {  66,124,"LHHO" },
    {  67,127,"HHHO" },
    {  68,125,"OHHO" },
    {  69,116,"LOHO" },
    {  70,119,"HOHO" },
    {  71,117,"OOHO" },
    {  72, 80,"LLOO" },
    {  73, 83,"HLOO" },
    {  74, 81,"OLOO" },
    {  75, 92,"LHOO" },
    {  76, 95,"HHOO" },
    {  77, 93,"OHOO" },
    {  78, 84,"LOOO" },
    {  79, 87,"HOOO" },
    {  80,  0,"LLLL" },
    {  81,  2,"ULLL" },
    {  82,  6,"UOLL" },
    {  83,233,"OUUH" },
    {  84, 14,"UHLL" },
    {  85, 18,"ULOL" },
    {  86, 22,"UOOL" },
    {  87, 26,"UUOL" },
    {  88, 30,"UHOL" },
    {  89, 34,"ULUL" },
    {  90, 38,"UOUL" },
    {  91, 42,"UUUL" },
    {  92, 46,"UHUL" },
    {  93, 50,"ULHL" },
    {  94, 54,"UOHL" },
    {  95, 58,"UUHL" },
    {  96, 62,"UHHL" },
    {  97, 66,"ULLO" },
    {  98, 70,"UOLO" },
    {  99, 74,"UULO" },
    { 100, 78,"UHLO" },
    { 101, 82,"ULOO" },
    { 102, 86,"UOOO" },
    { 103, 90,"UUOO" },
    { 104, 94,"UHOO" },
    { 105, 98,"ULUO" },
    { 106,102,"UOUO" },
    { 107,106,"UUUO" },
    { 108,110,"UHUO" },
    { 109,114,"ULHO" },
    { 110,118,"UOHO" },
    { 111,122,"UUHO" },
    { 112,126,"UHHO" },
    { 113,130,"ULLU" },
    { 114,134,"UOLU" },
    { 115,138,"UULU" },
    { 116,142,"UHLU" },
    { 117,146,"ULOU" },
    { 118,150,"UOOU" },
    { 119,154,"UUOU" },
    { 120,158,"UHOU" },
    { 121,162,"ULUU" },
    { 122,166,"UOUU" },
    { 123,249,"OUHH" },
    { 124,174,"UHUU" },
    { 125,178,"ULHU" },
    { 126,182,"UOHU" },
    { 127,186,"UUHU" },
    { 128,190,"UHHU" },
    { 129,194,"ULLH" },
    { 130,198,"UOLH" },
    { 131,202,"UULH" },
    { 132,206,"UHLH" },
    { 133,210,"ULOH" },
    { 134,214,"UOOH" },
    { 135,218,"UUOH" },
    { 136,222,"UHOH" },
    { 137,226,"ULUH" },
    { 138,230,"UOUH" },
    { 139,234,"UUUH" },
    { 140,238,"UHUH" },
    { 141,242,"ULHH" },
    { 142,146,"UOHH" },
    { 143,250,"UUHH" },
    { 144,254,"UHHH" },
    { 145,  8,"LULL" },
    { 146, 24,"LUOL" },
    { 147, 40,"LUUL" },
    { 148, 56,"LUHL" },
    { 149, 72,"LULO" },
    { 150, 88,"LUOO" },
    { 151,104,"LUUO" },
    { 152,120,"LUHO" },
    { 153,136,"LULU" },
    { 154,152,"LUOU" },
    { 155,168,"LUUU" },
    { 156,184,"LUHU" },
    { 157,200,"LULH" },
    { 158,216,"LUOH" },
    { 159,232,"LUUH" },
    { 160,248,"LUHH" },
    { 161, 11,"HULL" },
    { 162, 27,"HUOL" },
    { 163, 43,"HUUL" },
    { 164, 59,"HUHL" },
    { 165, 75,"HULO" },
    { 166, 91,"HUOO" },
    { 167,107,"HUUO" },
    { 168,123,"HUHO" },
    { 169,139,"HULU" },
    { 170,155,"HUOU" },
    { 171,171,"HUUU" },
    { 172,187,"HUHU" },
    { 173,203,"HULH" },
    { 174,219,"HUOH" },
    { 175,235,"HUUH" },
    { 176,251,"HUHH" },
    { 177,  9,"OULL" },
    { 178, 25,"OUOL" },
    { 179, 41,"OUUL" },
    { 180, 57,"OUHL" },
    { 181, 73,"OULO" },
    { 182, 89,"OUOO" },
    { 183,105,"OUUO" },
    { 184,121,"OUHO" },
    { 185,137,"OULU" },
    { 186,153,"OUOU" },
    { 187,169,"OUUU" },
    { 188,185,"OUHU" },
    { 189,201,"OULH" },
    { 190,217,"OUOH" },
    { 191, 10,"UULL" },
    { 192,170,"UUUU" },
    { 193, 32,"LLUL" },
    { 194, 96,"LLUO" },
    { 195,160,"LLUU" },
    { 196,224,"LLUH" },
    { 197, 35,"HLUL" },
    { 198, 99,"HLUO" },
    { 199,163,"HLUU" },
    { 200,227,"HLUH" },
    { 201, 33,"OLUL" },
    { 202, 97,"OLUO" },
    { 203,161,"OLUU" },
    { 204,225,"OLUH" },
    { 205, 44,"LHUL" },
    { 206,108,"LHUO" },
    { 207,172,"LHUU" },
    { 208,236,"LHUH" },
    { 209, 47,"HHUL" },
    { 210,111,"HHUO" },
    { 211,175,"HHUU" },
    { 212,239,"HHUH" },
    { 213, 45,"OHUL" },
    { 214,109,"OHUO" },
    { 215,173,"OHUU" },
    { 216,237,"OHUH" },
    { 217, 36,"LOUL" },
    { 218,100,"LOUO" },
    { 219,164,"LOUU" },
    { 220,228,"LOUH" },
    { 221, 39,"HOUL" },
    { 222,103,"HOUO" },
    { 223,167,"HOUU" },
    { 224,231,"HOUH" },
    { 225, 37,"OOUL" },
    { 226,101,"OOUO" },
    { 227,165,"OOUU" },
    { 228,229,"OOUH" },
    { 229,128,"LLLU" },
    { 230,131,"HLLU" },
    { 231,129,"OLLU" },
    { 232,140,"LHLU" },
    { 233,143,"HHLU" },
    { 234,141,"OHLU" },
    { 235,132,"LOLU" },
    { 236,135,"HOLU" },
    { 237,133,"OOLU" },
    { 238,176,"LLHU" },
    { 239,179,"HLHU" },
    { 240,177,"OLHU" },
    { 241,188,"LHHU" },
    { 242,191,"HHHU" },
    { 243,189,"OHHU" },
    { 244,180,"LOHU" },
    { 245,183,"HOHU" },
    { 246,181,"OOHU" },
    { 247,144,"LLOU" },
    { 248,147,"HLOU" },
    { 249,145,"OLOU" },
    { 250,156,"LHOU" },
    { 251,159,"HHOU" },
    { 252,157,"OHOU" },
    { 253,148,"LOOU" },
    { 254,151,"HOOU" },
    { 255,149,"OOOU" }
};

#endif
