//    Copyright (C) Mike Rieker, Beverly, MA USA
//    www.outerworldapps.com
//
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; version 2 of the License.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    EXPECT it to FAIL when someone's HeALTh or PROpeRTy is at RISk.
//
//    You should have received a copy of the GNU General Public License
//    along with this program; if not, write to the Free Software
//    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
//    http://www.gnu.org/licenses/gpl-2.0.html

// Runs PIPan8L on a Zynq board programmed with 8/L emulation code

#include <fcntl.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include "abcd.h"
#include "padlib.h"
#include "pindefs.h"

#define ABORT() do { fprintf (stderr, "abort() %s:%d\n", __FILE__, __LINE__); abort (); } while (0)
#define ASSERT(cond) do { if (__builtin_constant_p (cond)) { if (!(cond)) asm volatile ("assert failure line %c0" :: "i"(__LINE__)); } else { if (!(cond)) ABORT (); } } while (0)

#define Z_VER 0
#define Z_RA 1
#define Z_RB 2
#define Z_RC 3
#define Z_RD 4
#define Z_RE 5
#define Z_RF 6
#define Z_RG 7
#define Z_RH 8
#define Z_RI 9
#define Z_RJ 10
#define Z_RK 11
#define Z_N 12

#define a_iBEMA         (1U <<  0)
#define a_iCA_INCREMENT (1U <<  1)
#define a_iDATA_IN      (1U <<  2)
#define a_iMEMINCR      (1U <<  3)
#define a_iMEM_P        (1U <<  4)
#define a_iTHREECYCLE   (1U <<  5)
#define a_i_AC_CLEAR    (1U <<  6)
#define a_i_BRK_RQST    (1U <<  7)
#define a_i_EA          (1U <<  8)
#define a_i_EMA         (1U <<  9)
#define a_i_INT_INHIBIT (1U << 10)
#define a_i_INT_RQST    (1U << 11)
#define a_i_IO_SKIP     (1U << 12)
#define a_i_MEMDONE     (1U << 13)
#define a_i_STROBE      (1U << 14)
#define a_softreset     (1U << 29)
#define a_softclock     (1U << 30)
#define a_softenab      (1U << 31)

#define b_swCONT        (1U <<  0)
#define b_swDEP         (1U <<  1)
#define b_swDFLD        (1U <<  2)
#define b_swEXAM        (1U <<  3)
#define b_swIFLD        (1U <<  4)
#define b_swLDAD        (1U <<  5)
#define b_swMPRT        (1U <<  6)
#define b_swSTEP        (1U <<  7)
#define b_swSTOP        (1U <<  8)
#define b_swSTART       (1U <<  9)

#define c_iINPUTBUS (07777U <<  0)
#define c_iMEM      (07777U << 16)
#define d_i_DMAADDR (07777U <<  0)
#define d_i_DMADATA (07777U << 16)
#define e_swSR      (07777U <<  0)

#define f_oBIOP1        (1U <<  0)
#define f_oBIOP2        (1U <<  1)
#define f_oBIOP4        (1U <<  2)
#define f_oBTP2         (1U <<  3)
#define f_oBTP3         (1U <<  4)
#define f_oBTS_1        (1U <<  5)
#define f_oBTS_3        (1U <<  6)
#define f_oBUSINIT      (1U <<  7)
#define f_oBWC_OVERFLOW (1U <<  8)
#define f_oB_BREAK      (1U <<  9)
#define f_oE_SET_F_SET  (1U << 10)
#define f_oJMP_JMS      (1U << 11)
#define f_oLINE_LOW     (1U << 12)
#define f_oMEMSTART     (1U << 13)
#define f_o_ADDR_ACCEPT (1U << 14)
#define f_o_BF_ENABLE   (1U << 15)
#define f_o_BUSINIT     (1U << 16)
#define f_o_B_RUN       (1U << 17)
#define f_o_DF_ENABLE   (1U << 18)
#define f_o_KEY_CLEAR   (1U << 19)
#define f_o_KEY_DF      (1U << 20)
#define f_o_KEY_IF      (1U << 21)
#define f_o_KEY_LOAD    (1U << 22)
#define f_o_LOAD_SF     (1U << 23)
#define f_o_SP_CYC_NEXT (1U << 24)

