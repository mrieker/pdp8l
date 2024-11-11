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

// Performs TTY I/O for the PDP-8/L Zynq I/O board

//  ./z8ltty [-cps <charspersec>] [<octalportnumber>]
//     charspersec defaults to 10
//     octalportnumber defaults to 03

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <termios.h>
#include <unistd.h>

#include "z8ldefs.h"
#include "z8lutil.h"

static bool findtt (void *param, uint32_t volatile *ttyat)
{
    uint32_t port = *(uint32_t *) param;
    return (ttyat[Z_TTYPN] & 077) == port;
}

int main (int argc, char **argv)
{
    bool killit = false;
    uint32_t port = 3;
    uint32_t cps = 10;
    char *p;
    for (int i = 0; ++ i < argc;) {
        if (strcasecmp (argv[i], "-cps") == 0) {
            if ((++ i >= argc) || (argv[i][0] == '-')) {
                fprintf (stderr, "missing value for -cps\n");
                return 1;
            }
            cps = strtoul (argv[i], &p, 0);
            if ((*p != 0) || (cps == 0)) {
                fprintf (stderr, "-cps value %s must be positive integer\n", argv[i]);
                return 1;
            }
            continue;
        }
        if (strcasecmp (argv[i], "-killit") == 0) {
            killit = true;
            continue;
        }
        if (argv[i][0] == '-') {
            fprintf (stderr, "unknown option %s\n", argv[i]);
            return 1;
        }
        port = strtoul (argv[i], &p, 8);
        if ((*p != 0) || (port > 076)) {
            fprintf (stderr, "port number %s must be octal integer in range 0..076\n", argv[i]);
            return 1;
        }
    }

    Z8LPage z8p;
    uint32_t volatile *ttyat = z8p.findev ("TT", findtt, &port, true, killit);
    if (ttyat == NULL) {
        fprintf (stderr, "tty port %02o not found\n", port);
        return 1;
    }

    struct termios term_modified, term_original;
    bool stdintty = isatty (STDIN_FILENO) > 0;
    if (stdintty) {
        if (tcgetattr (STDIN_FILENO, &term_original) < 0) ABORT ();
        term_modified = term_original;
        cfmakeraw (&term_modified);
        if (tcsetattr (STDIN_FILENO, TCSANOW, &term_modified) < 0) ABORT ();
        fprintf (stderr, "z8ltty: use control-\\ for stop char\r\n");
    }

    ttyat[Z_TTYKB] = KB_ENAB;   // enable board to process io instructions

    bool stdoutty = isatty (STDOUT_FILENO) > 0;
    uint64_t readnextkbat   = 0;
    uint64_t setprintdoneat = 0xFFFFFFFFFFFFFFFFULL;
    while (true) {
        usleep (1000);

        struct timeval nowtv;
        if (gettimeofday (&nowtv, NULL) < 0) ABORT ();
        uint64_t nowus = nowtv.tv_sec * 1000000ULL + nowtv.tv_usec;

        if (nowus >= setprintdoneat) {
            ttyat[Z_TTYPR] = PR_FLAG;
            setprintdoneat = 0xFFFFFFFFFFFFFFFFULL;
        }

        if (setprintdoneat == 0xFFFFFFFFFFFFFFFFULL) {
            uint32_t prreg = ttyat[Z_TTYPR];
            if (prreg & PR_FULL) {
                uint8_t prchar = prreg & 0177;
                if ((prchar == 7) && stdoutty) {
                    int rc = write (STDOUT_FILENO, "<BEL>", 5);
                    if (rc < 5) ABORT ();
                } else {
                    int rc = write (STDOUT_FILENO, &prchar, 1);
                    if (rc <= 0) ABORT ();
                }
                setprintdoneat = nowus + 1000000 / cps;
            }
        }

        if (nowus >= readnextkbat) {
            struct pollfd polls[1] = { STDIN_FILENO, POLLIN, 0 };
            int rc = poll (polls, 1, 0);
            if ((rc < 0) && (errno != EINTR)) ABORT ();
            if ((rc >= 0) && (polls[0].revents & POLLIN)) {
                uint8_t kbchar;
                rc = read (STDIN_FILENO, &kbchar, 1);
                if ((rc == 0) && ! stdintty) break;
                if (rc <= 0) ABORT ();
                if ((kbchar == '\\' - '@') && stdintty) break;
                ttyat[Z_TTYKB] = KB_FLAG | KB_ENAB | 0200 | kbchar;
                readnextkbat = nowus + 1000000 / cps;
            }
        }
    }

    if (stdintty && (tcsetattr (STDIN_FILENO, TCSANOW, &term_original) < 0)) ABORT ();
    fprintf (stderr, "\n");

    return 0;
}
