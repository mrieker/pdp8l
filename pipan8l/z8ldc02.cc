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

// Performs TTY I/O for the PDP-8/L Zynq DC02 terminal Multiplexor Board

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include "z8ldefs.h"
#include "z8lutil.h"

#define MAXTERMS 6

static bool upcase;
static uint16_t portnos[MAXTERMS];
static uint32_t cps = 10;
static uint32_t volatile *dcat;

static void *dc02thread (void *vindex);

int main (int argc, char **argv)
{
    bool killit = false;
    char *p;
    int nterms = 0;

    for (int i = 0; ++ i < argc;) {
        if (strcmp (argv[i], "-?") == 0) {
            puts ("");
            puts ("     Serve DC02 Terminal Multiplexor access via Telnet ports");
            puts ("");
            puts ("  ./z8ldc02 [-cps <charspersec>] [-killit] [-upcase] <telnet-port-number> ...");
            puts ("     -cps    : set chars per second, default 10");
            puts ("     -killit : kill other process that is processing this tty port");
            puts ("     -upcase : convert all keyboard to upper case");
            puts ("");
            return 0;
        }
        if (strcasecmp (argv[i], "-cps") == 0) {
            if ((++ i >= argc) || (argv[i][0] == '-')) {
                fprintf (stderr, "missing value for -cps\n");
                return 1;
            }
            cps = strtoul (argv[i], &p, 0);
            if ((*p != 0) || (cps == 0) || (cps > 1000)) {
                fprintf (stderr, "-cps value %s must be integer in range 1..1000\n", argv[i]);
                return 1;
            }
            continue;
        }
        if (strcasecmp (argv[i], "-killit") == 0) {
            killit = true;
            continue;
        }
        if (strcasecmp (argv[i], "-upcase") == 0) {
            upcase = true;
            continue;
        }
        if (argv[i][0] == '-') {
            fprintf (stderr, "unknown option %s\n", argv[i]);
            return 1;
        }

        uint32_t port = strtoul (argv[i], &p, 0);
        if ((*p != 0) || (port <= 0) || (port > 65535)) {
            fprintf (stderr, "port number %s must be integer in range 1..65535\n", argv[i]);
            return 1;
        }
        if (nterms >= MAXTERMS) {
            fprintf (stderr, "too many port numbers for %u\n", port);
            return 1;
        }
        portnos[nterms++] = port;
    }

    if (nterms == 0) {
        fprintf (stderr, "no telnet port numbers given\n");
        return 1;
    }

    Z8LPage z8p;
    dcat = z8p.findev ("DC", NULL, NULL, true, killit);

    dcat[Z_TTYKB] = 0x80000000U;    // enable board to process io instructions

    // start a thread to process all but the first terminal
    for (int i = 0; ++ i < nterms;) {
        pthread_t threadid;
        int rc = pthread_create (&threadid, NULL, dc02thread, (void *)(long)i);
        if (rc != 0) ABORT ();
    }

    // process the first terminal in this thread
    dc02thread ((void *)0);

    return 0;
}

