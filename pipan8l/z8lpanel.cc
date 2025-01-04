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

// access pdp-8/l front panel via backplane piggy-back board connected to the Zynq
// see ./z8lpanel -? for options

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
#include <tcl.h>
#include <unistd.h>

#include "assemble.h"
#include "cmd_pin.h"
#include "disassemble.h"
#include "i2czlib.h"
#include "readprompt.h"
#include "tclmain.h"
#include "z8lutil.h"

// internal TCL commands
static Tcl_ObjCmdProc cmd_assemop;
static Tcl_ObjCmdProc cmd_disasop;
static Tcl_ObjCmdProc cmd_getreg;
static Tcl_ObjCmdProc cmd_getsw;
static Tcl_ObjCmdProc cmd_libname;
static Tcl_ObjCmdProc cmd_readchar;
static Tcl_ObjCmdProc cmd_relsw;
static Tcl_ObjCmdProc cmd_setsw;

static TclFunDef const fundefs[] = {
    { cmd_assemop,    "assemop",    "assemble instruction" },
    { cmd_disasop,    "disasop",    "disassemble instruction" },
    { cmd_getreg,     "getreg",     "get register value" },
    { cmd_getsw,      "getsw",      "get switch value" },
    { cmd_libname,    "libname",    "get library name i2c,sim,z8l" },
    { CMD_PIN },
    { cmd_readchar,   "readchar",   "read character with timeout" },
    { cmd_relsw,      "relsw",      "release control of switch" },
    { cmd_setsw,      "setsw",      "set switch value" },
    { NULL, NULL, NULL }
};

static Z8LPanel pads;

struct Switch {
    char const *name;
    int nbits;
    bool *bovrptr, *bvalptr;
    uint16_t *wovrptr, *wvalptr;
};

static Switch const switches[] = {
    { "cont",  1, NULL,               &pads.button.cont,  NULL, NULL },
    { "dep",   1, NULL,               &pads.button.dep,   NULL, NULL },
    { "dfld",  1, &pads.togovr.dfld,  &pads.togval.dfld,  NULL, NULL },
    { "exam",  1, NULL,               &pads.button.exam,  NULL, NULL },
    { "ifld",  1, &pads.togovr.ifld,  &pads.togval.ifld,  NULL, NULL },
    { "ldad",  1, NULL,               &pads.button.ldad,  NULL, NULL },
    { "mprt",  1, &pads.togovr.mprt,  &pads.togval.mprt,  NULL, NULL },
    { "sr",   12, NULL, NULL, &pads.togovr.sr,    &pads.togval.sr    },
    { "start", 1, NULL,               &pads.button.start, NULL, NULL },
    { "step",  1, &pads.togovr.step,  &pads.togval.step,  NULL, NULL },
    { "stop",  1, NULL,               &pads.button.stop,  NULL, NULL },
    {  NULL  }
};

static I2CZLib *padlib;
static pthread_mutex_t padmutex = PTHREAD_MUTEX_INITIALIZER;

static int showstatus (int argc, char **argv);
static void *udpthread (void *dummy);



