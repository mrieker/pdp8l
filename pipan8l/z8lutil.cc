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

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "z8lutil.h"

Z8LPage::Z8LPage ()
{
    zynqpage = NULL;
    zynqptr = NULL;

    zynqfd = open ("/proc/zynqpdp8l", O_RDWR);
    if (zynqfd < 0) {
        fprintf (stderr, "Z8LPage::Z8LPage: error opening /proc/zynqpdp8l: %m\n");
        ABORT ();
    }

    zynqptr = mmap (NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, zynqfd, 0);
    if (zynqptr == MAP_FAILED) {
        fprintf (stderr, "Z8LPage::Z8LPage: error mmapping /proc/zynqpdp8l: %m\n");
        ABORT ();
    }

    zynqpage = (uint32_t volatile *) zynqptr;
}

Z8LPage::~Z8LPage ()
{
    munmap (zynqptr, 4096);
    close (zynqfd);
    zynqpage = NULL;
    zynqptr = NULL;
    zynqfd = -1;
}

// find a device in the Z8L page
//  input:
//   id = NULL: all entries passed to entry() for checking
//        else: two-char string ident to check for
//   entry = NULL: return first dev that matches id
//           else: call this func to match dev
//   param = passed to entry()
//   lock = true: lock access to dev when found
//         false: don't bother locking
//  output:
//   returns NULL: dev not found
//           else: pointer to dev
uint32_t volatile *Z8LPage::findev (char const *id, bool (*entry) (void *param, uint32_t volatile *dev), void *param, bool lock)
{
    for (int idx = 0; idx < 1024;) {
        uint32_t volatile *dev = &zynqpage[idx];
        int len = 2 << ((*dev >> 12) & 15);
        if (idx + len > 1024) break;
        if ((id == NULL) || (((*dev >> 24) == (uint8_t) id[0]) && (((*dev >> 16) & 255) == (uint8_t) id[1]))) {
            if ((entry == NULL) || entry (param, dev)) {
                if (lock) {
                    struct flock flockit;
                trylk:;
                    memset (&flockit, 0, sizeof flockit);
                    flockit.l_type   = F_WRLCK;
                    flockit.l_whence = SEEK_SET;
                    flockit.l_start  = idx * sizeof zynqpage[0];
                    flockit.l_len    = len * sizeof zynqpage[0];
                    if (fcntl (zynqfd, F_SETLK, &flockit) < 0) {
                        if (((errno == EACCES) || (errno == EAGAIN)) && (fcntl (zynqfd, F_GETLK, &flockit) >= 0)) {
                            if (flockit.l_type == F_UNLCK) goto trylk;
                            fprintf (stderr, "Z8LPage::findev: locked by pid %d\n", (int) flockit.l_pid);
                        } else {
                            fprintf (stderr, "Z8LPage::findev: error locking: %m\n");
                        }
                        ABORT ();
                    }
                }
                return dev;
            }
        }
        idx += len;
    }
    return NULL;
}

// generate a random number
uint32_t randbits (int nbits)
{
    static uint64_t seed = 0x123456789ABCDEF0ULL;

    uint16_t randval = 0;

    while (-- nbits >= 0) {

        // https://www.xilinx.com/support/documentation/application_notes/xapp052.pdf
        uint64_t xnor = ~ ((seed >> 63) ^ (seed >> 62) ^ (seed >> 60) ^ (seed >> 59));
        seed = (seed << 1) | (xnor & 1);

        randval += randval + (seed & 1);
    }

    return randval;
}
