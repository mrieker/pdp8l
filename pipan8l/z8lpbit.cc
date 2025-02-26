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

// Set pulse bit generator parameters

// The PDP-8/L will output a pulse for any I/O instruction that can be picked up on an AM radio.
// The music.pal program uses this fact to play music.

// The z8lpbit.v board outputs a pulse to D36-B2 pin for any I/O instruction
// that can be connected to an amplified speaker with a pullup to +5V.

// The original music.pal program assumes the I/O instruction takes 1.5uS
// but the PDP-8/L takes approx 4.2uS for an I/O instruction.

// So there is a modified music.pal program in ../music that uses a 6004 instruction
// in a modified loop to keep the timing correct.  This program enables processing of
// the 6004 opcode by pdp8lpbit.v to do an 'increment and skip if zero AC' as expected
// by the modified music.pal program.

// This program can also forward the sound pulse bit from the PDP to a sound card on a PC:
//   1) on the PC do:     ./z8lpbit -server &
//   2) on the ZTurn do:  ./z8lpbit -sound <ipaddressofpc> &
// Adjust volume on PC sound control panel.

#include <arpa/inet.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#if HASPA
#include <pulse/simple.h>
#endif
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "z8ldefs.h"
#include "z8lutil.h"

#define SOUNDRATE 8000
#define UDPPORT 13287

static uint32_t volatile *pbat;

static int soundclient (char const *hostname);
static void soundserver ();

int main (int argc, char **argv)
{
    char *p;

    if ((argc == 2) && (strcasecmp (argv[1], "-server") == 0)) {
        soundserver ();
        return 0;
    }

    bool doiszac = true;
    char const *sound = NULL;
    uint32_t width = 599;   // 6.0uS
    for (int i = 0; ++ i < argc;) {
        if (strcmp (argv[i], "-?") == 0) {
            puts ("");
            puts ("  Set pulsebit generator parameters");
            puts ("");
            puts ("    ./z8lpbit [-noiszac] [-sound <serverhostname>] [-width <width>]");
            puts ("");
            puts ("      -noiszac = disable ISZAC I/O instruction 6004");
            puts ("      -sound   = forward sound on to server (see -server)");
            puts ("      -width   = set width of pulses in 10nS units, 1..8192");
            puts ("");
            puts ("    ./z8lpbit -server");
            puts ("");
            puts ("      act as server for -sound client");
            puts ("      typically runs on PC with sound card");
            puts ("");
            return 0;
        }
        if (strcasecmp (argv[i], "-noiszac") == 0) {
            doiszac = false;
            continue;
        }
        if (strcasecmp (argv[i], "-sound") == 0) {
            if ((++ i >= argc) || (argv[i][0] == '-')) {
                fprintf (stderr, "missing -sound argument\n");
                return 1;
            }
            sound = argv[i];
            continue;
        }
        if (strcasecmp (argv[i], "-width") == 0) {
            if ((++ i >= argc) || (argv[i][0] == '-')) {
                fprintf (stderr, "missing -width argument\n");
                return 1;
            }
            width = strtoul (argv[i], &p, 0);
            if ((*p != 0) || (width == 0) || (width > 8192)) {
                fprintf (stderr, "width %s must be integer in range 1..8192\n", argv[i]);
                return 1;
            }
            -- width;
            continue;
        }
        fprintf (stderr, "unknown argument %s\n", argv[i]);
        return 1;
    }

    Z8LPage z8p;
    pbat = z8p.findev ("PB", NULL, NULL, false);
    pbat[1] = (doiszac ? (1U << 31) : 0) | (width << 13);

    if (sound != NULL) {
        return soundclient (sound);
    }

    return 0;
}

//////////////////////////////////////////////
//  Send sound to server given by hostname  //
//  This program runs on the Zynq card      //
//////////////////////////////////////////////

#define NSOUNDBYTES 32

static int soundclient (char const *hostname)
{
    struct sockaddr_in server;
    memset (&server, 0, sizeof server);
    server.sin_family = AF_INET;
    server.sin_port   = htons (UDPPORT);
    if (! inet_aton (hostname, &server.sin_addr)) {
        struct hostent *he = gethostbyname (hostname);
        if (he == NULL) {
            fprintf (stderr, "bad server hostname %s\n", hostname);
            return 1;
        }
        if ((he->h_addrtype != AF_INET) || (he->h_length != 4)) {
            fprintf (stderr, "bad server ip address %s type\n", hostname);
            return 1;
        }
        server.sin_addr = *(struct in_addr *)he->h_addr;
    }

    int udpfd = socket (AF_INET, SOCK_DGRAM, 0);
    if (udpfd < 0) ABORT ();

    uint32_t samprate = 100000000 / SOUNDRATE;    // 100MHz / samples-per-sec
    uint32_t sampincr = 65535 / samprate;
    pbat[2] = ((samprate - 1) << 16) | sampincr;

    struct timespec waitfor;
    int rc = clock_gettime (CLOCK_MONOTONIC, &waitfor);
    if (rc < 0) ABORT ();

    uint32_t nzeroes = 0;
    while (true) {
        uint8_t soundbytes[NSOUNDBYTES];
        for (int i = 0; i < NSOUNDBYTES / 4; i ++) {
            waitfor.tv_nsec += 1000000000 / SOUNDRATE * 4;
            if (waitfor.tv_nsec >= 1000000000) {
                waitfor.tv_sec ++;
                waitfor.tv_nsec -= 1000000000;
            }
            rc = clock_nanosleep (CLOCK_MONOTONIC, TIMER_ABSTIME, &waitfor, NULL);
            if (rc < 0) ABORT ();

            uint32_t sample = pbat[3];
            soundbytes[i*4+0] = (uint8_t) (sample >> 24);
            soundbytes[i*4+1] = (uint8_t) (sample >> 16);
            soundbytes[i*4+2] = (uint8_t) (sample >>  8);
            soundbytes[i*4+3] = (uint8_t) (sample >>  0);

            if (sample == 0) nzeroes ++;
                       else nzeroes = 0;
        }

        if (nzeroes < SOUNDRATE * 5) {
            rc = sendto (udpfd, soundbytes, sizeof soundbytes, 0, (sockaddr *) &server, sizeof server);
            if (rc != (int) sizeof soundbytes) {
                if (rc < 0) {
                    fprintf (stderr, "error sending udp packet: %m\n");
                } else {
                    fprintf (stderr, "only sent %d of %d bytes\n", rc, (int) sizeof soundbytes);
                }
                ABORT ();
            }
        }
    }
}

