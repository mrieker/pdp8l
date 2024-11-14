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

// direct access to z8l signals

//  'o' pins : simit=0 : read PDP-8/L pins
//             simit=1 : read simulator pins

//  'i' pins : simit=0 : drives PDP-8/L pin from device & arm register

//  ./z8lpin [tclscriptfile]

#include <fcntl.h>
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

#include "readprompt.h"
#include "z8ldefs.h"
#include "z8lutil.h"

// internal TCL commands
struct FunDef {
    Tcl_ObjCmdProc *func;
    char const *name;
    char const *help;
};

static Tcl_ObjCmdProc cmd_ctrlcflag;
static Tcl_ObjCmdProc cmd_help;
static Tcl_ObjCmdProc cmd_pin;

static FunDef const fundefs[] = {
    { cmd_ctrlcflag,  "ctrlcflag",  "read and clear control-C flag" },
    { cmd_help,       "help",       "print this help" },
    { cmd_pin,        "pin",        "get/set pins" },
    { NULL, NULL, NULL }
};

// pin definitions
struct PinDef {
    char const *name;
    int dev;
    int reg;
    uint32_t mask;
    bool writ;
};

#define DEV_8L 0
#define DEV_CM 1
#define DEV_TT 2
#define DEV_XM 3

static uint32_t volatile *devs[4];