int main (int argc, char **argv)
{
    if ((argc >= 2) && (strcasecmp (argv[1], "-status") == 0)) {
        return showstatus (argc - 1, argv + 1);
    }

    setlinebuf (stdout);

    bool dislo4k = false;
    bool enlo4k  = false;
    bool real    = false;
    bool sim     = false;
    char const *logname = NULL;
    int tclargs = argc;
    for (int i = 0; ++ i < argc;) {
        if (strcmp (argv[i], "-?") == 0) {
            puts ("");
            puts ("  ./z8lpanel [-dislo4k | -enlo4k] [-log <logfile>] [-real | -sim] [<scriptfile.tcl>]");
            puts ("     access pdp-8/l front panel");
            puts ("     -dislo4k : use pdp-8/l core stack for low 4K");
            puts ("     -enlo4k : use fpga-provided ram for low 4K");
            puts ("     -log : record output to given log file");
            puts ("     -real : use real pdp-8/l");
            puts ("     -sim : simulate the pdp-8/l");
            puts ("     <scriptfile.tcl> : execute script then exit");
            puts ("                 else : read and process commands from stdin");
            puts ("");
            puts ("  ./z8lpanel -status [<hostname-or-ip-address-of-z8lpanel>]");
            puts ("     display ascii-art front panel");
            puts ("     runs on pc, raspi, zturn");
            puts ("     <hostname-or-ip-address-of-z8lpanel> defaults to localhost");
            puts ("");
            return 0;
        }
        if (strcasecmp (argv[i], "-dislo4k") == 0) {
            dislo4k = true;
            enlo4k  = false;
            continue;
        }
        if (strcasecmp (argv[i], "-enlo4k") == 0) {
            dislo4k = false;
            enlo4k  = true;
            continue;
        }
        if (strcasecmp (argv[i], "-log") == 0) {
            if ((++ i >= argc) || (argv[i][0] == '-')) {
                fprintf (stderr, "missing filename after -log\n");
                return 1;
            }
            logname = argv[i];
            continue;
        }
        if (strcasecmp (argv[i], "-real") == 0) {
            real = true;
            sim  = false;
            continue;
        }
        if (strcasecmp (argv[i], "-sim") == 0) {
            real = false;
            sim  = true;
            continue;
        }
        if (argv[i][0] == '-') {
            fprintf (stderr, "unknown option %s\n", argv[i]);
            return 1;
        }
        tclargs = i;
        break;
    }

    padlib = new I2CZLib ();
    padlib->openpads (dislo4k, enlo4k, real, sim);

    // create udp server thread
    {
        pthread_t udptid;
        int rc = pthread_create (&udptid, NULL, udpthread, NULL);
        if (rc != 0) ABORT ();
    }

    // process tcl commands
    return tclmain (fundefs, argv[0], "z8lpanel", logname, getenv ("z8lpanelini"), argc - tclargs, argv + tclargs);
}



// assemble instruciton
//  assemop [address] opcode
static int cmd_assemop (ClientData clientdata, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    if (objc > 1) {
        int i = 1;
        char const *stri = Tcl_GetString (objv[i]);
        if (strcasecmp (stri, "help") == 0) {
            puts ("");
            puts ("  assemop [<address>] <opcode>...");
            puts ("   address = integer address of the opcode (for page-relative addresses)");
            puts ("    opcode = opcode string(s)");
            puts ("");
            return TCL_OK;
        }

        char *p;
        uint16_t address = strtoul (stri, &p, 0);
        if ((objc == 2) || (p == stri) || (*p != 0)) address = 0200;
                              else stri = Tcl_GetString (objv[++i]);

        std::string opstr;
        while (true) {
            opstr.append (stri);
            if (++ i >= objc) break;
            opstr.push_back (' ');
            stri = Tcl_GetString (objv[i]);
        }

        uint16_t opcode;
        char *err = assemble (opstr.c_str (), address, &opcode);
        if (err != NULL) {
            Tcl_SetResult (interp, err, (void (*) (char *)) free);
            return TCL_ERROR;
        }
        Tcl_SetObjResult (interp, Tcl_NewIntObj (opcode));
        return TCL_OK;
    }

    Tcl_SetResult (interp, (char *) "bad number of arguments", TCL_STATIC);
    return TCL_ERROR;
}

// disassemble instruciton
//  disasop opcode [address]
static int cmd_disasop (ClientData clientdata, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    int opcode = -1;
    int addr = 0200;

    switch (objc) {
        case 2: {
            char const *opstr = Tcl_GetString (objv[1]);
            if (strcasecmp (opstr, "help") == 0) {
                puts ("");
                puts ("  disasop <opcode> [<address>]");
                puts ("    opcode = integer opcode");
                puts ("   address = integer address of the opcode (for page-relative addresses)");
                puts ("");
                return TCL_OK;
            }
            int rc = Tcl_GetIntFromObj (interp, objv[1], &opcode);
            if (rc != TCL_OK) return rc;
            break;
        }
        case 3: {
            int rc = Tcl_GetIntFromObj (interp, objv[1], &opcode);
            if (rc == TCL_OK) rc = Tcl_GetIntFromObj (interp, objv[2], &addr);
            if (rc != TCL_OK) return rc;
            break;
        }
        default: {
            Tcl_SetResult (interp, (char *) "bad number of arguments", TCL_STATIC);
            return TCL_ERROR;
        }
    }

    std::string str = disassemble (opcode, addr);
    Tcl_SetResult (interp, strdup (str.c_str ()), (void (*) (char *)) free);
    return TCL_OK;
}

