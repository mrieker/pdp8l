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

// Interactive test of MCP23017s

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "i2czlib.h"
#include "readprompt.h"

int main ()
{
    I2CZLib *lib = new I2CZLib ();
    lib->openpads (false, false, true, false);
    lib->relall ();
    while (true) {

        // read actual pin values
        uint16_t dirs[4], vals[4];
        lib->readi2c (dirs, vals, false);
        printf (" dir=%04X %04X %04X %04X  vals=%04X %04X %04X %04X\n",
            dirs[0], dirs[1], dirs[2], dirs[3], vals[0], vals[1], vals[2], vals[3]);

        char const *line = readprompt ("> ");
        if (line == NULL) break;

        char const *p = line;
        while ((*p == '\t') || (*p == ' ')) p ++;
        if (*p < ' ') continue;
        char which = *(p ++);

        char *q, *r, *s;
        uint32_t index = strtoul (p, &q, 0);
        uint32_t bitno = strtoul (q, &r, 0);
        uint32_t value = strtoul (r, &s, 0);

        if ((*s > ' ') || (r == s) || (index > 3) || (bitno > 15) || (value > 1)) {
            printf ("bad input: d|v <index 0..3> <bitno 0..15> <value 0..1>\n");
        } else {
            lib->readi2c (dirs, vals, true);
            if (which == 'd') dirs[index] = (dirs[index] & ~ (1U << bitno)) | (value << bitno);
            if (which == 'v') vals[index] = (vals[index] & ~ (1U << bitno)) | (value << bitno);
            lib->writei2c (dirs, vals);
        }
    }
    lib->relall ();
    return 0;
}