#define g_lbBRK         (1U <<  0)
#define g_lbCA          (1U <<  1)
#define g_lbDEF         (1U <<  2)
#define g_lbEA          (1U <<  3)
#define g_lbEXE         (1U <<  4)
#define g_lbFET         (1U <<  5)
#define g_lbION         (1U <<  6)
#define g_lbLINK        (1U <<  7)
#define g_lbRUN         (1U <<  8)
#define g_lbWC          (1U <<  9)
#define g_lbIR          (7U << 25)

#define h_oBAC      (07777U <<  0)
#define h_oBMB      (07777U << 16)
#define i_oMA       (07777U <<  0)
#define i_lbAC      (07777U << 16)
#define j_lbMA      (07777U <<  0)
#define j_lbMB      (07777U << 16)

#define c_iINPUTBUS0 (c_iINPUTBUS & - c_iINPUTBUS)
#define c_iMEM0      (c_iMEM      & - c_iMEM)
#define d_i_DMAADDR0 (d_i_DMAADDR & - d_i_DMAADDR)
#define d_i_DMADATA0 (d_i_DMADATA & - d_i_DMADATA)
#define e_swSR0      (e_swSR      & - e_swSR)
#define g_lbIR0      (g_lbIR      & - g_lbIR)
#define h_oBAC0      (h_oBAC      & - h_oBAC)
#define h_oBMB0      (h_oBMB      & - h_oBMB)
#define i_oMA0       (i_oMA       & - i_oMA)
#define i_lbAC0      (i_lbAC      & - i_lbAC)
#define j_lbMA0      (j_lbMA      & - j_lbMA)
#define j_lbMB0      (j_lbMB      & - j_lbMB)

#define k_state     (   7U <<  0)
#define k_memodify  (   3U <<  3)
#define k_memstate  (   7U <<  5)
#define k_timedelay (   7U <<  8)
#define k_timestate (  15U << 11)
#define k_cyclectr  (1023U << 15)