// get register (lights)
static int cmd_getreg (ClientData clientdata, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    switch (objc) {
        case 2: {
            char const *regname = Tcl_GetString (objv[1]);
            if (strcasecmp (regname, "help") == 0) {
                puts ("");
                puts (" getreg <registername>");
                puts ("   multi-bit registers:");
                puts ("      ac - accumulator");
                puts ("      ir - instruction register 0..7");
                puts ("      ma - memory address");
                puts ("      mb - memory buffer");
                puts ("   state - which of F E D WC CA B are lit");
                puts ("   single-bit registers:");
                puts ("      ea - extended address");
                puts ("    link - link bit");
                puts ("     ion - interrupts enabled");
                puts ("     par - memory parity error");
                puts ("    prot - memory protect error");
                puts ("     run - executing instructions");
                puts ("");
                return TCL_OK;
            }

            if (pthread_mutex_lock (&padmutex) != 0) ABORT ();
            padlib->readpads (&pads);

            uint16_t regval = 0xFFFFU;
            if (strcasecmp (regname, "ac")   == 0) regval = pads.light.ac;
            if (strcasecmp (regname, "ir")   == 0) regval = pads.light.ir;
            if (strcasecmp (regname, "ma")   == 0) regval = pads.light.ma;
            if (strcasecmp (regname, "mb")   == 0) regval = pads.light.mb;
            if (strcasecmp (regname, "ea")   == 0) regval = pads.light.ema;
            if (strcasecmp (regname, "ion")  == 0) regval = pads.light.ion;
            if (strcasecmp (regname, "link") == 0) regval = pads.light.link;
            if (strcasecmp (regname, "par")  == 0) regval = pads.light.pare;
            if (strcasecmp (regname, "prot") == 0) regval = pads.light.prte;
            if (strcasecmp (regname, "run")  == 0) regval = pads.light.run;
            if (regval != 0xFFFFU) {
                if (pthread_mutex_unlock (&padmutex) != 0) ABORT ();
                Tcl_SetObjResult (interp, Tcl_NewIntObj (regval));
                return TCL_OK;
            }

            if (strcasecmp (regname, "state") == 0) {
                char *statebuf = (char *) malloc (16);
                if (statebuf == NULL) ABORT ();
                char *p = statebuf;
                if (pads.light.fet) { strcpy (p, "F ");  p += strlen (p); }
                if (pads.light.exe) { strcpy (p, "E ");  p += strlen (p); }
                if (pads.light.def) { strcpy (p, "D ");  p += strlen (p); }
                if (pads.light.wct) { strcpy (p, "WC "); p += strlen (p); }
                if (pads.light.cad) { strcpy (p, "CA "); p += strlen (p); }
                if (pads.light.brk) { strcpy (p, "B ");  p += strlen (p); }
                if (pthread_mutex_unlock (&padmutex) != 0) ABORT ();
                if (p > statebuf) -- p;
                *p = 0;
                Tcl_SetResult (interp, statebuf, (void (*) (char *)) free);
                return TCL_OK;
            }

            if (pthread_mutex_unlock (&padmutex) != 0) ABORT ();
            Tcl_SetResultF (interp, "bad register name %s", regname);
            return TCL_ERROR;
        }
    }
    Tcl_SetResult (interp, (char *) "bad number of arguments", TCL_STATIC);
    return TCL_ERROR;
}

