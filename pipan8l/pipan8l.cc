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

// access pdp-8/l front panel
// see ./pipan8l -? for options

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
#include "disassemble.h"
#include "padlib.h"
#include "pindefs.h"
#include "readprompt.h"
#include "tclmain.h"

#define ABORT() do { fprintf (stderr, "abort() %s:%d\n", __FILE__, __LINE__); abort (); } while (0)
#define ASSERT(cond) do { if (__builtin_constant_p (cond)) { if (!(cond)) asm volatile ("assert failure line %c0" :: "i"(__LINE__)); } else { if (!(cond)) ABORT (); } } while (0)

struct PermSw {
    char const *name;
    int nbits;
    int pinums[12];
};

struct RegBit {
    char const *name;
    int pinum;
};

// internal TCL commands
static Tcl_ObjCmdProc cmd_assemop;
static Tcl_ObjCmdProc cmd_disasop;
static Tcl_ObjCmdProc cmd_flushit;
static Tcl_ObjCmdProc cmd_getpin;
static Tcl_ObjCmdProc cmd_getreg;
static Tcl_ObjCmdProc cmd_getsw;
static Tcl_ObjCmdProc cmd_libname;
static Tcl_ObjCmdProc cmd_readchar;
static Tcl_ObjCmdProc cmd_setpin;
static Tcl_ObjCmdProc cmd_setsw;

static TclFunDef const fundefs[] = {
    { cmd_assemop,    "assemop",    "assemble instruction" },
    { cmd_disasop,    "disasop",    "disassemble instruction" },
    { cmd_flushit,    "flushit",    "flush writes / invalidate reads" },
    { cmd_getpin,     "getpin",     "get gpio pin" },
    { cmd_getreg,     "getreg",     "get register value" },
    { cmd_getsw,      "getsw",      "get switch value" },
    { cmd_libname,    "libname",    "get library name i2c,sim,z8l" },
    { cmd_readchar,   "readchar",   "read character with timeout" },
    { cmd_setpin,     "setpin",     "set gpio pin" },
    { cmd_setsw,      "setsw",      "set switch value" },
    { NULL, NULL, NULL }
};

static int const acbits[] = { P_AC00, P_AC01, P_AC02, P_AC03, P_AC04, P_AC05, P_AC06, P_AC07, P_AC08, P_AC09, P_AC10, P_AC11, -1 };
static int const irbits[] = { P_IR00, P_IR01, P_IR02, -1 };
static int const mabits[] = { P_MA00, P_MA01, P_MA02, P_MA03, P_MA04, P_MA05, P_MA06, P_MA07, P_MA08, P_MA09, P_MA10, P_MA11, -1 };
static int const mbbits[] = { P_MB00, P_MB01, P_MB02, P_MB03, P_MB04, P_MB05, P_MB06, P_MB07, P_MB08, P_MB09, P_MB10, P_MB11, -1 };
static int const srbits[] = { P_SR00, P_SR01, P_SR02, P_SR03, P_SR04, P_SR05, P_SR06, P_SR07, P_SR08, P_SR09, P_SR10, P_SR11, -1 };

static PermSw const permsws[] = {
    { "bncy",  1, P_BNCY },
    { "cont",  1, P_CONT },
    { "dep",   1, P_DEP  },
    { "dfld",  1, P_DFLD },
    { "exam",  1, P_EXAM },
    { "ifld",  1, P_IFLD },
    { "ldad",  1, P_LDAD },
    { "mprt",  1, P_MPRT },
    { "sr", 12, P_SR11, P_SR10, P_SR09, P_SR08, P_SR07, P_SR06, P_SR05, P_SR04, P_SR03, P_SR02, P_SR01, P_SR00 },
    { "start", 1, P_STRT },
    { "step",  1, P_STEP },
    { "stop",  1, P_STOP },
    {  NULL,   0, 0 }
};

static uint16_t const wrmsks[P_NU16S] = { P0_WMSK, P1_WMSK, P2_WMSK, P3_WMSK, P4_WMSK };