// map Z8L bits to pipan8l bits
static uint32_t ztop[] = {

        // light bulbs
    Z_RG, g_lbBRK,       P_BRK,
    Z_RG, g_lbCA,        P_CAD,
    Z_RG, g_lbDEF,       P_DEF,
    Z_RG, g_lbEA,        P_EMA,
    Z_RG, g_lbEXE,       P_EXE,
    Z_RG, g_lbFET,       P_FET,
    Z_RG, g_lbION,       P_ION,
    Z_RG, g_lbLINK,      P_LINK,
    Z_RG, g_lbRUN,       P_RUN,
    Z_RG, g_lbWC,        P_WCT,
    Z_RG, g_lbIR0 <<  2, P_IR00,
    Z_RG, g_lbIR0 <<  1, P_IR01,
    Z_RG, g_lbIR0 <<  0, P_IR02,
    Z_RI, i_lbAC0 << 11, P_AC00,
    Z_RI, i_lbAC0 << 10, P_AC01,
    Z_RI, i_lbAC0 <<  9, P_AC02,
    Z_RI, i_lbAC0 <<  8, P_AC03,
    Z_RI, i_lbAC0 <<  7, P_AC04,
    Z_RI, i_lbAC0 <<  6, P_AC05,
    Z_RI, i_lbAC0 <<  5, P_AC06,
    Z_RI, i_lbAC0 <<  4, P_AC07,
    Z_RI, i_lbAC0 <<  3, P_AC08,
    Z_RI, i_lbAC0 <<  2, P_AC09,
    Z_RI, i_lbAC0 <<  1, P_AC10,
    Z_RI, i_lbAC0 <<  0, P_AC11,
    Z_RJ, j_lbMA0 << 11, P_MA00,
    Z_RJ, j_lbMA0 << 10, P_MA01,
    Z_RJ, j_lbMA0 <<  9, P_MA02,
    Z_RJ, j_lbMA0 <<  8, P_MA03,
    Z_RJ, j_lbMA0 <<  7, P_MA04,
    Z_RJ, j_lbMA0 <<  6, P_MA05,
    Z_RJ, j_lbMA0 <<  5, P_MA06,
    Z_RJ, j_lbMA0 <<  4, P_MA07,
    Z_RJ, j_lbMA0 <<  3, P_MA08,
    Z_RJ, j_lbMA0 <<  2, P_MA09,
    Z_RJ, j_lbMA0 <<  1, P_MA10,
    Z_RJ, j_lbMA0 <<  0, P_MA11,
    Z_RJ, j_lbMB0 << 11, P_MB00,
    Z_RJ, j_lbMB0 << 10, P_MB01,
    Z_RJ, j_lbMB0 <<  9, P_MB02,
    Z_RJ, j_lbMB0 <<  8, P_MB03,
    Z_RJ, j_lbMB0 <<  7, P_MB04,
    Z_RJ, j_lbMB0 <<  6, P_MB05,
    Z_RJ, j_lbMB0 <<  5, P_MB06,
    Z_RJ, j_lbMB0 <<  4, P_MB07,
    Z_RJ, j_lbMB0 <<  3, P_MB08,
    Z_RJ, j_lbMB0 <<  2, P_MB09,
    Z_RJ, j_lbMB0 <<  1, P_MB10,
    Z_RJ, j_lbMB0 <<  0, P_MB11,

        // switches
    Z_RB, b_swCONT,      P_CONT,
    Z_RB, b_swDEP,       P_DEP,
    Z_RB, b_swDFLD,      P_DFLD,
    Z_RB, b_swEXAM,      P_EXAM,
    Z_RB, b_swIFLD,      P_IFLD,
    Z_RB, b_swLDAD,      P_LDAD,
    Z_RB, b_swMPRT,      P_MPRT,
    Z_RB, b_swSTEP,      P_STEP,
    Z_RB, b_swSTOP,      P_STOP,
    Z_RB, b_swSTART,     P_STRT,
    Z_RE, e_swSR0 << 11, P_SR00,
    Z_RE, e_swSR0 << 10, P_SR01,
    Z_RE, e_swSR0 <<  9, P_SR02,
    Z_RE, e_swSR0 <<  8, P_SR03,
    Z_RE, e_swSR0 <<  7, P_SR04,
    Z_RE, e_swSR0 <<  6, P_SR05,
    Z_RE, e_swSR0 <<  5, P_SR06,
    Z_RE, e_swSR0 <<  4, P_SR07,
    Z_RE, e_swSR0 <<  3, P_SR08,
    Z_RE, e_swSR0 <<  2, P_SR09,
    Z_RE, e_swSR0 <<  1, P_SR10,
    Z_RE, e_swSR0 <<  0, P_SR11,

     0, 0, 0
};

// names of all z8l bits

struct ZName {
    int zi;
    uint32_t zm;
    char const *zn;
};