// get switch
static int cmd_getsw (ClientData clientdata, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    switch (objc) {
        case 2: {
            char const *swname = Tcl_GetString (objv[1]);
            if (strcasecmp (swname, "help") == 0) {
                puts ("");
                puts (" getsw <switchname>");
                for (Switch const *sw = switches; sw->name != NULL; sw ++) {
                    printf ("      %s\n", sw->name);
                }
                puts ("");
                return TCL_OK;
            }
            for (Switch const *sw = switches; sw->name != NULL; sw ++) {
                if (strcasecmp (swname, sw->name) == 0) {
                    uint16_t swval = 0;
                    if (pthread_mutex_lock (&padmutex) != 0) ABORT ();
                    padlib->readpads (&pads);
                    if (sw->bvalptr != NULL) swval |= *(sw->bvalptr);
                    if (sw->wvalptr != NULL) swval |= *(sw->wvalptr);
                    if (pthread_mutex_unlock (&padmutex) != 0) ABORT ();
                    Tcl_SetObjResult (interp, Tcl_NewIntObj (swval));
                    return TCL_OK;
                }
            }
            Tcl_SetResultF (interp, "bad switch name %s", swname);
            return TCL_ERROR;
        }
    }
    Tcl_SetResult (interp, (char *) "bad number of arguments", TCL_STATIC);
    return TCL_ERROR;
}

// get library name
static int cmd_libname (ClientData clientdata, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    static char libname[] = "i2cz";
    Tcl_SetResult (interp, libname, TCL_STATIC);
    return TCL_OK;
}

// read character with timeout as an integer
// returns null string if timeout, else decimal integer
//  readchar file timeoutms
static int cmd_readchar (ClientData clientdata, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    if (objc == 3) {
        char const *stri = Tcl_GetString (objv[1]);
        if (memcmp (stri, "file", 4) != 0) {
            Tcl_SetResultF (interp, "first argument not a file");
            return TCL_ERROR;
        }
        int fd = atoi (stri + 4);
        int tmoms;
        int rc = Tcl_GetIntFromObj (interp, objv[2], &tmoms);
        if (rc != TCL_OK) return rc;
        struct pollfd fds = { fd, POLLIN, 0 };
        rc = poll (&fds, 1, tmoms);
        uint8_t buff;
        if (rc > 0) rc = read (fd, &buff, 1);
        if (rc < 0) {
            if (errno == EINTR) return TCL_OK;
            Tcl_SetResultF (interp, "%m");
            return TCL_ERROR;
        }
        if (rc > 0) Tcl_SetResultF (interp, "%u", (uint32_t) buff);
        return TCL_OK;
    }

    Tcl_SetResult (interp, (char *) "bad number of arguments", TCL_STATIC);
    return TCL_ERROR;
}

// release switch
static int cmd_relsw (ClientData clientdata, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    switch (objc) {
        case 2: {
            char const *swname = Tcl_GetString (objv[1]);
            if (strcasecmp (swname, "help") == 0) {
                puts ("");
                puts (" relsw <switchname> | all");
                for (Switch const *sw = switches; sw->name != NULL; sw ++) {
                    printf ("      %s\n", sw->name);
                }
                puts ("");
                return TCL_OK;
            }
            if (strcasecmp (swname, "all") == 0) {
                if (pthread_mutex_lock (&padmutex) != 0) ABORT ();
                memset (&pads.togovr, 0, sizeof pads.togovr);
                padlib->writepads (&pads);
                if (pthread_mutex_unlock (&padmutex) != 0) ABORT ();
                return TCL_OK;
            }
            for (Switch const *sw = switches; sw->name != NULL; sw ++) {
                if (strcasecmp (swname, sw->name) == 0) {
                    uint16_t mask = (1 << sw->nbits) - 1;
                    if (pthread_mutex_lock (&padmutex) != 0) ABORT ();
                    padlib->readpads (&pads);
                    if (sw->bovrptr != NULL) *(sw->bovrptr) &= ~ mask;
                    if (sw->wovrptr != NULL) *(sw->wovrptr) &= ~ mask;
                    padlib->writepads (&pads);
                    if (pthread_mutex_unlock (&padmutex) != 0) ABORT ();
                    return TCL_OK;
                }
            }
            Tcl_SetResultF (interp, "bad switch name %s", swname);
            return TCL_ERROR;
        }
    }
    Tcl_SetResult (interp, (char *) "bad number of arguments", TCL_STATIC);
    return TCL_ERROR;
}

