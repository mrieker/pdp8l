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

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
static Tcl_ObjCmdProc cmd_gettod;
static Tcl_ObjCmdProc cmd_libname;
static Tcl_ObjCmdProc cmd_readchar;
static Tcl_ObjCmdProc cmd_relsw;
static Tcl_ObjCmdProc cmd_setsw;

static TclFunDef const fundefs[] = {
    { cmd_assemop,    "assemop",    "assemble instruction" },
    { cmd_disasop,    "disasop",    "disassemble instruction" },
    { cmd_getreg,     "getreg",     "get register value" },
    { cmd_getsw,      "getsw",      "get switch value" },
    { cmd_gettod,     "gettod",     "get current time in us precision" },
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



int main (int argc, char **argv)
{
    if ((argc >= 2) && (strcasecmp (argv[1], "-status") == 0)) {
        return showstatus (argc - 1, argv + 1);
    }

    setlinebuf (stdout);

    bool brk     = false;
    bool dislo4k = false;
    bool enlo4k  = false;
    bool nobrk   = false;
    bool real    = false;
    bool sim     = false;
    char const *logname = NULL;
    int tclargs = argc;
    for (int i = 0; ++ i < argc;) {
        if (strcmp (argv[i], "-?") == 0) {
            puts ("");
            puts ("  ./z8lpanel [-brk | -nobrk] [-dislo4k | -enlo4k] [-log <logfile>] [-real | -sim] [<scriptfile.tcl>]");
            puts ("     access pdp-8/l front panel");
            puts ("         -brk : use pdp-8/l break cycle hardware");
            puts ("       -nobrk : don't use pdp-8/l break cycle hdwe");
            puts ("     -dislo4k : use pdp-8/l core stack for low 4K");
            puts ("      -enlo4k : use fpga-provided ram for low 4K");
            puts ("         -log : record output to given log file");
            puts ("        -real : use real pdp-8/l");
            puts ("         -sim : simulate the pdp-8/l");
            puts ("     <scriptfile.tcl> : execute script then exit");
            puts ("                 else : read and process commands from stdin");
            puts ("");
            puts ("  ./z8lpanel -status");
            puts ("     display ascii-art front panel");
            puts ("");
            return 0;
        }
        if (strcasecmp (argv[i], "-brk") == 0) {
            brk   = true;
            nobrk = false;
            continue;
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
        if (strcasecmp (argv[i], "-nobrk") == 0) {
            brk   = false;
            nobrk = true;
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
    padlib->openpads (brk, dislo4k, enlo4k, nobrk, real, sim);

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
                puts ("    pare - memory parity error");
                puts ("    prte - memory protect error");
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
            if (strcasecmp (regname, "pare") == 0) regval = pads.light.pare;
            if (strcasecmp (regname, "prte") == 0) regval = pads.light.prte;
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

// get time of day
static int cmd_gettod (ClientData clientdata, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    struct timeval nowtv;
    if (gettimeofday (&nowtv, NULL) < 0) ABORT ();
    Tcl_SetResultF (interp, "%u.%06u", (uint32_t) nowtv.tv_sec, (uint32_t) nowtv.tv_usec);
    return TCL_OK;
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
        int fd = STDIN_FILENO;
        if (strcmp (stri, "stdin") != 0) {
            if (memcmp (stri, "file", 4) != 0) {
                Tcl_SetResultF (interp, "first argument not a file");
                return TCL_ERROR;
            }
            fd = atoi (stri + 4);
        }
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
                padlib->relall ();
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

// requires './z8lpanel -status server' running in background on zturn

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
    uint32_t fps = 0;
    uint32_t nframes = 0;
    time_t lasttime = 0;

    padlib = new I2CZLib ();
    padlib->openpads (false, false, false, false, false, false);

    setvbuf (stdout, outbuf, _IOFBF, sizeof outbuf);

    int twirly = 0;

    while (true) {
        struct timeval tvnow;
        if (gettimeofday (&tvnow, NULL) < 0) ABORT ();
        usleep (50000 - tvnow.tv_usec % 50000);

        // measure updates per second
        time_t thistime = time (NULL);
        if (lasttime < thistime) {
            if (lasttime > 0) {
                fps = nframes / (thistime - lasttime);
            }
            lasttime = thistime;
            nframes  = 0;
        }
        ++ nframes;

        // read bulbs and switches
        padlib->readpads (&pads);

        // display it
        printf (TOP EOL);
        printf ("  PDP-8/L       %s   %s %s %s   %s %s %s   %s %s %s   %s %s %s  " ESC_BOLDV "%o.%04o" ESC_NORMV "  MA   IR [ %s %s %s  " ESC_ON "%s" ESC_NORMV " ]     %10u fps" EOL,
            BOOL (pads.light.ema), REG12L (pads.light.ma), pads.light.ema, pads.light.ma,
            RBITL (pads.light.ir,  9), RBITL (pads.light.ir, 10), RBITL (pads.light.ir, 11), mnes[pads.light.ir], fps);
        printf (EOL);
        printf ("                    %s %s %s   %s %s %s   %s %s %s   %s %s %s   " ESC_BOLDV " %04o" ESC_NORMV "  MB   ST [ %s %s %s %s %s %s ]" EOL,
            REG12L (pads.light.mb), pads.light.mb, STL (pads.light.fet, "F", "f"), STL (pads.light.exe, "E", "e"), STL (pads.light.def, "D", "d"),
            STL (pads.light.wct, "WC", "wc"), STL (pads.light.cad, "CA", "ca"), STL (pads.light.brk, "B", "b"));
        printf (EOL);
        printf ("                %s   %s %s %s   %s %s %s   %s %s %s   %s %s %s  " ESC_BOLDV "%o.%04o" ESC_NORMV "  AC   %s %s %s %s" EOL,
            BOOL (pads.light.link), REG12L (pads.light.ac), pads.light.link, pads.light.ac,
            STL (pads.light.ion, "ION", "ion"), STL (pads.light.pare, "PER", "per"), STL (pads.light.prte, "PRT", "prt"), STL (pads.light.run, "RUN", "run"));
        printf (EOL);
        printf ("  %s  %s %s   %s %s %s   %s %s %s   %s %s %s   %s %s %s   " ESC_BOLDV " %04o" ESC_NORMV "  SR   %s  %s %s %s %s %s  %s" EOL,
            STL (pads.togval.mprt, "MPRT", "mprt"), STL (pads.togval.dfld, "DFLD", "dfld"), STL (pads.togval.ifld, "IFLD", "ifld"), REG12L (pads.togval.sr),
            pads.togval.sr, STL (pads.button.ldad, "LDAD", "ldad"), STL (pads.button.start, "START", "start"), STL (pads.button.cont, "CONT", "cont"),
            STL (pads.button.stop, "STOP", "stop"), STL (pads.togval.step, "STEP", "step"), STL (pads.button.exam, "EXAM", "exam"), STL (pads.button.dep, "DEP", "dep"));
        printf (EOL EOP);
        printf ("  %c\r", "\\|/-"[++twirly&3]);
        fflush (stdout);
    }
}
