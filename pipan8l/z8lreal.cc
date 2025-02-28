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

// Switch the Zynq FPGA to use real PDP-8/L instead of pdp8lsim.v
// Resets all FPGA devices to power-on reset state
// Leaves simulator held in power-on reset state

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "z8ldefs.h"
#include "z8lutil.h"

static bool findtty03 (void *param, uint32_t volatile *ttyat)
{
    if (ttyat == NULL) {
        fprintf (stderr, "findtt: cannot find TT port 03\n");
        ABORT ();
    }
    return (ttyat[Z_TTYPN] & 077) == 003;
}

int main (int argc, char **argv)
{
    setlinebuf (stdout);

    bool enlo4k = false;
    bool entty03 = false;
    bool nobrk = false;
    bool os8zap = false;
    for (int i = 0; ++ i < argc;) {
        if (strcmp (argv[i], "-?") == 0) {
            puts ("");
            puts ("     Reset and configure ZTurn FPGA to access real PDP-8/L");
            puts ("     Must be plugged into PDP-8/L via zturn boardset");
            puts ("");
            puts ("  ./z8lreal [-enlo4k] [-entty03] [-nobrk] [-os8zap]");
            puts ("     -enlo4k  : use FPGA extmem for low 4K memory (else use PDP-8/L core)");
            puts ("     -entty03 : use FPGA tty 03 (else use PDP-8/L TTY)");
            puts ("                M706 M707 tty boards must be unplugged for this to work");
            puts ("     -nobrk   : PDP-8/L does not contain BREAK option hardware");
            puts ("                forces -enlo4k so DMA will work in low 4K memory");
            puts ("     -os8zap  : optimize out annoying ISZ JMP .-1 delay loop in OS/8");
            puts ("                must also have -enlo4k set for this to work in low 4K memory");
            puts ("");
            return 0;
        }
        if (strcasecmp (argv[i], "-enlo4k") == 0) {
            enlo4k = true;
            continue;
        }
        if (strcasecmp (argv[i], "-entty03") == 0) {
            entty03 = true;
            continue;
        }
        if (strcasecmp (argv[i], "-nobrk") == 0) {
            enlo4k = true;
            nobrk  = true;
            continue;
        }
        if (strcasecmp (argv[i], "-os8zap") == 0) {
            os8zap = true;
            continue;
        }
        fprintf (stderr, "unknown argument %s\n", argv[i]);
        return 1;
    }

    // access the zynq io page
    // hopefully it has our pdp8l.v code indicated by magic number in first word
    Z8LPage z8p;
    uint32_t volatile *pdpat = z8p.findev ("8L", NULL, NULL, true);
    printf ("8L version %08X\n", pdpat[Z_VER]);

    uint32_t volatile *cmemat = z8p.findev ("CM", NULL, NULL, true);
    printf ("CM version %08X\n", cmemat[0]);

    uint32_t volatile *xmemat = z8p.findev ("XM", NULL, NULL, true);
    printf ("XM version %08X\n", xmemat[0]);

    // select real PDP-8/L and reset everything
    pdpat[Z_RE] = e_nanocontin | e_nanotrigger | e_fpgareset;   // no e_simit
    usleep (10);
    pdpat[Z_RE] = e_nanocontin | e_nanotrigger;                 // release reset
                            // omitting e_simit holds sim in power-on reset state

    xmemat[1]   = XM_ENABLE | (enlo4k ? XM_ENLO4K : 0) | (os8zap ? XM_OS8ZAP : 0);
    cmemat[2]   = nobrk ? CM2_NOBRK : 0;

    // all devices are enabled at this point
    // turn off tty 03 if not wanted
    if (! entty03) {
        uint32_t volatile *tty03 = z8p.findev ("TT", findtty03, NULL, true);
        tty03[Z_TTYKB] &= ~ KB_ENAB;
    }

    return 0;
}
