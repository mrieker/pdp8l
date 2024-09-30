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

// continuously display what would be on PDP-8/L front panel
// reads panel lights and switches from pipan8l via udp

//  ./ttpan8l [<ipaddress-of-pipan8l>]

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "udppkt.h"

#define ABORT() do { fprintf (stderr, "abort() %s:%d\n", __FILE__, __LINE__); abort (); } while (0)
#define ASSERT(cond) do { if (__builtin_constant_p (cond)) { if (!(cond)) asm volatile ("assert failure line %c0" :: "i"(__LINE__)); } else { if (!(cond)) ABORT (); } } while (0)

#define ESC_NORMV "\033[m"             /* go back to normal video */
#define ESC_REVER "\033[7m"            /* turn reverse video on */
#define ESC_UNDER "\033[4m"            /* turn underlining on */
#define ESC_BLINK "\033[5m"            /* turn blink on */
#define ESC_BOLDV "\033[1m"            /* turn bold on */
#define ESC_REDBG "\033[41m"           /* red background */
#define ESC_GRNBG "\033[42m"           /* green background */
#define ESC_YELBG "\033[44m"           /* yellow background */
#define ESC_BLKFG "\033[30m"           /* black foreground */
#define ESC_REDFG "\033[91m"           /* red foreground */
#define ESC_EREOL "\033[K"             /* erase to end of line */
#define ESC_EREOP "\033[J"             /* erase to end of page */
#define ESC_HOME  "\033[H"             /* home cursor */

#define ESC_ON ESC_BOLDV ESC_GRNBG ESC_BLKFG

#define RBITL(r,n) BOOL ((r >> (11 - n)) & 1)
#define REG12L(r) RBITL (r, 0), RBITL (r, 1), RBITL (r, 2), RBITL (r, 3), RBITL (r, 4), RBITL (r, 5), RBITL (r, 6), RBITL (r, 7), RBITL (r, 8), RBITL (r, 9), RBITL (r, 10), RBITL (r, 11)
#define STL(b,on,off) (b ? (ESC_ON on ESC_NORMV) : off)
#define BOOL(b) STL (b, "*", "-")

#define TOP ESC_HOME
#define EOL ESC_EREOL "\n"
#define EOP ESC_EREOP

static char const *const mnes[] = { "AND", "TAD", "ISZ", "DCA", "JMS", "JMP", "IOT", "OPR" };

static char outbuf[4096];

