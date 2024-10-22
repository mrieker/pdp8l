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

// which of the pins are outputs (switches)
static uint16_t const wrmsks[P_NU16S] = { P0_WMSK, P1_WMSK, P2_WMSK, P3_WMSK, P4_WMSK };
// active low outputs
static uint16_t const wrrevs[P_NU16S] = { P0_WREV, P1_WREV, P2_WREV, P3_WREV, P4_WREV };
// active low inputs + active low outputs (so we can read the outputs back)
static uint16_t const rdwrrevs[P_NU16S] = { P0_RREV | P0_WREV, P1_RREV | P1_WREV, P2_RREV | P2_WREV, P3_RREV | P3_WREV, P4_RREV | P4_WREV };


static uint32_t ztop[] = {

        // light bulbs
     7, g_lbBRK,       P_BRK,
     7, g_lbCA,        P_CAD,
     7, g_lbDEF,       P_DEF,
     7, g_lbEA,        P_EMA,
     7, g_lbEXE,       P_EXE,
     7, g_lbFET,       P_FET,
     7, g_lbION,       P_ION,
     7, g_lbLINK,      P_LINK,
     7, g_lbRUN,       P_RUN,
     7, g_lbWC,        P_WCT,
     7, g_lbIR0 <<  2, P_IR00,
     7, g_lbIR0 <<  1, P_IR01,
     7, g_lbIR0 <<  0, P_IR02,
     9, i_lbAC0 << 11, P_AC00,
     9, i_lbAC0 << 10, P_AC01,
     9, i_lbAC0 <<  9, P_AC02,
     9, i_lbAC0 <<  8, P_AC03,
     9, i_lbAC0 <<  7, P_AC04,
     9, i_lbAC0 <<  6, P_AC05,
     9, i_lbAC0 <<  5, P_AC06,
     9, i_lbAC0 <<  4, P_AC07,
     9, i_lbAC0 <<  3, P_AC08,
     9, i_lbAC0 <<  2, P_AC09,
     9, i_lbAC0 <<  1, P_AC10,
     9, i_lbAC0 <<  0, P_AC11,
    10, j_lbMA0 << 11, P_MA00,
    10, j_lbMA0 << 10, P_MA01,
    10, j_lbMA0 <<  9, P_MA02,
    10, j_lbMA0 <<  8, P_MA03,
    10, j_lbMA0 <<  7, P_MA04,
    10, j_lbMA0 <<  6, P_MA05,
    10, j_lbMA0 <<  5, P_MA06,
    10, j_lbMA0 <<  4, P_MA07,
    10, j_lbMA0 <<  3, P_MA08,
    10, j_lbMA0 <<  2, P_MA09,
    10, j_lbMA0 <<  1, P_MA10,
    10, j_lbMA0 <<  0, P_MA11,
    10, j_lbMB0 << 11, P_MB00,
    10, j_lbMB0 << 10, P_MB01,
    10, j_lbMB0 <<  9, P_MB02,
    10, j_lbMB0 <<  8, P_MB03,
    10, j_lbMB0 <<  7, P_MB04,
    10, j_lbMB0 <<  6, P_MB05,
    10, j_lbMB0 <<  5, P_MB06,
    10, j_lbMB0 <<  4, P_MB07,
    10, j_lbMB0 <<  3, P_MB08,
    10, j_lbMB0 <<  2, P_MB09,
    10, j_lbMB0 <<  1, P_MB10,
    10, j_lbMB0 <<  0, P_MB11,

        // switches
     2, b_swCONT,      P_CONT,
     2, b_swDEP,       P_DEP,
     2, b_swDFLD,      P_DFLD,
     2, b_swEXAM,      P_EXAM,
     2, b_swIFLD,      P_IFLD,
     2, b_swLDAD,      P_LDAD,
     2, b_swMPRT,      P_MPRT,
     2, b_swSTEP,      P_STEP,
     2, b_swSTOP,      P_STOP,
     2, b_swSTART,     P_STRT,
     5, e_swSR0 << 11, P_SR00,
     5, e_swSR0 << 10, P_SR01,
     5, e_swSR0 <<  9, P_SR02,
     5, e_swSR0 <<  8, P_SR03,
     5, e_swSR0 <<  7, P_SR04,
     5, e_swSR0 <<  6, P_SR05,
     5, e_swSR0 <<  5, P_SR06,
     5, e_swSR0 <<  4, P_SR07,
     5, e_swSR0 <<  3, P_SR08,
     5, e_swSR0 <<  2, P_SR09,
     5, e_swSR0 <<  1, P_SR10,
     5, e_swSR0 <<  0, P_SR11,

     0, 0, 0
};


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

    for (int i = 0; i < 11; i ++) {
        zynqpage[i] = -1;
        printf ("Z8LLib::openpads*: %2d / %08X\n", i, zynqpage[i]);
    }

    for (int i = 0; i < 11; i ++) {
        zynqpage[i] = (i == 1) ? (a_softenab | a_softreset | a_i_EA) : 0;
        printf ("Z8LLib::openpads*: %2d / %08X\n", i, zynqpage[i]);
    }
}

// read pins from zynq pdp8l
void Z8LLib::readpads (uint16_t *pads)
{
    uint32_t z8ls[11];
    for (int i = 0; i < 11; i ++) {
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

// write values to zynq pdp8l
void Z8LLib::writepads (uint16_t const *pads)
{
    uint32_t z8ls[11];
    memset (z8ls, 0, sizeof z8ls);

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

    z8ls[1] = (z8ls[1] | a_softenab | a_i_EA) & ~ a_softreset & ~ a_softclock;

    for (int i = 0; i < 11; i ++) {
        zynqpage[i] = z8ls[i];
    }

    printf ("= = = = = = = =\n");
    while (true) {
        usleep (10);
        zynqpage[1] ^= a_softclock;
        usleep (10);
        for (int i = 0; i < 11; i ++) {
            printf ("Z8LLib::writepads*: %2d / %08X\n", i, zynqpage[i]);
        }
        printf ("Z8LLib::writepads*: 11 / %08X  > ", zynqpage[11]);
        fflush (stdout);
        char buff[1];
        int rc = read (STDIN_FILENO, buff, sizeof buff);
        if (rc <= 0) break;
        if (buff[0] != '\n') break;
    }
    write (STDOUT_FILENO, "\n", 1);
}