static PinDef const pindefs[] = {
    { "iBEMA",           DEV_8L, Z_RA, a_iBEMA,         true  },
    { "iCA_INCREMENT",   DEV_8L, Z_RA, a_iCA_INCREMENT, true  },
    { "iDATA_IN",        DEV_8L, Z_RA, a_iDATA_IN,      true  },
    { "iMEMINCR",        DEV_8L, Z_RA, a_iMEMINCR,      true  },
    { "iMEM_P",          DEV_8L, Z_RA, a_iMEM_P,        true  },
    { "iTHREECYCLE",     DEV_8L, Z_RA, a_iTHREECYCLE,   true  },
    { "i_AC_CLEAR",      DEV_8L, Z_RA, a_i_AC_CLEAR,    true  },
    { "i_BRK_RQST",      DEV_8L, Z_RA, a_i_BRK_RQST,    true  },
    { "i_EA",            DEV_8L, Z_RA, a_i_EA,          true  },
    { "i_EMA",           DEV_8L, Z_RA, a_i_EMA,         true  },
    { "i_INT_INHIBIT",   DEV_8L, Z_RA, a_i_INT_INHIBIT, true  },
    { "i_INT_RQST",      DEV_8L, Z_RA, a_i_INT_RQST,    true  },
    { "i_IO_SKIP",       DEV_8L, Z_RA, a_i_IO_SKIP,     true  },
    { "i_MEMDONE",       DEV_8L, Z_RA, a_i_MEMDONE,     true  },
    { "i_STROBE",        DEV_8L, Z_RA, a_i_STROBE,      true  },
    { "swCONT",          DEV_8L, Z_RB, b_swCONT,        true  },
    { "swDEP",           DEV_8L, Z_RB, b_swDEP,         true  },
    { "swDFLD",          DEV_8L, Z_RB, b_swDFLD,        true  },
    { "swEXAM",          DEV_8L, Z_RB, b_swEXAM,        true  },
    { "swIFLD",          DEV_8L, Z_RB, b_swIFLD,        true  },
    { "swLDAD",          DEV_8L, Z_RB, b_swLDAD,        true  },
    { "swMPRT",          DEV_8L, Z_RB, b_swMPRT,        true  },
    { "swSTEP",          DEV_8L, Z_RB, b_swSTEP,        true  },
    { "swSTOP",          DEV_8L, Z_RB, b_swSTOP,        true  },
    { "swSTART",         DEV_8L, Z_RB, b_swSTART,       true  },
    { "swSR",            DEV_8L, Z_RB, b_swSR,          true  },
    { "iINPUTBUS",       DEV_8L, Z_RC, c_iINPUTBUS,     true  },
    { "iMEM",            DEV_8L, Z_RC, c_iMEM,          true  },
    { "i_DMAADDR",       DEV_8L, Z_RD, d_i_DMAADDR,     true  },
    { "i_DMADATA",       DEV_8L, Z_RD, d_i_DMADATA,     true  },
    { "simit",           DEV_8L, Z_RE, e_simit,         true  },
    { "softreset",       DEV_8L, Z_RE, e_softreset,     true  },
    { "nanocontin",      DEV_8L, Z_RE, e_nanocontin,    true  },
    { "nanotrigger",     DEV_8L, Z_RE, e_nanotrigger,   true  },
    { "nanocstep",       DEV_8L, Z_RE, e_nanocstep,     false },
    { "brkwhenhltd",     DEV_8L, Z_RE, e_brkwhenhltd,   true  },
    { "bareit",          DEV_8L, Z_RE, e_bareit,        true  },
    { "oBIOP1",          DEV_8L, Z_RF, f_oBIOP1,        false },
    { "oBIOP2",          DEV_8L, Z_RF, f_oBIOP2,        false },
    { "oBIOP4",          DEV_8L, Z_RF, f_oBIOP4,        false },
    { "oBTP2",           DEV_8L, Z_RF, f_oBTP2,         false },
    { "oBTP3",           DEV_8L, Z_RF, f_oBTP3,         false },
    { "oBTS_1",          DEV_8L, Z_RF, f_oBTS_1,        false },
    { "oBTS_3",          DEV_8L, Z_RF, f_oBTS_3,        false },
    { "oBWC_OVERFLOW",   DEV_8L, Z_RF, f_oBWC_OVERFLOW, false },
    { "oB_BREAK",        DEV_8L, Z_RF, f_oB_BREAK,      false },
    { "oE_SET_F_SET",    DEV_8L, Z_RF, f_oE_SET_F_SET,  false },
    { "oJMP_JMS",        DEV_8L, Z_RF, f_oJMP_JMS,      false },
    { "oLINE_LOW",       DEV_8L, Z_RF, f_oLINE_LOW,     false },
    { "oMEMSTART",       DEV_8L, Z_RF, f_oMEMSTART,     false },
    { "o_ADDR_ACCEPT",   DEV_8L, Z_RF, f_o_ADDR_ACCEPT, false },
    { "o_BF_ENABLE",     DEV_8L, Z_RF, f_o_BF_ENABLE,   false },
    { "o_BUSINIT",       DEV_8L, Z_RF, f_o_BUSINIT,     false },
    { "o_B_RUN",         DEV_8L, Z_RF, f_o_B_RUN,       false },
    { "o_DF_ENABLE",     DEV_8L, Z_RF, f_o_DF_ENABLE,   false },
    { "o_KEY_CLEAR",     DEV_8L, Z_RF, f_o_KEY_CLEAR,   false },
    { "o_KEY_DF",        DEV_8L, Z_RF, f_o_KEY_DF,      false },
    { "o_KEY_IF",        DEV_8L, Z_RF, f_o_KEY_IF,      false },
    { "o_KEY_LOAD",      DEV_8L, Z_RF, f_o_KEY_LOAD,    false },
    { "o_LOAD_SF",       DEV_8L, Z_RF, f_o_LOAD_SF,     false },
    { "o_SP_CYC_NEXT",   DEV_8L, Z_RF, f_o_SP_CYC_NEXT, false },
    { "lbBRK",           DEV_8L, Z_RG, g_lbBRK,         false },
    { "lbCA",            DEV_8L, Z_RG, g_lbCA,          false },
    { "lbDEF",           DEV_8L, Z_RG, g_lbDEF,         false },
    { "lbEA",            DEV_8L, Z_RG, g_lbEA,          false },
    { "lbEXE",           DEV_8L, Z_RG, g_lbEXE,         false },
    { "lbFET",           DEV_8L, Z_RG, g_lbFET,         false },
    { "lbION",           DEV_8L, Z_RG, g_lbION,         false },
    { "lbLINK",          DEV_8L, Z_RG, g_lbLINK,        false },
    { "lbRUN",           DEV_8L, Z_RG, g_lbRUN,         false },
    { "lbWC",            DEV_8L, Z_RG, g_lbWC,          false },
    { "debounced",       DEV_8L, Z_RG, g_debounced,     false },
    { "lastswLDAD",      DEV_8L, Z_RG, g_lastswLDAD,    false },
    { "lastswSTART",     DEV_8L, Z_RG, g_lastswSTART,   false },
    { "lbIR",            DEV_8L, Z_RG, g_lbIR,          false },
    { "oBAC",            DEV_8L, Z_RH, h_oBAC,          false },
    { "oBMB",            DEV_8L, Z_RH, h_oBMB,          false },
    { "oMA",             DEV_8L, Z_RI, i_oMA,           false },
    { "lbAC",            DEV_8L, Z_RI, i_lbAC,          false },
    { "lbMA",            DEV_8L, Z_RJ, j_lbMA,          false },
    { "lbMB",            DEV_8L, Z_RJ, j_lbMB,          false },
    { "majstate",        DEV_8L, Z_RK, k_majstate,      false },
    { "timedelay",       DEV_8L, Z_RK, k_timedelay,     false },
    { "timestate",       DEV_8L, Z_RK, k_timestate,     false },
    { "cyclectr",        DEV_8L, Z_RK, k_cyclectr,      false },
    { "nextmajst",       DEV_8L, Z_RK, k_nextmajst,     false },
    { "breakdata",       DEV_8L, Z_RL, l_breakdata,     false },
    { "bDMABUS",         DEV_8L, Z_RO, o_bDMABUS,       false },
    { "r_BAC",           DEV_8L, Z_RO, o_r_BAC,         true  },
    { "r_BMB",           DEV_8L, Z_RO, o_r_BMB,         true  },
    { "r_MA",            DEV_8L, Z_RO, o_r_MA,          true  },
    { "x_DMAADDR",       DEV_8L, Z_RO, o_x_DMAADDR,     true  },
    { "x_DMADATA",       DEV_8L, Z_RO, o_x_DMADATA,     true  },
    { "x_INPUTBUS",      DEV_8L, Z_RO, o_x_INPUTBUS,    true  },
    { "x_MEM",           DEV_8L, Z_RO, o_x_MEM,         true  },
    { "bMEMBUS",         DEV_8L, Z_RP, p_bMEMBUS,       false },
    { "bPIOBUS",         DEV_8L, Z_RP, p_bPIOBUS,       false },
    { "CM_ADDR",         DEV_CM, 1, CM_ADDR,            true  },
    { "CM_WRITE",        DEV_CM, 1, CM_WRITE,           true  },
    { "CM_DATA",         DEV_CM, 1, CM_DATA,            true  },
    { "CM_BUSY",         DEV_CM, 1, CM_BUSY,            false },
    { "CM_RRDY",         DEV_CM, 1, CM_RRDY,            false },
    { "CM_ENAB",         DEV_CM, 1, CM_ENAB,            true  },
    { "KB_FLAG",         DEV_TT, Z_TTYKB, KB_FLAG,      false },
    { "KB_ENAB",         DEV_TT, Z_TTYKB, KB_ENAB,      true  },
    { "PR_FLAG",         DEV_TT, Z_TTYPR, PR_FLAG,      false },
    { "PR_FULL",         DEV_TT, Z_TTYPR, PR_FULL,      false },
    { "XM_ENLO4K",       DEV_XM, 1, XM_ENLO4K,          true  },
    { "XM_ENABLE",       DEV_XM, 1, XM_ENABLE,          true  },
    { "XM2_MEMDELAY",    DEV_XM, 2, XM2_MEMDELAY,       false },
    { "XM2_SAVEDIFLD",   DEV_XM, 2, XM2_SAVEDIFLD,      false },
    { "XM2_SAVEDDFLD",   DEV_XM, 2, XM2_SAVEDDFLD,      false },
    { "XM2_IFLDAFJMP",   DEV_XM, 2, XM2_IFLDAFJMP,      false },
    { "XM2_IFLD",        DEV_XM, 2, XM2_IFLD,           false },
    { "XM2_DFLD",        DEV_XM, 2, XM2_DFLD,           false },
    { "XM2_FIELD",       DEV_XM, 2, XM2_FIELD,          false },
    { "XM2__MWDONE",     DEV_XM, 2, XM2__MWDONE,        false },
    { "XM2__MRDONE",     DEV_XM, 2, XM2__MRDONE,        false },
    { NULL, 0, 0, 0, false }
};

