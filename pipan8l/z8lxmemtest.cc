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

// Test the pdp8lxmem.v module
// - just writes random numbers to the 32K ram and reads them back and verifies

//  ./z8lxmemtest.armv7l
//  - fpga code must be enabled for sim mode and not being reset
//    ./z8l.sh does it

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

#define KB_FLAG 0x80000000
#define KB_ENAB 0x40000000
#define PR_FLAG 0x80000000
#define PR_FULL 0x40000000

#define ABORT() do { fprintf (stderr, "abort() %s:%d\n", __FILE__, __LINE__); abort (); } while (0)

int main (int argc, char **argv)
{
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

    uint32_t volatile *xmemat = NULL;
    for (int i = 0; i < 1024;) {
        xmemat = &zynqpage[i];
        uint32_t desc = *xmemat;
        if ((desc & 0xFFFF0000U) == (('X' << 24) | ('M' << 16))) goto found;
        i += 2 << ((desc & 0xF000) >> 12);
    }
    fprintf (stderr, "xmem not found\n");
    return 1;
found:;
    printf ("VERSION=%08X\n", xmemat[0]);

    while (true) {
        uint16_t wdata = 0;
        for (int i = 0; i < 0x8000; i ++) {
            wdata = (wdata + 03333) & 07777;
            xmemat[1] = (wdata << 16) | 0x8000 | (i & 0x7FFF);
            while (xmemat[1] & 0x10000000) { }
        }
        wdata = 0;
        for (int i = 0; i < 0x8000; i ++) {
            wdata = (wdata + 03333) & 07777;
            xmemat[1] = i & 0x7FFF;
            while (xmemat[1] & 0x10000000) { }
            uint16_t rdata = (xmemat[1] >> 16) & 07777;
            if (rdata != wdata) printf ("error %05o was %04o should be %04o\n", i, rdata, wdata);
        }
    }

    return 0;
}