static bool rdpadsvalid;
static bool wrpadsdirty;
static PadLib *padlib;
static pthread_mutex_t padmutex = PTHREAD_MUTEX_INITIALIZER;
static uint16_t rdpads[P_NU16S];
static uint16_t wrpads[P_NU16S];

static uint16_t getreg (int const *pins);
static void setpin (int pin, bool set);
static bool getpin (int pin);
static void flushit ();
static int showstatus (int argc, char **argv);
static void *udpthread (void *dummy);



int main (int argc, char **argv)
{
    if ((argc >= 2) && (strcasecmp (argv[1], "-status") == 0)) {
        return showstatus (argc - 1, argv + 1);
    }

    setlinebuf (stdout);

    bool simit = false;
    bool z8lit = false;
    char const *logname = NULL;
    int tclargs = argc;
    for (int i = 0; ++ i < argc;) {
        if (strcmp (argv[i], "-?") == 0) {
            puts ("");
            puts ("  ./pipan8l [-log <logfile>] [-sim | -z8l] [<scriptfile.tcl>]");
            puts ("     access pdp-8/l front panel");
            puts ("     -log : record output to given log file");
            puts ("     -sim : access c-module simulator");
            puts ("            invoke via ./pipan8l script");
            puts ("            runs on pc, raspi, zturn");
            puts ("            only has internal tty");
            puts ("            does not need to be plugged into pdp");
            puts ("     -z8l : access zturn simulator front panel");
            puts ("            invoke via ./z8lsim script");
            puts ("            runs on zturn only");
            puts ("            has all zturn devices");
            puts ("            does not need to be plugged into pdp");
            puts ("     ...otherwise access real pdp front panel");
            puts ("            invoke via ./pipan8l script");
            puts ("            runs on raspi only");
            puts ("            has all devices that are in pdp, including zturn if plugged in");
            puts ("            must be plugged into pdp via pipan8l pcb");
            puts ("     <scriptfile.tcl> : execute script then exit");
            puts ("                 else : read and process commands from stdin");
            puts ("");
            puts ("  ./pipan8l -status [<hostname-or-ip-address-of-pipan8l>]");
            puts ("     display ascii-art front panel operated by one of the above pipan8l methods");
            puts ("     invoke via ./pipan8l script");
            puts ("     runs on pc, raspi, zturn");
            puts ("     <hostname-or-ip-address-of-pipan8l> defaults to localhost");
            puts ("");
            return 0;
        }
        if (strcasecmp (argv[i], "-log") == 0) {
            if ((++ i >= argc) || (argv[i][0] == '-')) {
                fprintf (stderr, "missing filename after -log\n");
                return 1;
            }
            logname = argv[i];
            continue;
        }
        if (strcasecmp (argv[i], "-sim") == 0) {
            simit = true;
            z8lit = false;
            continue;
        }
        if (strcasecmp (argv[i], "-z8l") == 0) {
            simit = false;
            z8lit = true;
            continue;
        }
        if (argv[i][0] == '-') {
            fprintf (stderr, "usage: %s [-log <logfilename>] [-sim | -z8l] [<tclfilename> ...]\n", argv[0]);
            return 1;
        }
        tclargs = i;
        break;
    }

    padlib = z8lit ? (PadLib *) new Z8LLib () : simit ? (PadLib *) new SimLib () : (PadLib *) new I2CLib ();
    padlib->openpads ();
    // initialize switches from existing switch states
    padlib->readpads (rdpads);
    for (int i = 0; i < P_NU16S; i ++) wrpads[i] = rdpads[i];

    // create udp server thread
    {
        pthread_t udptid;
        int rc = pthread_create (&udptid, NULL, udpthread, NULL);
        if (rc != 0) ABORT ();
    }

    // process tcl commands
    return tclmain (fundefs, argv[0], "pipan8l", logname, getenv ("pipan8lini"), argc - tclargs, argv + tclargs);
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

static int cmd_flushit (ClientData clientdata, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    switch (objc) {
        case 1: {
            if (pthread_mutex_lock (&padmutex) != 0) ABORT ();
            flushit ();
            rdpadsvalid = false;
            if (pthread_mutex_unlock (&padmutex) != 0) ABORT ();
            return TCL_OK;
        }
        case 2: {
            char const *swname = Tcl_GetString (objv[1]);
            if (strcasecmp (swname, "help") == 0) {
                puts ("");
                puts (" flushit");
                puts ("   flush pending writes to paddles");
                puts ("   invalidate cached paddle reads");
                puts ("");
                return TCL_OK;
            }
            break;
        }
    }
    Tcl_SetResult (interp, (char *) "bad number of arguments", TCL_STATIC);
    return TCL_ERROR;
}

// get gpio pin
static int cmd_getpin (ClientData clientdata, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    switch (objc) {
        case 2: {
            char const *regname = Tcl_GetString (objv[1]);
            if (strcasecmp (regname, "help") == 0) {
                puts ("");
                puts (" getpin <pinnumber>");
                puts ("   0.. 7 : U1 GPA0..7");
                puts ("   8..15 : U1 GPB0..7");
                puts ("  16..23 : U2 GPA0..7");
                puts ("  24..31 : U2 GPB0..7");
                puts ("  32..39 : U3 GPA0..7");
                puts ("  40..47 : U3 GPB0..7");
                puts ("  48..55 : U4 GPA0..7");
                puts ("  56..63 : U4 GPB0..7");
                puts ("  64..71 : U5 GPA0..7");
                puts ("  72..79 : U5 GPB0..7");
                puts ("");
                return TCL_OK;
            }
            int pinnum;
            int rc = Tcl_GetIntFromObj (interp, objv[1], &pinnum);
            if (rc != TCL_OK) return rc;
            if ((pinnum < 0) || (pinnum > 79)) {
                Tcl_SetResultF (interp, "bad pin number %d\n", pinnum);
                return TCL_ERROR;
            }
            int pinval = padlib->readpin (pinnum);
            if (pinval < 0) {
                Tcl_SetResultF (interp, "error reading pin %d\n", pinnum);
                return TCL_ERROR;
            }
            Tcl_SetObjResult (interp, Tcl_NewIntObj (pinval));
            return TCL_OK;
        }
    }
    Tcl_SetResult (interp, (char *) "bad number of arguments", TCL_STATIC);
    return TCL_ERROR;
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
            int const *regpins = NULL;
            if (strcasecmp (regname, "ac") == 0) regpins = acbits;
            if (strcasecmp (regname, "ir") == 0) regpins = irbits;
            if (strcasecmp (regname, "ma") == 0) regpins = mabits;
            if (strcasecmp (regname, "mb") == 0) regpins = mbbits;
            if (regpins != NULL) {
                if (pthread_mutex_lock (&padmutex) != 0) ABORT ();
                int regval = getreg (regpins);
                if (pthread_mutex_unlock (&padmutex) != 0) ABORT ();
                Tcl_SetObjResult (interp, Tcl_NewIntObj (regval));
                return TCL_OK;
            }
            if (strcasecmp (regname, "state") == 0) {
                char *statebuf = (char *) malloc (16);
                if (statebuf == NULL) ABORT ();
                char *p = statebuf;
                if (pthread_mutex_lock (&padmutex) != 0) ABORT ();
                if (getpin (P_FET)) { strcpy (p, "F ");  p += strlen (p); }
                if (getpin (P_EXE)) { strcpy (p, "E ");  p += strlen (p); }
                if (getpin (P_DEF)) { strcpy (p, "D ");  p += strlen (p); }
                if (getpin (P_WCT)) { strcpy (p, "WC "); p += strlen (p); }
                if (getpin (P_CAD)) { strcpy (p, "CA "); p += strlen (p); }
                if (getpin (P_BRK)) { strcpy (p, "B ");  p += strlen (p); }
                if (pthread_mutex_unlock (&padmutex) != 0) ABORT ();
                if (p > statebuf) -- p;
                *p = 0;
                Tcl_SetResult (interp, statebuf, (void (*) (char *)) free);
                return TCL_OK;
            }
            int pinum = -1;
            if (strcasecmp (regname, "ea")   == 0) pinum = P_EMA;
            if (strcasecmp (regname, "ion")  == 0) pinum = P_ION;
            if (strcasecmp (regname, "link") == 0) pinum = P_LINK;
            if (strcasecmp (regname, "par")  == 0) pinum = P_PARE;
            if (strcasecmp (regname, "prot") == 0) pinum = P_PRTE;
            if (strcasecmp (regname, "run")  == 0) pinum = P_RUN;
            if (pinum >= 0) {
                if (pthread_mutex_lock (&padmutex) != 0) ABORT ();
                int regval = getpin (pinum);
                if (pthread_mutex_unlock (&padmutex) != 0) ABORT ();
                Tcl_SetObjResult (interp, Tcl_NewIntObj (regval));
                return TCL_OK;
            }
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
                for (int i = 0; permsws[i].name != NULL; i ++) {
                    printf ("      %s\n", permsws[i].name);
                }
                puts ("");
                return TCL_OK;
            }
            for (int i = 0; permsws[i].name != NULL; i ++) {
                if (strcasecmp (swname, permsws[i].name) == 0) {
                    int swval = 0;
                    if (pthread_mutex_lock (&padmutex) != 0) ABORT ();
                    for (int j = permsws[i].nbits; -- j >= 0;) {
                        swval <<= 1;
                        swval += getpin (permsws[i].pinums[j]);
                    }
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
    Tcl_SetResult (interp, (char *) padlib->libname (), TCL_STATIC);
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

// set gpio pin
static int cmd_setpin (ClientData clientdata, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    switch (objc) {
        case 3: {
            char const *regname = Tcl_GetString (objv[1]);
            if (strcasecmp (regname, "help") == 0) {
                puts ("");
                puts (" setpin <pinnumber> <boolean>");
                puts ("   0.. 7 : U1 GPA0..7");
                puts ("   8..15 : U1 GPB0..7");
                puts ("  16..23 : U2 GPA0..7");
                puts ("  24..31 : U2 GPB0..7");
                puts ("  32..39 : U3 GPA0..7");
                puts ("  40..47 : U3 GPB0..7");
                puts ("  48..55 : U4 GPA0..7");
                puts ("  56..63 : U4 GPB0..7");
                puts ("  64..71 : U5 GPA0..7");
                puts ("  72..79 : U5 GPB0..7");
                puts ("");
                return TCL_OK;
            }
            int pinnum, pinval;
            int rc = Tcl_GetIntFromObj (interp, objv[1], &pinnum);
            if (rc != TCL_OK) return rc;
            if ((pinnum < 0) || (pinnum > 79)) {
                Tcl_SetResultF (interp, "bad pin number %d\n", pinnum);
                return TCL_ERROR;
            }
            rc = Tcl_GetBooleanFromObj (interp, objv[2], &pinval);
            if (rc != TCL_OK) return rc;
            if (! ((wrmsks[pinnum/16] >> (pinnum % 16)) & 1)) {
                pinval = -1;
            } else if (padlib->writepin (pinnum, pinval) < 0) {
                Tcl_SetResultF (interp, "error writing pin %d\n", pinnum);
                return TCL_ERROR;
            }
            Tcl_SetObjResult (interp, Tcl_NewIntObj (pinval));
            return TCL_OK;
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
                for (int i = 0; permsws[i].name != NULL; i ++) {
                    printf ("      %s\n", permsws[i].name);
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
            for (int i = 0; permsws[i].name != NULL; i ++) {
                if (strcasecmp (swname, permsws[i].name) == 0) {
                    if ((swval < 0) || (swval >= 1 << permsws[i].nbits)) {
                        Tcl_SetResultF (interp, "value %d out of range for switch %s", swval, swname);
                        return TCL_ERROR;
                    }
                    if (pthread_mutex_lock (&padmutex) != 0) ABORT ();
                    for (int j = 0; j < permsws[i].nbits; j ++) {
                        setpin (permsws[i].pinums[j], swval & 1);
                        swval >>= 1;
                    }
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



/////////////////
//  Utilities  //
/////////////////

// flush writes then read register
// - call with mutex locked
static uint16_t getreg (int const *pins)
{
    uint16_t reg = 0;
    for (int pin; (pin = *(pins ++)) >= 0;) {
        reg += reg + getpin (pin);
    }
    return reg;
}

// queue write for given pin
// - call with mutex locked
static void setpin (int pin, bool set)
{
    int index = pin >> 4;
    int bitno = pin & 017;
    ASSERT ((index >= 0) && (index < P_NU16S));
    uint16_t old = wrpads[index];
    if (set) wrpads[index] |= 1U << bitno;
     else wrpads[index] &= ~ (1U << bitno);
    if (old != wrpads[index]) wrpadsdirty = true;
}

// flush writes then read pin into cache if not already there
// - call with mutex locked
static bool getpin (int pin)
{
    flushit ();
    if (! rdpadsvalid) {
        padlib->readpads (rdpads);
        rdpadsvalid = true;
    }
    int index = pin >> 4;
    int bitno = pin & 017;
    ASSERT ((index >= 0) && (index < P_NU16S));
    return (rdpads[index] >> bitno) & 1;
}

// flush writes
// if anything flushed, invalidate read cache
// - call with mutex locked
static void flushit ()
{
    if (wrpadsdirty) {
        padlib->writepads (wrpads);
        wrpadsdirty = false;
        rdpadsvalid = false;
    }
}



// continuously display what would be on the PDP-8/L front panel
// reads panel lights and switches from pipan8l via udp

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
    uint16_t ma;
    uint16_t ir;
    uint16_t mb;
    uint16_t ac;
    uint16_t sr;
    bool ea;
    bool stf;
    bool ste;
    bool std;
    bool stwc;
    bool stca;
    bool stb;
    bool link;
    bool ion;
    bool per;
    bool prot;
    bool run;
    bool mprt;
    bool dfld;
    bool ifld;
    bool ldad;
    bool start;
    bool cont;
    bool stop;
    bool step;
    bool exam;
    bool dep;
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

    // running on some system to read panel state form pipan8l.cc via udp
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
        flushit ();
        rdpadsvalid = false;
        udppkt.ma    = getreg (mabits);
        udppkt.ir    = getreg (irbits) << 9;
        udppkt.mb    = getreg (mbbits);
        udppkt.ac    = getreg (acbits);
        udppkt.sr    = getreg (srbits);
        udppkt.ea    = getpin (P_EMA);
        udppkt.stf   = getpin (P_FET);
        udppkt.ste   = getpin (P_EXE);
        udppkt.std   = getpin (P_DEF);
        udppkt.stwc  = getpin (P_WCT);
        udppkt.stca  = getpin (P_CAD);
        udppkt.stb   = getpin (P_BRK);
        udppkt.link  = getpin (P_LINK);
        udppkt.ion   = getpin (P_ION);
        udppkt.per   = getpin (P_PARE);
        udppkt.prot  = getpin (P_PRTE);
        udppkt.run   = getpin (P_RUN);
        udppkt.mprt  = getpin (P_MPRT);
        udppkt.dfld  = getpin (P_DFLD);
        udppkt.ifld  = getpin (P_IFLD);
        udppkt.ldad  = getpin (P_LDAD);
        udppkt.start = getpin (P_STRT);
        udppkt.cont  = getpin (P_CONT);
        udppkt.stop  = getpin (P_STOP);
        udppkt.step  = getpin (P_STEP);
        udppkt.exam  = getpin (P_EXAM);
        udppkt.dep   = getpin (P_DEP);
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
