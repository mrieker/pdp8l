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

// Jam a value in TTY40's keyboard register

// Useful in this scenario:
//  1) Get processor running:
//      0020  CLA IAC   ; put 0001 into ac
//      0021  IOT 6406  ; clear ac, read kb reg
//      0022  JMP 0020  ; repeat forever
//  2) Get z8ldump running on a screen
//  3) Use this program to write values to kb reg
//  4) See the values in the AC of z8ldump

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "z8ldefs.h"
#include "z8lutil.h"

static uint32_t volatile *ttyat;

static bool findtt (void *param, uint32_t volatile *ttyat);

int main (int argc, char **argv)
{
    if (argc != 2) {
        fprintf (stderr, "usage: z8lkbjam.armv7l <kbvalue>\n");
        return 1;
    }

    uint32_t port = 040;
    Z8LPage z8p;
    ttyat = z8p.findev ("TT", findtt, &port, true, true);

    ttyat[Z_TTYKB] = KB_FLAG | KB_ENAB | strtoul (argv[1], NULL, 0);
    printf ("%08X\n", ttyat[Z_TTYKB]);
    return 0;
}

static bool findtt (void *param, uint32_t volatile *ttyat)
{
    uint32_t port = *(uint32_t *) param;
    if (ttyat == NULL) {
        fprintf (stderr, "findtt: cannot find TT port %02o\n", port);
        ABORT ();
    }
    return (ttyat[Z_TTYPN] & 077) == port;
}
