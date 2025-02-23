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

// Switch pulse bit generator to trigger on ISZAC I/O opcode 6004

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "z8ldefs.h"
#include "z8lutil.h"

int main (int argc, char **argv)
{
    char *p;

    bool doiszac = true;
    uint32_t width = 599;   // 6.0uS
    for (int i = 0; ++ i < argc;) {
        if (strcmp (argv[i], "-?") == 0) {
            puts ("");
            puts ("  Set pulsebit generator parameters");
            puts ("");
            puts ("    ./z8lpbit [-noiszac] [-width <width>]");
            puts ("");
            puts ("    -noiszac = disable ISZAC I/O instruction 6004");
            puts ("    -width = set width of pulses in 10nS units, 1..8192");
            puts ("");
            return 0;
        }
        if (strcasecmp (argv[i], "-noiszac") == 0) {
            doiszac = false;
            continue;
        }
        if (strcasecmp (argv[i], "-width") == 0) {
            if ((++ i >= argc) || (argv[i][0] == '-')) {
                fprintf (stderr, "missing -width argument\n");
                return 1;
            }
            width = strtoul (argv[i], &p, 0);
            if ((*p != 0) || (width == 0) || (width > 8192)) {
                fprintf (stderr, "width %s must be integer in range 1..8192\n", argv[i]);
                return 1;
            }
            -- width;
            continue;
        }
        fprintf (stderr, "unknown argument %s\n", argv[i]);
        return 1;
    }

    Z8LPage z8p;
    uint32_t volatile *pbat = z8p.findev ("PB", NULL, NULL, true);
    pbat[1] = (doiszac ? (1U << 31) : 0) | (width << 13);
    return 0;
}