static bool volatile ctrlcflag;
static char *inihelp;
static sigset_t sigintmask;
static uint32_t volatile *extmemptr;

static void siginthand (int signum);
static void Tcl_SetResultF (Tcl_Interp *interp, char const *fmt, ...);



int main (int argc, char **argv)
{
    setlinebuf (stdout);
    sigemptyset (&sigintmask);
    sigaddset (&sigintmask, SIGINT);

    char const *fn = NULL;
    for (int i = 0; ++ i < argc;) {
        if ((argv[i][0] == '-') || (fn != NULL)) {
            fprintf (stderr, "usage: %s [<tclfilename>]\n", argv[0]);
            return 1;
        }
        fn = argv[i];
    }

    // access the zynq io page and find devices thereon
    Z8LPage z8p;
    devs[DEV_8L] = z8p.findev ("8L", NULL, NULL, false);
    devs[DEV_CM] = z8p.findev ("CM", NULL, NULL, false);
    devs[DEV_TT] = z8p.findev ("TT", NULL, NULL, false);
    devs[DEV_XM] = z8p.findev ("XM", NULL, NULL, false);

    // get pointer to the 32K-word ram
    // maps each 12-bit word into low 12 bits of 32-bit word
    // upper 20 bits discarded on write, readback as zeroes
    extmemptr = z8p.extmem ();

    Tcl_FindExecutable (argv[0]);

    Tcl_Interp *interp = Tcl_CreateInterp ();
    if (interp == NULL) ABORT ();
    int rc = Tcl_Init (interp);
    if (rc != TCL_OK) {
        char const *err = Tcl_GetStringResult (interp);
        if ((err != NULL) && (err[0] != 0)) {
            fprintf (stderr, "pipan8l: error %d initialing tcl: %s\n", rc, err);
        } else {
            fprintf (stderr, "pipan8l: error %d initialing tcl\n", rc);
        }
        ABORT ();
    }

    for (int i = 0; fundefs[i].name != NULL; i ++) {
        if (Tcl_CreateObjCommand (interp, fundefs[i].name, fundefs[i].func, NULL, NULL) == NULL) ABORT ();
    }

    // set ctrlcflag on control-C, but exit if two in a row
    signal (SIGINT, siginthand);

    // maybe there is a script init file
    char const *scriptini = getenv ("z8lpinini");
    if (scriptini != NULL) {
        rc = Tcl_EvalFile (interp, scriptini);
        if (rc != TCL_OK) {
            char const *err = Tcl_GetStringResult (interp);
            if ((err == NULL) || (err[0] == 0)) fprintf (stderr, "z8lpin: error %d evaluating scriptini %s\n", rc, scriptini);
                                  else fprintf (stderr, "z8lpin: error %d evaluating scriptini %s: %s\n", rc, scriptini, err);
            Tcl_EvalEx (interp, "puts $::errorInfo", -1, TCL_EVAL_GLOBAL);
            return 1;
        }
        char const *res = Tcl_GetStringResult (interp);
        if ((res != NULL) && (res[0] != 0)) inihelp = strdup (res);
    }

    // if given a filename, process that file as a whole
    if (fn != NULL) {
        rc = Tcl_EvalFile (interp, fn);
        if (rc != TCL_OK) {
            char const *res = Tcl_GetStringResult (interp);
            if ((res == NULL) || (res[0] == 0)) fprintf (stderr, "z8lpin: error %d evaluating script %s\n", rc, fn);
                                  else fprintf (stderr, "z8lpin: error %d evaluating script %s: %s\n", rc, fn, res);
            Tcl_EvalEx (interp, "puts $::errorInfo", -1, TCL_EVAL_GLOBAL);
            return 1;
        }
    }

    // either way, prompt and process commands from stdin
    // to have a script file with no stdin processing, end script file with 'exit'
    puts ("\nTCL scripting, do 'help' for z8lpin-specific commands");
    puts ("  do 'exit' to exit z8lpin");
    for (char const *line;;) {
        ctrlcflag = false;
        line = readprompt ("z8lpin> ");
        ctrlcflag = false;
        if (line == NULL) break;
        rc = Tcl_EvalEx (interp, line, -1, TCL_EVAL_GLOBAL);
        char const *res = Tcl_GetStringResult (interp);
        if (rc != TCL_OK) {
            if ((res == NULL) || (res[0] == 0)) fprintf (stderr, "z8lpin: error %d evaluating command\n", rc);
                                  else fprintf (stderr, "z8lpin: error %d evaluating command: %s\n", rc, res);
            Tcl_EvalEx (interp, "puts $::errorInfo", -1, TCL_EVAL_GLOBAL);
        }
        else if ((res != NULL) && (res[0] != 0)) puts (res);
    }

    Tcl_Finalize ();

    return 0;
}

