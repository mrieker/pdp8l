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

//  z8ltty    - default tty port 03
//  z8ltty 40 - tty port 40

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

#define Z_TTYVER 0
#define Z_TTYKB 1
#define Z_TTYPR 2
#define Z_TTYPN 3

#define ABORT() do { fprintf (stderr, "abort() %s:%d\n", __FILE__, __LINE__); abort (); } while (0)

int main (int argc, char **argv)
{
    uint32_t port = 3;
    if (argc > 1) port = strtoul (argv[1], NULL, 8);

    int zynqfd = open ("/proc/zynqgpio", O_RDWR);
    if (zynqfd < 0) {
        fprintf (stderr, "Z8LLib::openpads: error opening /proc/zynqgpio: %m\n");
        ABORT ();
    }

    void *zynqptr = mmap (NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, zynqfd, 0);
    if (zynqptr == MAP_FAILED) {
        fprintf (stderr, "Z8LLib::openpads: error mmapping /proc/zynqgpio: %m\n");
        ABORT ();
    }

    uint32_t volatile *zynqpage = (uint32_t volatile *) zynqptr;

    for (int i = 0; i < 1024; i += 16) {
        printf ("%03X:", i);
        for (int j = 0; j < 16; j ++) {
            printf (" %08X", zynqpage[i+j]);
        }
        printf ("\n");
    }

    uint32_t volatile *ttyat = NULL;
    for (int i = 0; i < 1024;) {
        ttyat = &zynqpage[i];
        uint32_t desc = *ttyat;
        if (((desc & 0xFFFF0000U) == (('T' << 24) | ('T' << 16))) && (ttyat[Z_TTYPN] == port)) goto found;
        i += 2 << ((desc & 0xF000) >> 12);
    }
    fprintf (stderr, "tty port %02o not found\n", port);
    return 1;
found:;

    struct termios term_modified, term_original;
    if (tcgetattr (STDIN_FILENO, &term_original) < 0) ABORT ();
    term_modified = term_original;
    cfmakeraw (&term_modified);
    if (tcsetattr (STDIN_FILENO, TCSANOW, &term_modified) < 0) ABORT ();

    printf ("use control-\\ for stop char\r\n");

    uint64_t readnextkbat = 0;
    uint64_t setprintdoneat = 0xFFFFFFFFFFFFFFFFULL;
    while (true) {
        usleep (1000);

        struct timeval nowtv;
        if (gettimeofday (&nowtv, NULL) < 0) ABORT ();
        uint64_t nowus = nowtv.tv_sec * 1000000ULL + nowtv.tv_usec;

        if (nowus >= setprintdoneat) {
            ttyat[Z_TTYPR] = 0x80000000;
            setprintdoneat = 0xFFFFFFFFFFFFFFFFULL;
        }

        if (setprintdoneat == 0xFFFFFFFFFFFFFFFFULL) {
            uint32_t prreg = ttyat[Z_TTYPR];
            if (prreg & 0x40000000) {
                uint8_t prchar = prreg & 0x7F;
                int rc = write (STDOUT_FILENO, &prchar, 1);
                if (rc <= 0) ABORT ();
                setprintdoneat = nowus + 100000;
            }
        }

        if (nowus >= readnextkbat) {
            struct pollfd polls[1] = { STDIN_FILENO, POLLIN, 0 };
            int rc = poll (polls, 1, 0);
            if ((rc < 0) && (errno != EINTR)) ABORT ();
            if ((rc >= 0) && (polls[0].revents & POLLIN)) {
                uint8_t kbchar;
                rc = read (STDIN_FILENO, &kbchar, 1);
                if (rc <= 0) ABORT ();
                if (kbchar == '\\' - '@') break;
                ttyat[Z_TTYKB] = 0x80000080 | kbchar;
                readnextkbat = nowus + 100000;
            }
        }
    }

    if (tcsetattr (STDIN_FILENO, TCSANOW, &term_original) < 0) ABORT ();
    return 0;
}
