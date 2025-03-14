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

// Performs paper tape reader I/O for the PDP-8/L Zynq I/O board

#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "z8ldefs.h"
#include "z8lutil.h"

#define PTR_FLAG 0x80000000U // tells PDP char is ready to read
#define PTR_ENAB 0x40000000U // enables pdp8lptr.v to process i/o instructions
#define PTR_STEP 0x20000000U // tells ARM to read another char from file

int main (int argc, char **argv)
{
    bool clear = false;
    bool inscr = false;
    bool killit = false;
    char const *filename = NULL;
    uint32_t cps = 300;
    uint8_t mask = 0;
    for (int i = 0; ++ i < argc;) {
        if (strcmp (argv[i], "-?") == 0) {
            puts ("");
            puts ("     Access paper tape reader");
            puts ("");
            puts ("  ./z8lptr [-7bit] [-clear] [-cps <charspersec>] [-inscr] [-killit] [-text] <filename>");
            puts ("     -7bit   : force top bit of byte = 1");
            puts ("     -clear  : clear status bits at beginning");
            puts ("     -cps    : set chars per second, default 300");
            puts ("     -inscr  : insert <CR> before <LF>");
            puts ("     -killit : kill other process that is processing paper tape reader");
            puts ("     -text   : equivalent to -7bit -inscr");
            puts ("");
            return 0;
        }
        if (strcasecmp (argv[i], "-7bit") == 0) {
            mask = 0200;
            continue;
        }
        if (strcasecmp (argv[i], "-clear") == 0) {
            clear = true;
            continue;
        }
        if (strcasecmp (argv[i], "-cps") == 0) {
            if ((++ i >= argc) || (argv[i][0] == '-')) {
                fprintf (stderr, "missing value for -cps\n");
                return 1;
            }
            char *p;
            cps = strtoul (argv[i], &p, 0);
            if ((*p != 0) || (cps == 0) || (cps > 1000)) {
                fprintf (stderr, "-cps value %s must be integer in range 1..1000\n", argv[i]);
                return 1;
            }
            continue;
        }
        if (strcasecmp (argv[i], "-inscr") == 0) {
            inscr = true;
            continue;
        }
        if (strcasecmp (argv[i], "-killit") == 0) {
            killit = true;
            continue;
        }
        if (strcasecmp (argv[i], "-text") == 0) {
            mask = 0200;
            inscr = true;
            continue;
        }
        if (argv[i][0] == '-') {
            fprintf (stderr, "unknown option %s\n", argv[i]);
            return 1;
        }
        if (filename != NULL) {
            fprintf (stderr, "unknown argument %s\n", argv[i]);
            return 1;
        }
        filename = argv[i];
    }

    if (filename == NULL) {
        fprintf (stderr, "missing filename\n");
        return 1;
    }
    int filedes = open (filename, O_RDONLY);
    if (filedes < 0) {
        fprintf (stderr, "error opening %s: %m\n", filename);
        return 1;
    }

    struct stat statbuf;
    if (fstat (filedes, &statbuf) < 0) ABORT ();
    uint32_t fsize = statbuf.st_size;

    Z8LPage z8p;
    uint32_t volatile *ptrat = z8p.findev ("PR", NULL, NULL, true, killit);
    if (clear) {
        ptrat[1]  = PTR_ENAB;   // enable pdp8lptr.v to process io instructions
    } else {
        ptrat[1] |= PTR_ENAB;   // enable pdp8lptr.v to process io instructions
    }

    bool lastcr = false;
    uint32_t nbytes = 0;
    while (true) {
        printf ("\r%u/%u byte%s so far ", nbytes, fsize, ((nbytes == 1) ? "" : "s"));
        fflush (stdout);

        do usleep (1000000 / cps);
        while (! (ptrat[1] & PTR_STEP));
        ptrat[1] = PTR_ENAB;

        uint8_t rdbyte;
        int rc = read (filedes, &rdbyte, 1);
        if (rc < 0) {
            fprintf (stderr, "error reading %s: %m\n", filename);
            return 1;
        }
        if (rc == 0) {
            printf ("\nend of file reached\n");
            return 0;
        }

        if (((rdbyte & 0177) == '\n') && inscr && ! lastcr) {
            ptrat[1] = PTR_FLAG | PTR_ENAB | (rdbyte - '\n' + '\r') | mask;
            do usleep (1000000 / cps);
            while (! (ptrat[1] & PTR_STEP));
        }

        lastcr = (rdbyte & 0177) == '\r';

        ptrat[1] = PTR_FLAG | PTR_ENAB | rdbyte | mask;
        ++ nbytes;
    }
}
