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

// Print PDP-8/L memory cycle trace
// Uses MWHOLD/MWSTEP bits of pdp8lxmem.v to stop the processor at end of each memory cycle
// Must have ENLO4K set so it will be able to stop processor when it accesses low 4K memory

//  ./z8lmctrace [-clear]

#include <stdint.h>
#include <stdio.h>
#include <string>
#include <string.h>
#include <unistd.h>

#include "disassemble.h"
#include "z8ldefs.h"
#include "z8lutil.h"

#define FIELD(index,mask) ((pdpat[index] & mask) / (mask & - mask))

enum State {
    ST_UNKN,
    ST_FETCH,
    ST_DEFER,
    ST_EXEC,
    ST_WC,
    ST_CA,
    ST_BRK,
    ST_INTACK
};

int main (int argc, char **argv)
{
    Z8LPage z8p;
    uint32_t volatile *pdpat  = z8p.findev ("8L", NULL, NULL, false);
    uint32_t volatile *xmemat = z8p.findev ("XM", NULL, NULL, false);
    uint32_t volatile *extmem = z8p.extmem ();

    printf ("8L version %08X\n", pdpat[Z_VER]);
    printf ("XM version %08X\n", xmemat[Z_VER]);

    bool readflag = false;
    for (int i = 0; ++ i < argc;) {
        if (strcmp (argv[i], "-?") == 0) {
            puts ("");
            puts ("  Print memory cycles as they happen:\n");
            puts ("    ./z8lmctrace [-read]\n");
            puts ("");
            puts ("      -read : step at reads as well as writes");
            puts ("");
            puts ("  Must have -enlo4k mode set so extmem gets used for everything\n");
            puts ("    eg, ./z8lreal -enlo4k\n");
            puts ("");
            puts ("  Clear memory cycle trace mode:\n");
            puts ("    ./z8lmectrace -clear\n");
            puts ("");
            return 0;
        }
        if (strcasecmp (argv[i], "-clear") == 0) {
            xmemat[1] &= ~ XM_MWHOLD & ~ XM_MRHOLD;
            return 0;
        }
        if (strcasecmp (argv[i], "-read") == 0) {
            readflag = true;
            continue;
        }
        fprintf (stderr, "unknown argument %s\n", argv[i]);
        return 1;
    }

    setlinebuf (stdout);

    if (! (xmemat[1] & XM_ENLO4K)) {
        fprintf (stderr, "enlo4k mode must be set\n");
        ABORT ();
    }

    xmemat[1] = (xmemat[1] & ~ XM_MRHOLD) | XM_MWHOLD | (readflag ? XM_MRHOLD : 0);

    bool intack = false;
    bool wcca   = false;
    bool last_dmabrk = false;
    bool last_intack = false;
    bool last_jmpjms = false;
    bool last_wcca   = false;
    State shadowst    = ST_UNKN;
    uint16_t shadowir = 0;
    while (true) {

        // tell pdp8lxmem.v to let processor do one mem cycle up to just before sending STROBE pulse
        bool halted = false;
        uint16_t readdata = 0xFFFFU;
        if (readflag) {
            bool halted = false;
            for (int i = 0; xmemat[1] & XM_MRSTEP; i ++) {
                if (i > 10000) {
                    if (FIELD (Z_RF, f_oB_RUN)) {
                        fprintf (stderr, "timed out waiting for cycle to complete\n");
                        ABORT ();
                    }
                    if (! halted) {
                        printf ("processor halted\n");
                        halted   = true;
                        intack   = false;
                        wcca     = false;
                        shadowst = ST_UNKN;
                        last_dmabrk = false;
                        last_intack = false;
                        last_jmpjms = false;
                        last_wcca   = false;
                    }
                    usleep (10000);
                    i = 0;
                }
            }
            readdata = FIELD (Z_RC, c_i_MEM) ^ 07777;
            xmemat[1] |= XM_MRSTEP;
        }

        // tell pdp8lxmem.v to let processor do one mem cycle up to just before sending MEMDONE pulse
        for (int i = 0; xmemat[1] & XM_MWSTEP; i ++) {
            if (i > 10000) {
                if (FIELD (Z_RF, f_oB_RUN)) {
                    fprintf (stderr, "timed out waiting for cycle to complete\n");
                    ABORT ();
                }
                if (! halted) {
                    printf ("processor halted\n");
                    halted   = true;
                    intack   = false;
                    wcca     = false;
                    shadowst = ST_UNKN;
                    last_dmabrk = false;
                    last_intack = false;
                    last_jmpjms = false;
                    last_wcca   = false;
                }
                usleep (10000);
                i = 0;
            }
        }

        // pdp8lxmem.v is stopped just before it outputs the MEMDONE (TP4) signal

        uint16_t xaddr = FIELD (Z_RL, l_xbraddr);   // get 15-bit address left in extmem ram address register
        uint16_t writedata = FIELD (Z_RH, h_oBMB);  // get what PDP is sending out to be written
        uint16_t wdata = extmem[xaddr];             // read extmem directly to get what ended up in there

        bool didio  = FIELD (Z_RF, f_didio);        // cycle did some i/o
        bool dmabrk = ! FIELD (Z_RF, f_o_B_BREAK);  // at end of a BRK cycle
        bool jmpjms = FIELD (Z_RF, f_oJMP_JMS);     // ir contains a jmp or jms opcode

        // try to figure out what state it was in during the cycle
        if (dmabrk) shadowst = ST_BRK;
        else if (intack) shadowst = ST_INTACK;
        else if (wcca) shadowst = last_wcca ? ST_CA : ST_WC;
        else if (didio | last_dmabrk | last_intack) shadowst = ST_FETCH;
        else if (! last_jmpjms & jmpjms) shadowst = ST_FETCH;
        else switch (shadowst) {
            case ST_FETCH: {
                if ((shadowir & 07000) < 05000) shadowst = (shadowir & 00400) ? ST_DEFER : ST_EXEC; // AND,TAD,ISZ,DCA,JMS
                else if ((shadowir & 07400) == 05400) shadowst = ST_DEFER;                          // JMPI
                break;
            }
            case ST_DEFER: {
                shadowst = ((shadowir & 07000) == 05000) ? ST_FETCH : ST_EXEC;
                break;
            }
            case ST_EXEC: {
                shadowst = ST_FETCH;
                break;
            }
            default: {
                shadowst = ST_UNKN;
                break;
            }
        }

        if (shadowst == ST_FETCH) shadowir = wdata;

        // format state as a string for printing
        char const *statestr = "";
        std::string disasstr;
        switch (shadowst) {
            case ST_UNKN:   break;
            case ST_FETCH:  { statestr = "  FETCH  "; disasstr = disassemble (shadowir, xaddr & 07777); break; }
            case ST_DEFER:  { statestr = "  DEFER";  break; }
            case ST_EXEC:   { statestr = "  EXEC";   break; }
            case ST_BRK:    { statestr = "  BRK";    break; }
            case ST_WC:     { statestr = "  WC";     break; }
            case ST_CA:     { statestr = "  CA";     break; }
            case ST_INTACK: { statestr = "  INTACK"; break; }
        }

        // print out mem cycle info
        printf ("%08X:  %05o / ", pdpat[Z_RN], xaddr);
        if (readflag) printf ("%04o => ", readdata);
        printf ("%04o %c %04o", writedata, ((writedata == wdata) ? '=' : '?'), wdata);
        printf ("  AC=%04o DF=%o IF=%o IFAJ=%o ION=%o%s%s\n",
            (pdpat[Z_RH] & h_oBAC) / h_oBAC0,
            (xmemat[2] & XM2_DFLD) / XM2_DFLD0,
            (xmemat[2] & XM2_IFLD) / XM2_IFLD0,
            (xmemat[2] & XM2_IFLDAFJMP) / XM2_IFLDAFJMP0,
            FIELD (Z_RF, f_oC36B2),
            statestr, disasstr.c_str ());

        // get ready for next cycle
        last_dmabrk = dmabrk;
        last_intack = intack;
        last_jmpjms = jmpjms;
        last_wcca   = wcca;

        intack = ! FIELD (Z_RF, f_o_LOAD_SF);       // next cycle is interrupt acknowledge
        wcca   = ! FIELD (Z_RF, f_o_SP_CYC_NEXT);   // next cycle is wc or ca

        xmemat[1] |= XM_MWSTEP;
    }
}