static void siginthand (int signum)
{
    if ((isatty (STDIN_FILENO) > 0) && (write (STDIN_FILENO, "\n", 1) > 0)) { }
    if (ctrlcflag) exit (1);
    ctrlcflag = true;
}

void logflush ()
{ }

static int cmd_pin (ClientData clientdata, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    if ((objc == 2) && (strcasecmp (Tcl_GetString (objv[1]), "help") == 0)) {
        puts ("");
        puts ("  pin {get pin ...} | {set pin val ...} ...");
        puts ("");
        return TCL_OK;
    }

    bool setmode = false;
    int ngotvals = 0;
    Tcl_Obj *gotvals[objc];

    for (int i = 0; ++ i < objc;) {
        char const *name = Tcl_GetString (objv[i]);

        if (strcasecmp (name, "get") == 0) {
            setmode = false;
            continue;
        }
        if (strcasecmp (name, "set") == 0) {
            setmode = true;
            continue;
        }

        bool writeable;
        int width;
        uint32_t mask;
        uint32_t volatile *ptr;

        if (strncasecmp (name, "em:", 3) == 0) {
            char *p;
            mask = strtoul (name + 3, &p, 0);
            if ((*p != 0) || (mask > 077777)) {
                Tcl_SetResultF (interp, "extended memory address %s must be integer in range 000000..077777", name + 3);
                return TCL_ERROR;
            }
            ptr = extmemptr + mask;
            mask = 07777;
            width = 12;
            writeable = true;
        } else {
            PinDef const *pte;
            for (pte = pindefs; pte->name != NULL; pte ++) {
                if (strcasecmp (pte->name, name) == 0) goto gotit;
            }
            Tcl_SetResultF (interp, "bad pin name %s", name);
            return TCL_ERROR;
        gotit:;
            mask = pte->mask;
            width = 0;
            while (1U << ++ width <= mask / (mask & - mask)) { }
            ptr = devs[pte->dev] + pte->reg;
            writeable = pte->writ;
        }

        if (setmode) {
            if (! writeable) {
                Tcl_SetResultF (interp, "pin %s not settable", name);
                return TCL_ERROR;
            }
            if (++ i >= objc) {
                Tcl_SetResultF (interp, "missing pin value for set %s", name);
                return TCL_ERROR;
            }
            int val;
            int rc  = Tcl_GetIntFromObj (interp, objv[i], &val);
            if (rc != TCL_OK) return rc;
            if ((val < 0) || (val >= 1 << width)) {
                Tcl_SetResultF (interp, "value 0%o too big for %s", val, name);
                return TCL_ERROR;
            }
            *ptr = (*ptr & ~ mask) | val * (mask & - mask);
        } else {
            uint32_t val = (*ptr & mask) / (mask & - mask);
            gotvals[ngotvals++] = Tcl_NewIntObj (val);
        }
    }

    if (ngotvals > 0) {
        if (ngotvals < 2) {
            Tcl_SetObjResult (interp, gotvals[0]);
        } else {
            Tcl_SetObjResult (interp, Tcl_NewListObj (ngotvals, gotvals));
        }
    }
    return TCL_OK;
}

