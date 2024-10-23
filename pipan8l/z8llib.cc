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

#include "padlib.h"
#include "pindefs.h"
#include "z8ldefs.h"

#define ABORT() do { fprintf (stderr, "abort() %s:%d\n", __FILE__, __LINE__); abort (); } while (0)
#define ASSERT(cond) do { if (__builtin_constant_p (cond)) { if (!(cond)) asm volatile ("assert failure line %c0" :: "i"(__LINE__)); } else { if (!(cond)) ABORT (); } } while (0)

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
    }
    usleep (10);
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

    // if envar z8lclocks defined, clock it that many times silently
    int clocks = 0;
    char const *env = getenv ("z8lclocks");
    if (env != NULL) clocks = atoi (env);
    if (clocks > 0) {
        while (-- clocks > 0) {
            usleep (10);
            zynqpage[Z_RA] = z8ls[Z_RA] | a_softclock;
            usleep (10);
            zynqpage[Z_RA] = z8ls[Z_RA];
        }
        usleep (10);
        return;
    }

    // otherwise, output prompt for clocking
    printf ("Z8LLib::writepads*: = = = = = = = =\n");
    int iter = 0;
    while (true) {
        usleep (10);
        zynqpage[Z_RA] ^= a_softclock;
        usleep (10);
        if (-- iter <= 0) {
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