// set switch
static int cmd_setsw (ClientData clientdata, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    switch (objc) {
        case 2: {
            char const *swname = Tcl_GetString (objv[1]);
            if (strcasecmp (swname, "help") == 0) {
                puts ("");
                puts (" setsw <switchname> <integervalue>");
                for (Switch const *sw = switches; sw->name != NULL; sw ++) {
                    printf ("      %s\n", sw->name);
                }
                puts ("");
                return TCL_OK;
            }
            break;
        }
        case 3: {
            char const *swname = Tcl_GetString (objv[1]);
            int swval = 0;
            int rc = Tcl_GetIntFromObj (interp, objv[2], &swval);
            if (rc != TCL_OK) return rc;
            for (Switch const *sw = switches; sw->name != NULL; sw ++) {
                if (strcasecmp (swname, sw->name) == 0) {
                    uint16_t mask = (1 << sw->nbits) - 1;
                    if ((swval < 0) || (swval > mask)) {
                        Tcl_SetResultF (interp, "value %d out of range for switch %s", swval, swname);
                        return TCL_ERROR;
                    }
                    if (pthread_mutex_lock (&padmutex) != 0) ABORT ();
                    padlib->readpads (&pads);
                    if (sw->bvalptr != NULL) *(sw->bvalptr) = swval;
                    if (sw->wvalptr != NULL) *(sw->wvalptr) = swval;
                    if (sw->bovrptr != NULL) *(sw->bovrptr) = mask;
                    if (sw->wovrptr != NULL) *(sw->wovrptr) = mask;
                    padlib->writepads (&pads);
                    if (pthread_mutex_unlock (&padmutex) != 0) ABORT ();
                    return TCL_OK;
                }
            }
            Tcl_SetResultF (interp, "bad switch name %s", swname);
            return TCL_ERROR;
        }
    }
    Tcl_SetResult (interp, (char *) "bad number of arguments", TCL_STATIC);
    return TCL_ERROR;
}



// continuously display what would be on the PDP-8/L front panel
// reads panel lights and switches from z8lpanel via udp

// with -z8l, reads lights & switches from pdp8lsim.v simulator running in zynq

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

#define UDPPORT 23456

struct UDPPkt {
    uint64_t seq;
    Z8LPanel pan;
};

static char const *const mnes[] = { "AND", "TAD", "ISZ", "DCA", "JMS", "JMP", "IOT", "OPR" };

static char outbuf[4000];

