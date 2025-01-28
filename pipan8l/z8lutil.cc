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
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#include "z8ldefs.h"
#include "z8lutil.h"

uint32_t Z8LPage::mypid = getpid ();

Z8LPage::Z8LPage ()
{
    extmemptr = NULL;
    zynqpage = NULL;
    zynqptr = NULL;

    cmemat = NULL;
    xmemat = NULL;
    extmemat = NULL;

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
    if (zynqptr != NULL) munmap (zynqptr, 4096);
    if (extmemptr != NULL) munmap (extmemptr, 0x20000);
    close (zynqfd);
    zynqpage = NULL;
    zynqptr = NULL;
    extmemptr = NULL;
    zynqfd = -1;
    cmemat = NULL;
    xmemat = NULL;
    extmemat = NULL;
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
//   returns pointer to dev
uint32_t volatile *Z8LPage::findev (char const *id, bool (*entry) (void *param, uint32_t volatile *dev), void *param, bool lockit, bool killit)
{
    for (int idx = 0; idx < 1024;) {
        uint32_t volatile *dev = &zynqpage[idx];
        int len = 2 << ((*dev >> 12) & 15);
        if (idx + len > 1024) break;
        if ((id == NULL) || (((*dev >> 24) == (uint8_t) id[0]) && (((*dev >> 16) & 255) == (uint8_t) id[1]))) {
            if ((entry == NULL) || entry (param, dev)) {
                if (lockit) {
                    struct flock flockit;
                    int kills = killit ? 3 : 0;
                trylk:;
                    memset (&flockit, 0, sizeof flockit);
                    flockit.l_type   = F_WRLCK;
                    flockit.l_whence = SEEK_SET;
                    flockit.l_start  = idx * sizeof zynqpage[0];
                    flockit.l_len    = len * sizeof zynqpage[0];
                    if (fcntl (zynqfd, F_SETLK, &flockit) < 0) {
                        if (((errno == EACCES) || (errno == EAGAIN)) && (fcntl (zynqfd, F_GETLK, &flockit) >= 0)) {
                            if (flockit.l_type == F_UNLCK) goto trylk;
                            fprintf (stderr, "Z8LPage::findev: %c%c locked by pid %d\n",
                                *dev >> 24, *dev >> 16, (int) flockit.l_pid);
                            if (-- kills >= 0) {
                                fprintf (stderr, "Z8LPage::findev: killing pid %d\n", (int) flockit.l_pid);
                                kill ((int) flockit.l_pid, SIGTERM);
                                usleep (100000);
                                goto trylk;
                            }
                        } else {
                            fprintf (stderr, "Z8LPage::findev: error locking %c%c: %m\n", *dev >> 24, *dev >> 16);
                        }
                        ABORT ();
                    }
                }
                return dev;
            }
        }
        idx += len;
    }
    if (entry == NULL) {
        fprintf (stderr, "Z8LPage::findev: cannot find %s\n", id);
        ABORT ();
    }
    entry (param, NULL);
    return NULL;
}

// get a pointer to the 32K-word memory chip in the FPGA
// this mapping is created by extmemmap.v
uint32_t volatile *Z8LPage::extmem ()
{
    if (extmemptr == NULL) {
        extmemptr = mmap (NULL, 0x20000, PROT_READ | PROT_WRITE, MAP_SHARED, zynqfd, 0x20000);
        if (extmemptr == MAP_FAILED) {
            fprintf (stderr, "Z8LPage::extmem: error mmapping /proc/zynqpdp8l: %m\n");
            ABORT ();
        }
    }
    return (uint32_t volatile *) extmemptr;
}

#define CMWAIT(pred) do {                                               \
    uint32_t started = 0;                                               \
    while (true) {                                                      \
        int i;                                                          \
        for (i = 1000000; -- i >= 0;) if (pred) break;                  \
        if (i >= 0) break;                                              \
        uint32_t now = time (NULL);                                     \
        if (started == 0) started = now;                                \
        else if (now - started > 1) {                                   \
            fprintf (stderr, "z8lpage: cmem controller is stuck\n");    \
            ABORT ();                                                   \
        }                                                               \
    }                                                                   \
} while (false)

// do a dma cycle
//  input:
//     cm<CM_ADDR>  = 15-bit address
//     cm<CM_WRITE> = do a write access (else it's a read)
//     cm<CM_DATA>  = write data
//     cm<CM_ENAB>  = enable access
//   cm2<CM2_CAINC> = increment address of a 3-cycle
//   cm2<CM2_3CYCL> = do a 3-cycle
//  output:
//     cm<CM_DATA>  = read data
//     cm<CM_WCOVF> = wordcount overflow in 3-cycle
uint32_t Z8LPage::dmacycle (uint32_t cm, uint32_t cm2)
{
    if (cmemat == NULL) cmemat = findev ("CM", NULL, NULL, false);

    cmlock ();
    CMWAIT (! (cmemat[1] & CM_BUSY));
    cmemat[2] = cm2;
    cmemat[1] = cm;
    CMWAIT ((cm = cmemat[1]) & CM_DONE);
    cmunlk ();
    return cm;
}