// read and clear the control-C flag
static int cmd_ctrlcflag (ClientData clientdata, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    bool oldctrlcflag = ctrlcflag;
    switch (objc) {
        case 2: {
            char const *opstr = Tcl_GetString (objv[1]);
            if (strcasecmp (opstr, "help") == 0) {
                puts ("");
                puts ("  ctrlcflag - read control-C flag");
                puts ("  ctrlcflag <boolean> - read control-C flag and set to given value");
                puts ("");
                puts ("  in any case, control-C flag is cleared at command prompt");
                puts ("");
                return TCL_OK;
            }
            int temp;
            int rc = Tcl_GetBooleanFromObj (interp, objv[1], &temp);
            if (rc != TCL_OK) return rc;
            if (sigprocmask (SIG_BLOCK, &sigintmask, NULL) != 0) ABORT ();
            oldctrlcflag = ctrlcflag;
            ctrlcflag = temp != 0;
            if (sigprocmask (SIG_UNBLOCK, &sigintmask, NULL) != 0) ABORT ();
            // fallthrough
        }
        case 1: {
            Tcl_SetObjResult (interp, Tcl_NewIntObj (oldctrlcflag));
            return TCL_OK;
        }
    }
    Tcl_SetResult (interp, (char *) "bad number of arguments", TCL_STATIC);
    return TCL_ERROR;
}

// print help messages
static int cmd_help (ClientData clientdata, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    puts ("");
    for (int i = 0; fundefs[i].help != NULL; i ++) {
        printf ("  %10s - %s\n", fundefs[i].name, fundefs[i].help);
    }
    puts ("");
    puts ("for help on specific command, do '<command> help'");
    if (inihelp != NULL) {
        puts ("");
        puts (inihelp);
    }
    puts ("");
    return TCL_OK;
}

// set result to formatted string
static void Tcl_SetResultF (Tcl_Interp *interp, char const *fmt, ...)
{
    char *buf = NULL;
    va_list ap;
    va_start (ap, fmt);
    if (vasprintf (&buf, fmt, ap) < 0) ABORT ();
    va_end (ap);
    Tcl_SetResult (interp, buf, (void (*) (char *)) free);
}
