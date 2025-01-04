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

// direct access to z8l fpga pins

#include <stdint.h>
#include <stdio.h>

#include "cmd_pin.cc"
#include "tclmain.h"

// internal TCL commands

static TclFunDef const fundefs[] = {
    { CMD_PIN },
    { NULL, NULL, NULL }
};

int main (int argc, char **argv)
{
    setlinebuf (stdout);

    int tclargs = argc;
    for (int i = 0; ++ i < argc;) {
        if (strcmp (argv[i], "-?") == 0) {
            puts ("");
            puts ("     Provide direct access to FPGA pins to/from PDP");
            puts ("");
            puts ("  ./z8lpin [<tclscriptfile> [<scriptargs>...]]");
            puts ("     <tclscriptfile> : execute script then exit");
            puts ("                else : read and process commands from stdin");
            puts ("");
            return 0;
        }
        if (argv[i][0] == '-') {
            fprintf (stderr, "unknown option %s\n", argv[i]);
            return 1;
        }
        tclargs = i;
        break;
    }

    // execute script(s)
    return tclmain (fundefs, argv[0], "z8lpin", NULL, getenv ("z8lpinini"), argc - tclargs, argv + tclargs);
}