// make sure last write has completed
void Z8LPage::dmaflush ()
{
    cmlock ();
    CMWAIT (! (cmemat[1] & CM_BUSY));
    cmunlk ();
}

// get exclusive access (co-operative) to pdp8lcmem.v device by the calling process
void Z8LPage::cmlock ()
{
    if (cmemat == NULL) cmemat = findev ("CM", NULL, NULL, false);

    uint32_t started = 0;
    while (true) {
        uint32_t lkpid;
        for (int i = 1000000; -- i >= 0;) {
            cmemat[3] = mypid;
            lkpid = cmemat[3];
            if (lkpid == mypid) return;
        }
        uint32_t now = time (NULL);
        if (started == 0) started = now;
        else if (now - started > 1) {
            fprintf (stderr, "z8lpage: cmem controller is stuckn");
            ABORT ();
        }
        if ((lkpid != 0) && (kill (lkpid, 0) < 0) && (errno == ESRCH)) {
            fprintf (stderr, "Z8LPage::cmlock: pid %u died, releasing lock\n", lkpid);
            cmemat[3] = lkpid;
        }
    }
}

// release exclusive access to pdp8lcmem.v device by the calling process
void Z8LPage::cmunlk ()
{
    ASSERT (cmemat != NULL);
    uint32_t lkpid = cmemat[3];
    ASSERT (lkpid == mypid);
    cmemat[3] = lkpid;
}

// format shadow string
char *formatshadow (uint32_t volatile *shat)
{
    static char const *const msstr[] = { "--", "F ", "D ", "E ", "WC", "CA", "BR", "IA",
                                         "ST", "09", "10", "11", "12", "13", "14", "15" };

    uint32_t sh1 = shat[1];
    uint32_t sh2 = shat[2];
    uint32_t sh3 = shat[3];
    uint32_t sh4 = shat[4];

    char iregstr[5], pctrstr[5], mbufstr[5], madrstr[5], acumstr[5], eadrstr[5];
    sprintf (iregstr, "%04o", (sh2 & SH2_IREG) / (SH2_IREG & - SH2_IREG));
    sprintf (pctrstr, "%04o", (sh2 & SH2_PCTR) / (SH2_PCTR & - SH2_PCTR));
    sprintf (mbufstr, "%04o", (sh3 & SH3_MBUF) / (SH3_MBUF & - SH3_MBUF));
    sprintf (madrstr, "%04o", (sh3 & SH3_MADR) / (SH3_MADR & - SH3_MADR));
    sprintf (acumstr, "%04o", (sh4 & SH4_ACUM) / (SH4_ACUM & - SH4_ACUM));
    sprintf (eadrstr, "%04o", (sh4 & SH4_EADR) / (SH4_EADR & - SH4_EADR));

    char *buf;
    int rc = asprintf (&buf,
        "err=%04X"
        " foe=%o"
        " ms=%s"
        " ts=%u+%2u"
        " ir=%s"
        " pc=%s"
        " mb=%s"
        " ma=%s"
        " l.ac=%c.%s"
        " ea=%s",

        (sh1 & SH_ERROR) / (SH_ERROR & - SH_ERROR),
        (sh1 & SH_FRZONERR) / SH_FRZONERR,
        msstr[(sh2&SH2_MAJSTATE)/(SH2_MAJSTATE&-SH2_MAJSTATE)],
        (sh2 & SH2_TIMESTATE) / (SH2_TIMESTATE & - SH2_TIMESTATE),
                (sh3 & SH3_TIMEDELAY) / (SH3_TIMEDELAY & - SH3_TIMEDELAY),
        (sh1 & SH_IRKNOWN) ? iregstr : "----",
        (sh1 & SH_PCKNOWN) ? pctrstr : "----",
        (sh1 & SH_MBKNOWN) ? mbufstr : "----",
        (sh1 & SH_MAKNOWN) ? madrstr : "----",
        (sh1 & SH_LNKNOWN) ? '0' + (sh4 & SH4_LINK) / SH4_LINK : '-',
                (sh1 & SH_ACKNOWN) ? acumstr : "----",
        (sh1 & SH_EAKNOWN) ? eadrstr : "----");

    ASSERT (rc > 0);
    return buf;
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