static ZName const znames[] = {
    { Z_RA, a_iBEMA         , "iBEMA"         },
    { Z_RA, a_iCA_INCREMENT , "iCA_INCREMENT" },
    { Z_RA, a_iDATA_IN      , "iDATA_IN"      },
    { Z_RA, a_iMEMINCR      , "iMEMINCR"      },
    { Z_RA, a_iMEM_P        , "iMEM_P"        },
    { Z_RA, a_iTHREECYCLE   , "iTHREECYCLE"   },
    { Z_RA, a_i_AC_CLEAR    , "i_AC_CLEAR"    },
    { Z_RA, a_i_BRK_RQST    , "i_BRK_RQST"    },
    { Z_RA, a_i_EA          , "i_EA"          },
    { Z_RA, a_i_EMA         , "i_EMA"         },
    { Z_RA, a_i_INT_INHIBIT , "i_INT_INHIBIT" },
    { Z_RA, a_i_INT_RQST    , "i_INT_RQST"    },
    { Z_RA, a_i_IO_SKIP     , "i_IO_SKIP"     },
    { Z_RA, a_i_MEMDONE     , "i_MEMDONE"     },
    { Z_RA, a_i_STROBE      , "i_STROBE"      },
    { Z_RA, a_softreset     , "softreset"     },
    { Z_RA, a_softclock     , "softclock"     },
    { Z_RA, a_softenab      , "softenab"      },

    { Z_RB, b_swCONT        , "swCONT"        },
    { Z_RB, b_swDEP         , "swDEP"         },
    { Z_RB, b_swDFLD        , "swDFLD"        },
    { Z_RB, b_swEXAM        , "swEXAM"        },
    { Z_RB, b_swIFLD        , "swIFLD"        },
    { Z_RB, b_swLDAD        , "swLDAD"        },
    { Z_RB, b_swMPRT        , "swMPRT"        },
    { Z_RB, b_swSTEP        , "swSTEP"        },
    { Z_RB, b_swSTOP        , "swSTOP"        },
    { Z_RB, b_swSTART       , "swSTART"       },

    { Z_RC, c_iINPUTBUS     , "iINPUTBUS"     },
    { Z_RC, c_iMEM          , "iMEM"          },
    { Z_RD, d_i_DMAADDR     , "i_DMAADDR"     },
    { Z_RD, d_i_DMADATA     , "i_DMADATA"     },
    { Z_RE, e_swSR          , "swSR"          },

    { Z_RF, f_oBIOP1        , "oBIOP1"        },
    { Z_RF, f_oBIOP2        , "oBIOP2"        },
    { Z_RF, f_oBIOP4        , "oBIOP4"        },
    { Z_RF, f_oBTP2         , "oBTP2"         },
    { Z_RF, f_oBTP3         , "oBTP3"         },
    { Z_RF, f_oBTS_1        , "oBTS_1"        },
    { Z_RF, f_oBTS_3        , "oBTS_3"        },
    { Z_RF, f_oBUSINIT      , "oBUSINIT"      },
    { Z_RF, f_oBWC_OVERFLOW , "oBWC_OVERFLOW" },
    { Z_RF, f_oB_BREAK      , "oB_BREAK"      },
    { Z_RF, f_oE_SET_F_SET  , "oE_SET_F_SET"  },
    { Z_RF, f_oJMP_JMS      , "oJMP_JMS"      },
    { Z_RF, f_oLINE_LOW     , "oLINE_LOW"     },
    { Z_RF, f_oMEMSTART     , "oMEMSTART"     },
    { Z_RF, f_o_ADDR_ACCEPT , "o_ADDR_ACCEPT" },
    { Z_RF, f_o_BF_ENABLE   , "o_BF_ENABLE"   },
    { Z_RF, f_o_BUSINIT     , "o_BUSINIT"     },
    { Z_RF, f_o_B_RUN       , "o_B_RUN"       },
    { Z_RF, f_o_DF_ENABLE   , "o_DF_ENABLE"   },
    { Z_RF, f_o_KEY_CLEAR   , "o_KEY_CLEAR"   },
    { Z_RF, f_o_KEY_DF      , "o_KEY_DF"      },
    { Z_RF, f_o_KEY_IF      , "o_KEY_IF"      },
    { Z_RF, f_o_KEY_LOAD    , "o_KEY_LOAD"    },
    { Z_RF, f_o_LOAD_SF     , "o_LOAD_SF"     },
    { Z_RF, f_o_SP_CYC_NEXT , "o_SP_CYC_NEXT" },

    { Z_RG, g_lbBRK         , "lbBRK"         },
    { Z_RG, g_lbCA          , "lbCA"          },
    { Z_RG, g_lbDEF         , "lbDEF"         },
    { Z_RG, g_lbEA          , "lbEA"          },
    { Z_RG, g_lbEXE         , "lbEXE"         },
    { Z_RG, g_lbFET         , "lbFET"         },
    { Z_RG, g_lbION         , "lbION"         },
    { Z_RG, g_lbLINK        , "lbLINK"        },
    { Z_RG, g_lbRUN         , "lbRUN"         },
    { Z_RG, g_lbWC          , "lbWC"          },
    { Z_RG, g_lbIR          , "lbIR"          },

    { Z_RH, h_oBAC          , "oBAC"          },
    { Z_RH, h_oBMB          , "oBMB"          },
    { Z_RI, i_oMA           , "oMA"           },
    { Z_RI, i_lbAC          , "lbAC"          },
    { Z_RJ, j_lbMA          , "lbMA"          },
    { Z_RJ, j_lbMB          , "lbMB"          },

    { Z_RK, k_state,     "state"     },
    { Z_RK, k_memodify,  "memodify"  },
    { Z_RK, k_memstate,  "memstate"  },
    { Z_RK, k_timedelay, "timedelay" },
    { Z_RK, k_timestate, "timestate" },
    { Z_RK, k_cyclectr,  "cyclectr"  },

    { 0, 0, NULL }
};

