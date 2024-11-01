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
// We get signals that look a lot like the B,C,D 34,35,36 connectors and the front panel
// Start PIPan8L via z8lpan.sh script, eg,
//  $ z8lpan.sh testmem.tcl
//  pipan8l> looptest testrands

#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "padlib.h"
#include "pindefs.h"
#include "z8ldefs.h"
#include "z8lutil.h"

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
    0, a_simit | a_i_AC_CLEAR | a_i_BRK_RQST | a_i_EA | a_i_EMA | a_i_INT_INHIBIT | a_i_INT_RQST | a_i_IO_SKIP | a_i_MEMDONE | a_i_STROBE,
    0, 0, d_i_DMAADDR | d_i_DMADATA };



Z8LLib::Z8LLib ()
{
    z8p    = NULL;
    extmem = NULL;
    pdpat  = NULL;
    xmemat = NULL;
}

Z8LLib::~Z8LLib ()
{
    if (z8p != NULL) {
        delete z8p;
        z8p    = NULL;
        extmem = NULL;
        pdpat  = NULL;
        xmemat = NULL;
    }
}

void Z8LLib::openpads ()
{
    // open /proc/zynqpdp8l and mmap it
    z8p = new Z8LPage ();

    // find processor registers
    pdpat = z8p->findev ("8L", NULL, NULL, true);
    if (pdpat == NULL) {
        fprintf (stderr, "Z8LLib::openpads: bad magic number\n");
        ABORT ();
    }
    fprintf (stderr, "Z8LLib::openpads: 8L version %08X\n", pdpat[0]);

    // find pdp8lxmem.v registers
    xmemat = z8p->findev ("XM", NULL, NULL, true);
    if (pdpat == NULL) {
        fprintf (stderr, "Z8LLib::openpads: XM not found\n");
        ABORT ();
    }
    fprintf (stderr, "Z8LLib::openpads: XM version %08X\n", xmemat[0]);

    // mmap 32K word extmem chip (extmemmap.v)
    // one 12-bit word in low end of our 32-bit word
    extmem = z8p->extmem ();

    // put everything in power-on-reset state
    for (int i = 0; i < Z_N; i ++) {
        pdpat[i] = ((i == Z_RA) ? a_softreset : 0) | forceons[i];
    }

    // enable extended memory io instruction processing
    // locate lower 4K memory in extmem block memory so it can be accessed via extmem
    xmemat[1] = XM_ENABLE | XM_ENLO4K;
}

// read pins from zynq pdp8l.v
void Z8LLib::readpads (uint16_t *pads)
{
    uint32_t z8ls[Z_N];
    for (int i = 0; i < Z_N; i ++) {
        z8ls[i] = pdpat[i];
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

// write values to zynq pdp8l.v
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

    z8ls[Z_RA] |= pdpat[Z_RA] & a_nanocycle;

    for (int i = 0; i < Z_N; i ++) {
        pdpat[i] = z8ls[i];
    }
}