int main (int argc, char **argv)
{
    char const *ipaddr = "127.0.0.1";
    if (argc == 2) ipaddr = argv[1];
    else if (argc != 1) {
        fprintf (stderr, "usage: ttpan8l [<ipaddress-of-pipan8l>]\n");
        return 1;
    }

    struct sockaddr_in server;
    memset (&server, 0, sizeof server);
    server.sin_family = AF_INET;
    server.sin_port   = htons (UDPPORT);
    if (! inet_aton (ipaddr, &server.sin_addr)) {
        struct hostent *he = gethostbyname (ipaddr);
        if (he == NULL) {
            fprintf (stderr, "bad server ip address %s\n", ipaddr);
            return 1;
        }
        if ((he->h_addrtype != AF_INET) || (he->h_length != 4)) {
            fprintf (stderr, "bad server ip address %s type\n", ipaddr);
            return 1;
        }
        server.sin_addr = *(struct in_addr *)he->h_addr;
    }

    setvbuf (stdout, outbuf, _IOFBF, sizeof outbuf);

    int udpfd = socket (AF_INET, SOCK_DGRAM, 0);
    if (udpfd < 0) ABORT ();

    struct sockaddr_in client;
    memset (&client, 0, sizeof client);
    client.sin_family = AF_INET;
    if (bind (udpfd, (sockaddr *) &client, sizeof client) < 0) {
        fprintf (stderr, "error binding: %m\n");
        return 1;
    }

    struct timeval timeout;
    memset (&timeout, 0, sizeof timeout);
    timeout.tv_usec = 100000;
    if (setsockopt (udpfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout) < 0) ABORT ();

    UDPPkt udppkt;
    memset (&udppkt, 0, sizeof udppkt);

    uint32_t fps = 0;
    time_t lasttime = 0;
    uint64_t lastseq = 0;
    uint64_t seq = 0;

    while (true) {

        // ping the server so it sends something out
    ping:;
        udppkt.seq = ++ seq;
        int rc = sendto (udpfd, &udppkt, sizeof udppkt, 0, (sockaddr *) &server, sizeof server);
        if (rc != sizeof udppkt) {
            if (rc < 0) {
                fprintf (stderr, "error sending udp packet: %m\n");
            } else {
                fprintf (stderr, "only sent %d of %d bytes\n", rc, (int) sizeof udppkt);
            }
            return 1;
        }

        // read state from server
        do {
            rc = read (udpfd, &udppkt, sizeof udppkt);
            if (rc != sizeof udppkt) {
                if (rc < 0) {
                    if (errno == EAGAIN) goto ping;
                    fprintf (stderr, "error receiving udp packet: %m\n");
                } else {
                    fprintf (stderr, "only received %d of %d bytes\n", rc, (int) sizeof udppkt);
                }
                return 1;
            }
        } while (udppkt.seq < seq);
        if (udppkt.seq > seq) {
            fprintf (stderr, "bad seq rcvd %llu, sent %llu\n", (long long unsigned) udppkt.seq, (long long unsigned) seq);
            return 1;
        }

        // measure updates per second
        time_t thistime = time (NULL);
        if (lasttime < thistime) {
            if (lasttime > 0) {
                fps = (seq - lastseq) / (thistime - lasttime);
            }
            lastseq = seq;
            lasttime = thistime;
        }

        // display it
        printf (TOP EOL);
        printf ("  PDP-8/L       %s   %s %s %s   %s %s %s   %s %s %s   %s %s %s  " ESC_BOLDV "%o.%04o" ESC_NORMV "  MA   IR [ %s %s %s  " ESC_ON "%s" ESC_NORMV " ]     %10u fps" EOL,
            BOOL (udppkt.ea), REG12L (udppkt.ma), udppkt.ea, udppkt.ma,
            RBITL (udppkt.ir, 0), RBITL (udppkt.ir, 1), RBITL (udppkt.ir, 2), mnes[(udppkt.ir>>9)&7], fps);
        printf (EOL);
        printf ("                    %s %s %s   %s %s %s   %s %s %s   %s %s %s   " ESC_BOLDV " %04o" ESC_NORMV "  MB   ST [ %s %s %s %s %s %s ]" EOL,
            REG12L (udppkt.mb), udppkt.mb, STL (udppkt.stf, "F", "f"), STL (udppkt.ste, "E", "e"), STL (udppkt.std, "D", "d"),
            STL (udppkt.stwc, "WC", "wc"), STL (udppkt.stca, "CA", "ca"), STL (udppkt.stb, "B", "b"));
        printf (EOL);
        printf ("                %s   %s %s %s   %s %s %s   %s %s %s   %s %s %s  " ESC_BOLDV "%o.%04o" ESC_NORMV "  AC   %s %s %s %s" EOL,
            BOOL (udppkt.link), REG12L (udppkt.ac), udppkt.link, udppkt.ac,
            STL (udppkt.ion, "ION", "ion"), STL (udppkt.per, "PER", "per"), STL (udppkt.prot, "PRT", "prt"), STL (udppkt.run, "RUN", "run"));
        printf (EOL);
        printf ("  %s  %s %s   %s %s %s   %s %s %s   %s %s %s   %s %s %s   " ESC_BOLDV " %04o" ESC_NORMV "  SR   %s  %s %s %s %s %s  %s" EOL,
            STL (udppkt.mprt, "MPRT", "mprt"), STL (udppkt.dfld, "DFLD", "dfld"), STL (udppkt.ifld, "IFLD", "ifld"), REG12L (udppkt.sr),
            udppkt.sr, STL (udppkt.ldad, "LDAD", "ldad"), STL (udppkt.start, "START", "start"), STL (udppkt.cont, "CONT", "cont"),
            STL (udppkt.stop, "STOP", "stop"), STL (udppkt.step, "STEP", "step"), STL (udppkt.exam, "EXAM", "exam"), STL (udppkt.dep, "DEP", "dep"));
        printf (EOL);
        printf (EOP);
        fflush (stdout);
    }
}