static uint32_t const forceons[Z_N] = {
    0, a_i_AC_CLEAR | a_i_BRK_RQST | a_i_EA | a_i_EMA | a_i_INT_INHIBIT | a_i_INT_RQST | a_i_IO_SKIP | a_i_MEMDONE | a_i_STROBE,
    0, 0, d_i_DMAADDR | d_i_DMADATA };



Z8LLib::Z8LLib ()
{
    zynqfd = -1;
    zynqpage = NULL;
}

Z8LLib::~Z8LLib ()
{
    munmap ((void *) zynqpage, 4096);
    close (zynqfd);
    zynqpage = NULL;
    zynqfd = -1;
}

void Z8LLib::openpads ()
{
    munmap ((void *) zynqpage, 4096);
    close (zynqfd);
    zynqpage = NULL;
    zynqfd = -1;

    // use the i2cmaster.v module in the fpga to access the i2c bus
    zynqfd = open ("/proc/zynqgpio", O_RDWR);
    if (zynqfd < 0) {
        fprintf (stderr, "Z8LLib::openpads: error opening /proc/zynqgpio: %m\n");
        ABORT ();
    }

    void *zynqptr = mmap (NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, zynqfd, 0);
    if (zynqptr == MAP_FAILED) {
        fprintf (stderr, "Z8LLib::openpads: error mmapping /proc/zynqgpio: %m\n");
        ABORT ();
    }
    zynqpage = (uint32_t volatile *) zynqptr;
    uint32_t ver = zynqpage[Z_VER];
    fprintf (stderr, "Z8LLib::openpads: zynq version %08X\n", ver);
    if ((ver & 0xFFFF0000U) != (('8' << 24) | ('L' << 16))) {
        fprintf (stderr, "Z8LLib::openpads: bad magic number\n");
        ABORT ();
    }

    usleep (10);
    zynqpage[Z_RA] = a_softenab | a_softreset;
    usleep (10);
    zynqpage[Z_RA] = a_softenab | a_softreset | a_softclock;
    usleep (10);
    zynqpage[Z_RA] = a_softenab | a_softreset;

    for (int i = 0; i < Z_N; i ++) {
        usleep (10);
        zynqpage[i] = ((i == Z_RA) ? (a_softenab | a_softreset) : 0) | forceons[i];
        usleep (10);
        printf ("Z8LLib::openpads*: %2d / %08X\n", i, zynqpage[i]);
    }
}