static void *dc02thread (void *vindex)
{
    int index = (int)(long)vindex;
    ASSERT ((index >= 0) && (index < MAXTERMS));
    uint32_t volatile *dcreg = &dcat[2+index];

    // set up socket to listen for inbound telnet connections on
    uint16_t port = portnos[index];

    struct sockaddr_in servaddr;
    memset (&servaddr, 0, sizeof servaddr);
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons (port);

    int lisfd = socket (AF_INET, SOCK_STREAM, 0);
    if (lisfd < 0) {
        fprintf (stderr, "dc02thread[%d]: socket error: %m\n", index);
        ABORT ();
    }
    int one = 1;
    if (setsockopt (lisfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one) < 0) {
        fprintf (stderr, "dc02thread[%d]: SO_REUSEADDR error: %m\n", index);
        ABORT ();
    }
    if (bind (lisfd, (sockaddr *)&servaddr, sizeof servaddr) < 0) {
        fprintf (stderr, "dc02thread[%d]: bind %u error: %m\n", index, port);
        ABORT ();
    }
    if (listen (lisfd, 1) < 0) {
        fprintf (stderr, "dc02thread[%d]: listen error: %m\n", index);
        ABORT ();
    }

    while (true) {
        int one, rc, skipchars;

        fprintf (stderr, "dc02thread[%d]: listening on %u\n", index, port);

        // wait for then accept a connection
        struct sockaddr_in clinaddr;
        memset (&clinaddr, 0, sizeof clinaddr);
        socklen_t addrlen = sizeof clinaddr;
        int confd = accept (lisfd, (sockaddr *)&clinaddr, &addrlen);
        if (confd < 0) {
            fprintf (stderr, "dc02thread[%d]: accept error: %m\n", index);
            goto conclosed;
        }
        one = 1;
        if (setsockopt (confd, SOL_SOCKET, SO_KEEPALIVE, &one, sizeof one) < 0) {
            fprintf (stderr, "dc02thread[%d]: setsockopt SO_KEEPALIVE error: %m\n", index);
            goto closecon;
        }
        fprintf (stderr, "dc02thread[%d]: accepted from %s:%d\n",
            index, inet_ntoa (clinaddr.sin_addr), ntohs (clinaddr.sin_port));

        // ref: http://support.microsoft.com/kb/231866
        static uint8_t const initmodes[] = {
            255, 251, 0,        // IAC WILL TRANSMIT-BINARY
            255, 251, 1,        // IAC WILL ECHO
            255, 251, 3,        // IAC WILL SUPPRESS-GO-AHEAD
            255, 253, 0,        // IAC DO   TRANSMIT-BINARY
            255, 253, 1,        // IAC DO   ECHO
            255, 253, 3,        // IAC DO   SUPPRESS-GO-AHEAD
        };

        // tell client we do full duplex binary
        rc = write (confd, initmodes, sizeof initmodes);
        if (rc < 0) {
            fprintf (stderr, "dc02thread[%d]: write error: %m\n", index);
            goto closecon;
        }
        if (rc != sizeof initmodes) {
            fprintf (stderr, "dc02thread[%d]: only wrote %d of %d chars\n", index, rc, (int) sizeof initmodes);
            goto closecon;
        }

        skipchars = 0;

        while (true) {
            usleep (1000000 / cps);

            // see if keyboard character ready to be read from network
            struct pollfd polls[1] = { confd, POLLIN, 0 };
            rc = poll (polls, 1, 0);
            if ((rc < 0) && (errno != EINTR)) ABORT ();
            if ((rc > 0) && (polls[0].revents & POLLIN)) {

                // read char from network
                uint8_t kbchar;
                rc = read (confd, &kbchar, 1);
                if (rc != 1) {
                    if (rc < 0) fprintf (stderr, "dc02thread[%d]: error reading from link: %m\n", index);
                    else fprintf (stderr, "dc02thread[%d]: eof reading from link\n", index);
                    goto closecon;
                }

                // discard telnet 'IAC x x' sequences
                if (kbchar == 255) skipchars = 3;
                if ((skipchars < 0) || (-- skipchars < 0)) {

                    if (upcase && (kbchar >= 'a') && (kbchar <= 'z')) kbchar -= 'a' - 'A';

                    // overwrite kbchar, set kbflag = 1
                    *dcreg = 0x91000080U | kbchar;
                }
            }

            // see if PDP has posted a char to be printed
            uint32_t prreg = *dcreg;
            if (prreg & 0x20000000U) {

                // write char to network
                uint8_t prchar = (prreg >> 12) & 0x7F;
                rc = write (confd, &prchar, 1);
                if (rc != 1) {
                    if (rc < 0) fprintf (stderr, "dc02thread[%d]: error writing to link: %m\n", index);
                    else fprintf (stderr, "dc02thread[%d]: only wrote %d of 1 byte\n", index, rc);
                    goto closecon;
                }

                // set prflag=1, prfull=0
                *dcreg = 0x4C000000;
            }
        }

    closecon:;
        fprintf (stderr, "dc02thread[%d]: closing connection\n", index);
        close (confd);
    conclosed:;
        sleep (3);
    }
}