static int showstatus (int argc, char **argv)
{
    char const *ipaddr = NULL;
    for (int i = 0; ++ i < argc;) {
        if (argv[i][0] == '-') {
            fprintf (stderr, "unknown option %s\n", argv[i]);
            return 1;
        }
        if (ipaddr != NULL) {
            fprintf (stderr, "unknown argument %s\n", argv[i]);
            return 1;
        }
        ipaddr = argv[i];
    }

    // running on some system to read panel state form z8lpanel.cc via udp
    if (ipaddr == NULL) ipaddr = "127.0.0.1";

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

    setvbuf (stdout, outbuf, _IOFBF, sizeof outbuf);

    while (true) {
        struct timeval tvnow;
        if (gettimeofday (&tvnow, NULL) < 0) ABORT ();
        usleep (1000 - tvnow.tv_usec % 1000);

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
            BOOL (udppkt.pan.light.ema), REG12L (udppkt.pan.light.ma), udppkt.pan.light.ema, udppkt.pan.light.ma,
            RBITL (udppkt.pan.light.ir,  9), RBITL (udppkt.pan.light.ir, 10), RBITL (udppkt.pan.light.ir, 11), mnes[udppkt.pan.light.ir], fps);
        printf (EOL);
        printf ("                    %s %s %s   %s %s %s   %s %s %s   %s %s %s   " ESC_BOLDV " %04o" ESC_NORMV "  MB   ST [ %s %s %s %s %s %s ]" EOL,
            REG12L (udppkt.pan.light.mb), udppkt.pan.light.mb, STL (udppkt.pan.light.fet, "F", "f"), STL (udppkt.pan.light.exe, "E", "e"), STL (udppkt.pan.light.def, "D", "d"),
            STL (udppkt.pan.light.wct, "WC", "wc"), STL (udppkt.pan.light.cad, "CA", "ca"), STL (udppkt.pan.light.brk, "B", "b"));
        printf (EOL);
        printf ("                %s   %s %s %s   %s %s %s   %s %s %s   %s %s %s  " ESC_BOLDV "%o.%04o" ESC_NORMV "  AC   %s %s %s %s" EOL,
            BOOL (udppkt.pan.light.link), REG12L (udppkt.pan.light.ac), udppkt.pan.light.link, udppkt.pan.light.ac,
            STL (udppkt.pan.light.ion, "ION", "ion"), STL (udppkt.pan.light.pare, "PER", "per"), STL (udppkt.pan.light.prte, "PRT", "prt"), STL (udppkt.pan.light.run, "RUN", "run"));
        printf (EOL);
        printf ("  %s  %s %s   %s %s %s   %s %s %s   %s %s %s   %s %s %s   " ESC_BOLDV " %04o" ESC_NORMV "  SR   %s  %s %s %s %s %s  %s" EOL,
            STL (udppkt.pan.togval.mprt, "MPRT", "mprt"), STL (udppkt.pan.togval.dfld, "DFLD", "dfld"), STL (udppkt.pan.togval.ifld, "IFLD", "ifld"), REG12L (udppkt.pan.togval.sr),
            udppkt.pan.togval.sr, STL (udppkt.pan.button.ldad, "LDAD", "ldad"), STL (udppkt.pan.button.start, "START", "start"), STL (udppkt.pan.button.cont, "CONT", "cont"),
            STL (udppkt.pan.button.stop, "STOP", "stop"), STL (udppkt.pan.togval.step, "STEP", "step"), STL (udppkt.pan.button.exam, "EXAM", "exam"), STL (udppkt.pan.button.dep, "DEP", "dep"));
        printf (EOL);
        printf (EOP);
        fflush (stdout);
    }
}



// pass state of processor to whoever asks via udp
static void *udpthread (void *dummy)
{
    pthread_detach (pthread_self ());

    // create a socket to listen on
    int udpfd = socket (AF_INET, SOCK_DGRAM, 0);
    if (udpfd < 0) ABORT ();

    // prevent TCL script exec commands from inheriting the fd and keeping it open
    if (fcntl (udpfd, F_SETFD, FD_CLOEXEC) < 0) ABORT ();

    struct sockaddr_in server;
    memset (&server, 0, sizeof server);
    server.sin_family = AF_INET;
    server.sin_port   = htons (UDPPORT);
    if (bind (udpfd, (sockaddr *) &server, sizeof server) < 0) {
        fprintf (stderr, "udpthread: error binding to %d: %m\n", UDPPORT);
        ABORT ();
    }

    while (true) {
        struct sockaddr_in client;
        socklen_t clilen = sizeof client;
        UDPPkt udppkt;
        int rc = recvfrom (udpfd, &udppkt, sizeof udppkt, 0, (struct sockaddr *) &client, &clilen);
        if (rc != sizeof udppkt) {
            if (rc < 0) {
                fprintf (stderr, "udpthread: error receiving udp packet: %m\n");
            } else {
                fprintf (stderr, "udpthread: only received %d of %d bytes\n", rc, (int) sizeof udppkt);
            }
            continue;
        }

        if (pthread_mutex_lock (&padmutex) != 0) ABORT ();
        padlib->readpads (&udppkt.pan);
        if (pthread_mutex_unlock (&padmutex) != 0) ABORT ();

        rc = sendto (udpfd, &udppkt, sizeof udppkt, 0, (sockaddr *) &client, sizeof client);
        if (rc != sizeof udppkt) {
            if (rc < 0) {
                fprintf (stderr, "udpthread: error sending udp packet: %m\n");
            } else {
                fprintf (stderr, "udpthread: only sent %d of %d bytes\n", rc, (int) sizeof udppkt);
            }
        }
    }
}