// read pins from zynq pdp8l
void Z8LLib::readpads (uint16_t *pads)
{
    uint32_t z8ls[Z_N];
    for (int i = 0; i < Z_N; i ++) {
        z8ls[i] = zynqpage[i];
    }

    memset (pads, 0, P_NU16S * sizeof *pads);

    for (int i = 0; ztop[i*3] != 0; i ++) {
        uint32_t zi = ztop[i*3+0];
        uint32_t zm = ztop[i*3+1];

        if (z8ls[zi] & zm) {
            uint32_t pb = ztop[i*3+2];
            uint32_t pi = pb >> 4;
            uint16_t pm = 1U << (pb & 15);
            pads[pi] |= pm;
        }
    }
}

static char const *const memodifynames[4] = { "NONE", "DEPOS", "STATE", "???3" };
static char const *const memstatenames[8] = { "IDLE", "START", "LOCAL", "EXTERN", "STARTIO", "DOIO", "FINISH", "???7" };
static char const *const statenames[8] = { "START", "FETCH", "DEFER", "EXEC", "WC", "CA", "BRK", "INTAK" };
static char const *const timestatenames[16] = {
    "IDLE",
    "TS1BODY", "TP1BEG", "TP1END",
    "TS2BODY", "TP2BEG", "TP2END",
    "TS3BODY", "TP3BEG", "TP3END",
    "TS4BODY", "TP4BEG", "TP4END",
    "???13", "???14", "???15" };

// write values to zynq pdp8l
void Z8LLib::writepads (uint16_t const *pads)
{
    uint32_t z8ls[Z_N];
    memcpy (z8ls, forceons, sizeof z8ls);

    for (int i = 0; ztop[i*3] != 0; i ++) {
        uint32_t pb = ztop[i*3+2];
        uint32_t pi = pb >> 4;
        uint16_t pm = 1U << (pb & 15);
        if (pads[pi] & pm) {
            uint32_t zi = ztop[i*3+0];
            uint32_t zm = ztop[i*3+1];
            z8ls[zi] |= zm;
        }
    }

    z8ls[Z_RA] = (z8ls[Z_RA] | a_softenab) & ~ a_softreset & ~ a_softclock;

    for (int i = 0; i < Z_N; i ++) {
        zynqpage[i] = z8ls[i];
    }

    printf ("Z8LLib::writepads*: = = = = = = = =\n");
    int iter = 0;
    while (true) {
        usleep (10);
        zynqpage[Z_RA] ^= a_softclock;
        usleep (10);
        if (-- iter <= 0) {
            for (int i = 0; i < Z_N; i ++) {
                uint32_t zr = zynqpage[i];
                printf ("Z8LLib::writepads*: %2d / %08X\n", i, zr);
                for (int j = 0; znames[j].zn != NULL; j ++) {
                    if (znames[j].zi == i) {
                        printf ("  %s=", znames[j].zn);
                        uint32_t mask   = znames[j].zm;
                        uint32_t lowbit = mask & - mask;
                        uint32_t value  = (zr & mask) / lowbit;
                        if ((i == Z_RK) && (mask == k_memodify)) {
                            printf ("%-5s", memodifynames[value]);
                            continue;
                        }
                        if ((i == Z_RK) && (mask == k_memstate)) {
                            printf ("%-7s", memstatenames[value]);
                            continue;
                        }
                        if ((i == Z_RK) && (mask == k_state)) {
                            printf ("%-5s", statenames[value]);
                            continue;
                        }
                        if ((i == Z_RK) && (mask == k_timestate)) {
                            printf ("%-7s", timestatenames[value]);
                            continue;
                        }
                        uint32_t width  = 1;
                        while (1ULL << (width * 3) <= mask / lowbit) width ++;
                        printf ("%0*o", width, value);
                    }
                }
                putchar ('\n');
            }
            printf ("Z8LLib::writepads*: > ");
            fflush (stdout);
            char buff[8];
            int rc = read (STDIN_FILENO, buff, sizeof buff);
            if (rc <= 0) break;
            if (buff[0] == '\n') continue;
            iter = atoi (buff);
            if (iter <= 0) break;
        }
    }
}
