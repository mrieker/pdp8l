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

// Back external memory with a core file

#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "z8lutil.h"

#define NBYTESPERPAGE 4096
#define NCOREWORDS 32768
#define NCOREBYTES ((int)(sizeof localcopy))
#define NWORDSPERPAGE ((int)(NBYTESPERPAGE / sizeof localcopy[0]))

static bool volatile aborted;
static uint16_t localcopy[NCOREWORDS];

static void sighand (int signum);

int main (int argc, char **argv)
{
    setlinebuf (stdout);

    bool forkit = false;
    char const *corefn = NULL;
    for (int i = 0; ++ i < argc;) {
        if (strcmp (argv[i], "-?") == 0) {
            puts ("");
            puts ("     Back external memory with a core file");
            puts ("");
            puts ("  ./z8lcore [-fork] <corefile>");
            puts ("     -fork : fork as a daemon after loading corefile into external memory");
            puts ("     <corefile> : file used to save/restore external memory");
            puts ("");
            return 0;
        }
        if (strcasecmp (argv[i], "-fork") == 0) {
            forkit = true;
            continue;
        }
        if ((argv[i][0] == '-') || (corefn != NULL)) {
            fprintf (stderr, "unknown argument %s\n", argv[i]);
            return 1;
        }
        corefn = argv[i];
    }
    if (corefn == NULL) {
        fprintf (stderr, "missing core file name\n");
        return 1;
    }
    int corefd = open (corefn, O_RDWR | O_CREAT, 0666);
    if (corefd < 0) {
        fprintf (stderr, "error creating %s: %m\n", corefn);
        return 1;
    }
    if (ftruncate (corefd, NCOREBYTES) < 0) {
        fprintf (stderr, "error extending %s to %d bytes: %m\n", corefn, NCOREBYTES);
        return 1;
    }

    if (forkit) {
        int pid = fork ();
        if (pid < 0) {
            fprintf (stderr, "error forking: %m\n");
            return 1;
        }
        if (pid > 0) {
            printf ("%d\n", pid);
            return 0;
        }
    }

    fclose (stdout);
    stdout = NULL;

    int rc = pread (corefd, localcopy, sizeof localcopy, 0);
    if (rc < 0) {
        fprintf (stderr, "z8lcore: error reading %s: %m\n", corefn);
        return 1;
    }
    if (rc != NCOREBYTES) {
        fprintf (stderr, "z8lcore: only read %d of %d bytes in %s\n", rc, NCOREBYTES, corefn);
        return 1;
    }

    Z8LPage z8p;
    uint32_t volatile *extmem = z8p.extmem ();
    for (int i = 0; i < NCOREWORDS; i ++) {
        extmem[i] = localcopy[i];
    }

    signal (SIGHUP,  sighand);
    signal (SIGINT,  sighand);
    signal (SIGTERM, sighand);

    bool die = false;
    while (! die) {
        sleep (1);
        die = aborted;
        bool pagedirty = false;
        for (int i = 0; i < NCOREWORDS; i ++) {
            uint16_t word = extmem[i];
            if (localcopy[i] != word) {
                localcopy[i] = word;
                pagedirty = true;
            }
            if ((i % NWORDSPERPAGE == NWORDSPERPAGE - 1) && pagedirty) {
                int page = i / NWORDSPERPAGE;
                rc = pwrite (corefd, localcopy + page * NWORDSPERPAGE, NBYTESPERPAGE, page * NBYTESPERPAGE);
                if (rc != NBYTESPERPAGE) {
                    if (rc < 0) {
                        fprintf (stderr, "z8lcore: error writing %s page %d: %m\n", corefn, page);
                    } else {
                        fprintf (stderr, "z8lcore: only wrote %d of %d bytes to %s page %d\n", rc, NBYTESPERPAGE, corefn, page);
                    }
                    return 1;
                }
                pagedirty = false;
            }
        }
    }
    if (close (corefd) < 0) {
        fprintf (stderr, "z8lcore: error closing %s: %m\n", corefn);
        return 1;
    }
    fprintf (stderr, "z8lcore: exiting\n");
    return 0;
}

static void sighand (int signum)
{
    aborted = true;
}