////////////////////////////////////////////////////////////////
//  Receive sound data sent by client and pass to pulseaudio  //
//  This program typically runs on a PC with a sound card     //
////////////////////////////////////////////////////////////////

#if HASPA
#define SOUNDRINGSIZE 1024
static pthread_cond_t soundcond1 = PTHREAD_COND_INITIALIZER;
static pthread_cond_t soundcond2 = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t soundmutex = PTHREAD_MUTEX_INITIALIZER;
static uint32_t soundinsert, soundremove, soundused;
static uint8_t soundring[SOUNDRINGSIZE];

static void *serverthread (void *dummy);
#endif

// receive audio sent by client and queue to playing thread
static void soundserver ()
{
#if HASPA
    int udpfd = socket (AF_INET, SOCK_DGRAM, 0);
    if (udpfd < 0) ABORT ();

    struct sockaddr_in server;
    memset (&server, 0, sizeof server);
    server.sin_family = AF_INET;
    server.sin_port   = htons (UDPPORT);
    if (bind (udpfd, (sockaddr *) &server, sizeof server) < 0) {
        fprintf (stderr, "soundserver: error binding to %d: %m\n", UDPPORT);
        ABORT ();
    }

    int rcvsize = SOUNDRINGSIZE;
    if (setsockopt (udpfd, SOL_SOCKET, SO_RCVBUF, &rcvsize, sizeof rcvsize) < 0) {
        fprintf (stderr, "soundserver: error setting receive buffer size: %m\n");
        ABORT ();
    }

    pthread_t threadid;
    int rc = pthread_create (&threadid, NULL, serverthread, NULL);
    if (rc != 0) ABORT ();

    pthread_mutex_lock (&soundmutex);
    while (true) {
        while (soundused >= SOUNDRINGSIZE - NSOUNDBYTES) {
            pthread_cond_wait (&soundcond2, &soundmutex);
        }
        uint32_t soundlen = SOUNDRINGSIZE - soundused;
        pthread_mutex_unlock (&soundmutex);

        if (soundinsert + soundlen > SOUNDRINGSIZE) soundlen = SOUNDRINGSIZE - soundinsert;

        int rc = read (udpfd, &soundring[soundinsert], soundlen);
        if (rc < 0) {
            fprintf (stderr, "soundserver: error receiving udp packet: %m\n");
            ABORT ();
        }

        pthread_mutex_lock (&soundmutex);
        soundused += rc;
        soundinsert = (soundinsert + rc) % SOUNDRINGSIZE;
        pthread_cond_broadcast (&soundcond1);
    }
}

// dequeue audio received and pass to pulseaudio
static void *serverthread (void *dummy)
{
    pa_simple *s;
    pa_sample_spec ss;

    memset (&ss, 0, sizeof ss);
    ss.format = PA_SAMPLE_U8;
    ss.channels = 1;
    ss.rate = SOUNDRATE;

    s = pa_simple_new(NULL,               // Use the default server.
                      "z8lpbit",          // Our application's name.
                      PA_STREAM_PLAYBACK,
                      NULL,               // Use the default device.
                      "PDP-8/L Pulse",    // Description of our stream.
                      &ss,                // Our sample format.
                      NULL,               // Use default channel map
                      NULL,               // Use default buffering attributes.
                      NULL                // Ignore error code.
                      );
    if (s == NULL) {
        fprintf (stderr, "soundserver: error accessing pulseaudio\n");
        ABORT ();
    }

    pthread_mutex_lock (&soundmutex);
    while (true) {
        uint32_t soundlen;
        while ((soundlen = soundused) == 0) {
            pthread_cond_wait (&soundcond1, &soundmutex);
        }
        pthread_mutex_unlock (&soundmutex);

        if (soundremove + soundlen > SOUNDRINGSIZE) soundlen = SOUNDRINGSIZE - soundremove;

        int error;
        if (pa_simple_write (s, &soundring[soundremove], soundlen, &error) < 0) {
            fprintf (stderr, "soundserver: error %d sending sound\n", error);
            ABORT ();
        }

        pthread_mutex_lock (&soundmutex);
        soundused -= soundlen;
        soundremove = (soundremove + soundlen) % SOUNDRINGSIZE;
        pthread_cond_broadcast (&soundcond2);
    }
#else
    fprintf (stderr, "soundserver: install pulseaudio (apt install libpulse-dev) then re-compile\n");
    ABORT ();
#endif
}
