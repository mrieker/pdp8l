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
//  ./z8lreal [-enlo4k]

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "z8ldefs.h"
#include "z8lutil.h"

int main (int argc, char **argv)
{
    setlinebuf (stdout);

    bool enlo4k = false;
    for (int i = 0; ++ i < argc;) {
        if (strcasecmp (argv[i], "-enlo4k") == 0) {
            enlo4k = true;
            continue;
        }
        fprintf (stderr, "unknown argument %s\n", argv[i]);
        return 1;
    }

    // access the zynq io page
    // hopefully it has our pdp8l.v code indicated by magic number in first word
    Z8LPage z8p;
    uint32_t volatile *pdpat = z8p.findev ("8L", NULL, NULL, true);
    if (pdpat == NULL) {
        fprintf (stderr, "z8lreal: bad magic number\n");
        ABORT ();
    }
    printf ("8L version %08X\n", pdpat[Z_VER]);

    uint32_t volatile *xmemat = z8p.findev ("XM", NULL, NULL, true);
    if (xmemat == NULL) {
        fprintf (stderr, "z8lreal: no extended memory controller\n");
        ABORT ();
    }
    printf ("XM version %08X\n", xmemat[0]);
    xmemat[1] = XM_ENABLE | (enlo4k ? XM_ENLO4K : 0);

    // select real PDP-8/L and reset io devices
    pdpat[Z_RE] = e_nanocontin | e_nanotrigger | e_softreset;   // no e_simit
    usleep (10);
    pdpat[Z_RA] = ZZ_RA;
    pdpat[Z_RB] = 0;
    pdpat[Z_RC] = 0;
    pdpat[Z_RD] = ZZ_RD;
    pdpat[Z_RF] = 0;
    pdpat[Z_RG] = 0;
    pdpat[Z_RH] = 0;
    pdpat[Z_RI] = 0;
    pdpat[Z_RJ] = 0;
    pdpat[Z_RK] = 0;
    usleep (10);
    pdpat[Z_RE] = e_nanocontin | e_nanotrigger;                 // release reset

    return 0;
}