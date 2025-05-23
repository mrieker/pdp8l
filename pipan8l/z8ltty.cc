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

// Performs TTY I/O for the PDP-8/L Zynq I/O board

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <termios.h>
#include <unistd.h>

#include "tclmain.h"
#include "z8ldefs.h"
#include "z8lutil.h"

struct TTYStopOn {
    TTYStopOn *next;
    Tcl_Obj *strobj;
    int hits;
    char buff[1];
};

static Tcl_ObjCmdProc cmd_punch;
static Tcl_ObjCmdProc cmd_reader;
static Tcl_ObjCmdProc cmd_recvchar;
static Tcl_ObjCmdProc cmd_run;
static Tcl_ObjCmdProc cmd_sendchar;

static TclFunDef const fundefs[] = {
    { cmd_punch,    "punch",    "load file for punching" },
    { cmd_reader,   "reader",   "load file for reading" },
    { cmd_recvchar, "recvchar", "receive printer/punch character" },
    { cmd_run,      "run",      "access tty i/o" },
    { cmd_sendchar, "sendchar", "send keyboard/reader character" },
    { NULL, NULL, NULL }
};

static uint8_t const nullbuf[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

static bool (*getprchar) (uint8_t *prchar_r);
static bool (*putkbchar) (uint8_t kbchar);

static bool nokb;
static bool punchquiet;
static bool punchstat;
static bool readerquiet;
static bool readerstat;
static bool upcase;
static int punchfile = -1;
static int readerfile = -1;
static struct termios term_original;
static uint32_t cps = 10;
static uint32_t punchbytes;
static uint32_t readerbytes;
static uint32_t readersize;
static uint32_t volatile *dcreg;
static uint32_t volatile *ttyat;
static uint8_t punchmask;
static uint8_t readermask;

static bool findtt (void *param, uint32_t volatile *ttyat);
static bool stoponcheck (TTYStopOn *const stopon, char prchar);
static void sigrunhand (int signum);

static bool dc_getprchar (uint8_t *prchar_r);
static bool dc_putkbchar (uint8_t kbchar);
static bool tt_getprchar (uint8_t *prchar_r);
static bool tt_putkbchar (uint8_t kbchar);



int main (int argc, char **argv)
{
    bool dc02 = false;
    bool dotcl = false;
    bool killit = false;
    int port = -1;
    int tclargs = argc;
    char *p;
    for (int i = 0; ++ i < argc;) {
        if (strcmp (argv[i], "-?") == 0) {
            puts ("");
            puts ("     Access TTY");
            puts ("");
            puts ("  ./z8ltty [-cps <charspersec>] [-dc02] [-killit] [-nokb] [<octalportnumber>] [-upcase] [-tcl [<scriptfilename> [<scriptargs...>]]]");
            puts ("     -cps    : set chars per second, default 10");
            puts ("     -dc02   : <octalportnumber> is DC02 port number, 0..5, default 0");
            puts ("     -killit : kill other process that is processing this tty port");
            puts ("     -nokb   : do not pass stdin keyboard to pdp");
            puts ("     <octalportnumber> defaults to 03, other values are 40 42 44 46");
            puts ("     -tcl    : use tcl scripting");
            puts ("               if <scriptfilename> [<scriptargs...>] given, process from that script");
            puts ("               otherwise read and process commands from stdin");
            puts ("               needed for loading papertape files");
            puts ("     -upcase : convert all keyboard to upper case");
            puts ("");
            puts ("     Can access TTY 03 only if -entty03 given to z8lreal or using z8lsim simulator");
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
        if (strcasecmp (argv[i], "-dc02") == 0) {
            dc02 = true;
            continue;
        }
        if (strcasecmp (argv[i], "-killit") == 0) {
            killit = true;
            continue;
        }
        if (strcasecmp (argv[i], "-nokb") == 0) {
            nokb = true;
            continue;
        }
        if (strcasecmp (argv[i], "-tcl") == 0) {
            dotcl = true;
            tclargs = i + 1;
            break;
        }
        if (strcasecmp (argv[i], "-upcase") == 0) {
            upcase = true;
            continue;
        }
        if ((argv[i][0] == '-') || (port >= 0)) {
            fprintf (stderr, "unknown option %s\n", argv[i]);
            return 1;
        }
        port = strtoul (argv[i], &p, 8);
        if (*p != 0) {
            fprintf (stderr, "port number %s must be octal integer\n", argv[i]);
            return 1;
        }
    }

    Z8LPage z8p;
    if (dc02) {
        if (port < 0) port = 0;
        if (port > 5) {
            fprintf (stderr, "port number %o must be in range 0..5\n", port);
            return 1;
        }
        uint32_t volatile *dcat = z8p.findev ("DC", NULL, NULL, false, killit);
        dcat[1]   = 0x80000000U;    // enable board to process io instructions
        dcreg     = &dcat[2+port];  // point to register for this terminal
        getprchar = dc_getprchar;   // set up get/put functions
        putkbchar = dc_putkbchar;
        z8p.locksubdev (dcreg, 1, killit);  // make sure another one of these isn't running
    } else {
        if (port < 0) port = 3;
        if ((port < 1) || (port > 076)) {
            fprintf (stderr, "port number %o must be in range 1..76\n", port);
            return 1;
        }
        ttyat = z8p.findev ("TT", findtt, &port, true, killit);
        ttyat[Z_TTYKB] = KB_ENAB;   // enable board to process io instructions
        getprchar = tt_getprchar;   // set up get/put functions
        putkbchar = tt_putkbchar;
    }

    int rc;
    if (dotcl) {
        rc = tclmain (fundefs, argv[0], "z8ltty", NULL, getenv ("z8lttyini"), argc - tclargs, argv + tclargs, false);
    } else {
        cmd_run (NULL, NULL, 0, NULL);
        rc = 0;
    }
    return rc;
}



static bool findtt (void *param, uint32_t volatile *ttyat)
{
    int port = *(int *) param;
    if (ttyat == NULL) {
        fprintf (stderr, "findtt: cannot find TT port %02o\n", port);
        ABORT ();
    }
    return (ttyat[Z_TTYPN] & 077) == (uint32_t) port;
}

// have outgoing char, step stopon state and see if complete match has occurred
static bool stoponcheck (TTYStopOn *const stopon, char prchar)
{
    // null doesn't match anything so reset
    if (prchar == 0) {
        stopon->hits = 0;
        return false;
    }

    // if char matches next coming up, increment number of matching chars
    // if reached the end, this one is completely matched
    // if previously completely matched, prchar will mismatch on the null
    int i = stopon->hits;
    if (stopon->buff[i] != prchar) {

        // mismatch, but maybe an earlier part is still matched
        // eg, looking for "abcabcabd" but got "abcabcabc", we reset to 6
        //     looking for "abcd" but got "abca", we reset to 1
        // also has to work for case where "aaa" was completely matched last time,
        //   if prchar is 'a', we say complete match this time, else reset to 0
        do {
            char *p = (char *) memrchr (stopon->buff, prchar, i);
            if (p == NULL) {
                i = -1;
                break;
            }
            i = p - stopon->buff;

            // prchar = 'c'
            // hits = 8 in "abcabcabd"
            //                      ^hits
            //    i = 5 in "abcabcabd"
            //                   ^i
        } while (memcmp (stopon->buff, stopon->buff + stopon->hits - i, i) != 0);
    }

    // i = index where prchar matches (or -1 if not at all)
    stopon->hits = ++ i;
    return stopon->buff[i] == 0;
}



// load file into punch
static int cmd_punch (ClientData clientdata, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    if (objc > 1) {
        char const *subcmd = Tcl_GetString (objv[1]);
        if (strcasecmp (subcmd, "help") == 0) {
            puts ("");
            puts ("  punch load [<option>...] <filename> - load file");
            puts ("    -7bit = mask top bit to zero when writing file");
            puts ("    -quiet = suppress terminal output while punching");
            puts ("    -stat = print status as file is written");
            puts ("  punch nulls  - punch 16 nulls");
            puts ("  punch unload - unload file");
            puts ("");
            return TCL_OK;
        }

        if (strcasecmp (subcmd, "load") == 0) {
            close (punchfile);
            punchfile  = -1;
            bool quiet = false;
            char const *fn = NULL;
            punchmask  = 0377;
            punchquiet = false;
            punchstat  = false;
            for (int i = 1; ++ i < objc;) {
                char const *arg = Tcl_GetString (objv[i]);
                if (strcasecmp (arg, "-7bit") == 0) {
                    punchmask = 0177;
                    continue;
                }
                if (strcasecmp (arg, "-quiet") == 0) {
                    quiet = true;
                    continue;
                }
                if (strcasecmp (arg, "-stat") == 0) {
                    punchstat = true;
                    continue;
                }
                if (arg[0] == '-') {
                    Tcl_SetResultF (interp, "unknown option %s", arg);
                    return TCL_ERROR;
                }
                if (fn != NULL) {
                    Tcl_SetResultF (interp, "unknown argument %s", arg);
                    return TCL_ERROR;
                }
                fn = arg;
            }
            if (fn == NULL) {
                Tcl_SetResultF (interp, "missing filename");
                return TCL_ERROR;
            }
            int fd = open (fn, O_WRONLY | O_CREAT, 0666);
            if (fd < 0) {
                Tcl_SetResultF (interp, "error creating %s: %m", fn);
                return TCL_ERROR;
            }
            punchbytes = 0;
            punchfile  = fd;
            punchquiet = quiet;
            return TCL_OK;
        }

        if (strcasecmp (subcmd, "nulls") == 0) {
            if (punchfile < 0) {
                Tcl_SetResultF (interp, "no punch file loaded");
                return TCL_ERROR;
            }
            int rc = write (punchfile, nullbuf, sizeof nullbuf);
            if (rc < 0) {
                Tcl_SetResultF (interp, "error writing nulls: %m");
                return TCL_ERROR;
            }
            if (rc < (int) sizeof nullbuf) {
                Tcl_SetResultF (interp, "only wrote %d of %d nulls", rc, (int) sizeof nullbuf);
                return TCL_ERROR;
            }
            return TCL_OK;
        }

        if (strcasecmp (subcmd, "unload") == 0) {
            punchquiet = false;
            int fd = punchfile;
            if (fd >= 0) {
                punchfile = -1;
                if (close (fd) < 0) {
                    Tcl_SetResultF (interp, "error closing: %m");
                    return TCL_ERROR;
                }
            }
            return TCL_OK;
        }
    }
    Tcl_SetResultF (interp, "missing/unknown sub-command");
    return TCL_ERROR;
}

// load file to into reader
static int cmd_reader (ClientData clientdata, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    if (objc > 1) {
        char const *subcmd = Tcl_GetString (objv[1]);
        if (strcasecmp (subcmd, "help") == 0) {
            puts ("");
            puts ("  reader load [<option>...] <filename> - load file");
            puts ("    -7bit = force top bit to one when reading file");
            puts ("    -quiet = suppress terminal output while reading");
            puts ("    -stat = print status as file is read");
            puts ("  reader unload - unload file");
            puts ("");
            return TCL_OK;
        }

        if (strcasecmp (subcmd, "load") == 0) {
            close (readerfile);
            readerfile  = -1;
            bool quiet  = false;
            char const *fn = NULL;
            readermask  = 0;
            readerquiet = false;
            readerstat  = false;
            for (int i = 1; ++ i < objc;) {
                char const *arg = Tcl_GetString (objv[i]);
                if (strcasecmp (arg, "-7bit") == 0) {
                    readermask = 0200;
                    continue;
                }
                if (strcasecmp (arg, "-quiet") == 0) {
                    quiet = true;
                    continue;
                }
                if (strcasecmp (arg, "-stat") == 0) {
                    readerstat = true;
                    continue;
                }
                if (arg[0] == '-') {
                    Tcl_SetResultF (interp, "unknown option %s", arg);
                    return TCL_ERROR;
                }
                if (fn != NULL) {
                    Tcl_SetResultF (interp, "unknown argument %s", arg);
                    return TCL_ERROR;
                }
                fn = arg;
            }
            if (fn == NULL) {
                Tcl_SetResultF (interp, "missing filename");
                return TCL_ERROR;
            }
            int fd = open (fn, O_RDONLY);
            if (fd < 0) {
                Tcl_SetResultF (interp, "error opening %s: %m", fn);
                return TCL_ERROR;
            }
            readerbytes = 0;
            readerfile  = fd;
            readerquiet = quiet;
            readersize  = 0;
            if (readerstat) {
                struct stat statbuf;
                if ((fstat (fd, &statbuf) >= 0) && S_ISREG (statbuf.st_mode)) {
                    readersize = statbuf.st_size;
                }
            }
            return TCL_OK;
        }

        if (strcasecmp (subcmd, "unload") == 0) {
            close (readerfile);
            readerfile  = -1;
            readerquiet = false;
            return TCL_OK;
        }
    }
    Tcl_SetResultF (interp, "missing/unknown sub-command");
    return TCL_ERROR;
}

// read single printer/punch character
static int cmd_recvchar (ClientData clientdata, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    if ((objc == 2) && (strcasecmp (Tcl_GetString (objv[1]), "help") == 0)) {
        puts ("");
        puts ("  recvchar [-7bit] [-int] [-time <timeoutms>]");
        puts ("    -7bit = mask top bit to zero");
        puts ("    -int  = return character as an integer or -1 if timeout");
        puts ("    -time = timeout after given milliseconds, default 1000");
        puts ("  returns character received or null string if timeout/control-C");
        puts ("");
        return TCL_OK;
    }

    bool intflag = false;
    int timeout = 1000;
    uint8_t mask = 0377;
    for (int i = 0; ++ i < objc;) {
        char const *arg = Tcl_GetString (objv[i]);
        if (strcasecmp (arg, "-7bit") == 0) {
            mask = 0177;
            continue;
        }
        if (strcasecmp (arg, "-int") == 0) {
            intflag = true;
            continue;
        }
        if (strcasecmp (arg, "-time") == 0) {
            if (++ i >= objc) {
                Tcl_SetResultF (interp, "missing timeout value");
                return TCL_ERROR;
            }
            int rc = Tcl_GetIntFromObj (interp, objv[i], &timeout);
            if (rc != TCL_OK) return rc;
            continue;
        }
        Tcl_SetResultF (interp, "unknown argument/option %s", arg);
        return TCL_ERROR;
    }

    while (! ctrlcflag) {
        uint8_t ch;
        if (getprchar (&ch)) {
            Tcl_SetResultF (interp, intflag ? "%u" : "%c", ch & mask);
            break;
        }
        if (-- timeout < 0) break;
        usleep (1000);
    }
    return TCL_OK;
}

// take over stdin/stdout for tty operations
// if there is a reader file loaded, shovel it to the pdp as keyboard characters at cps rate
// if there is a punch file, copy printer output to the file
// keep going until we get control-backslash
static int cmd_run (ClientData clientdata, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    bool stdintty, stdoutty;
    uint64_t nowus;
    uint64_t readnextkbat;
    uint64_t readnextprat;

    int retcode = TCL_ERROR;
    TTYStopOn *stopons = NULL;
    TTYStopOn **laststopon = &stopons;

    for (int i = 0; ++ i < objc;) {
        char const *arg = Tcl_GetString (objv[i]);
        if ((objc == 2) && (strcasecmp (arg, "help") == 0)) {
            puts ("");
            puts ("  run [-cps <charspersec>] [-kb] [-nokb] [-stopon <string>]");
            printf ("    -cps = characters per second, default %d\n", cps);
            printf ("    -kb = enable keyboard processing%s, stop via control-\\\n", (nokb ? "" : ", default"));
            printf ("    -nokb = disable keyboard processing%s, stop via control-C\n", (nokb ? ", default" : ""));
            puts ("    -stopon <string> = stop when string printed");
            puts ("       option may be given multiple times");
            puts ("");
            puts ("  return value is matching stopon <string> or null string if stopped via control-\\ or -C");
            puts ("");
            return TCL_OK;
        }

        if (strcasecmp (arg, "-cps") == 0) {
            if ((++ i >= objc) || (Tcl_GetString (objv[i])[0] == '-')) {
                Tcl_SetResultF (interp, "missing speed after -cps");
                goto reterr;
            }
            int icps;
            int rc = Tcl_GetIntFromObj (interp, objv[i], &icps);
            if (rc != TCL_OK) return rc;
            if ((icps < 1) || (icps > 1000)) {
                Tcl_SetResultF (interp, "cps %d must be in range 1..1000", icps);
                goto reterr;
            }
            cps = icps;
            continue;
        }

        if (strcasecmp (arg, "-kb") == 0) {
            nokb = false;
            continue;
        }
        if (strcasecmp (arg, "-nokb") == 0) {
            nokb = true;
            continue;
        }

        if (strcasecmp (arg, "-stopon") == 0) {
            if (++ i >= objc) {
                Tcl_SetResultF (interp, "missing string after -stopon");
                goto reterr;
            }
            char const *str = Tcl_GetString (objv[i]);
            int len = strlen (str);
            TTYStopOn *stopon = (TTYStopOn *) malloc (len + sizeof *stopon);
            if (stopon == NULL) ABORT ();
            *laststopon = stopon;
            laststopon = &stopon->next;
            stopon->next = NULL;
            stopon->strobj = objv[i];
            stopon->hits = 0;
            memcpy (stopon->buff, str, len + 1);
            continue;
        }

        Tcl_SetResultF (interp, "unknown argument %s", arg);
        goto reterr;
    }

    struct termios term_modified;
    stdintty = isatty (STDIN_FILENO) > 0;
    if (stdintty && ! nokb) {
        // stdin is a tty, set it to raw mode
        fprintf (stderr, "z8ltty: use control-\\ for stop char\n");
        if (tcgetattr (STDIN_FILENO, &term_original) < 0) ABORT ();
        if (signal (SIGHUP,  sigrunhand) != SIG_DFL) ABORT ();
        if (signal (SIGTERM, sigrunhand) != SIG_DFL) ABORT ();
        term_modified = term_original;
        cfmakeraw (&term_modified);
        if (tcsetattr (STDIN_FILENO, TCSADRAIN, &term_modified) < 0) ABORT ();
    } else {
        // stdin not a tty (or we are supposed to ignore it)
        // in case user presses control-\ to terminate
        if (signal (SIGQUIT, sigrunhand) != SIG_DFL) ABORT ();
    }

    struct timeval nowtv;
    if (gettimeofday (&nowtv, NULL) < 0) ABORT ();
    nowus = nowtv.tv_sec * 1000000ULL + nowtv.tv_usec;

    stdoutty = isatty (STDOUT_FILENO) > 0;
    readnextprat = nowus + 1111111 / cps;
    readnextkbat = (nokb && (readerfile < 0)) ? 0xFFFFFFFFFFFFFFFFULL : readnextprat;

    // keep processing until control-backslash
    // control-C is recognized only if -nokb mode
    while (! ctrlcflag) {
        if (gettimeofday (&nowtv, NULL) < 0) ABORT ();
        nowus = nowtv.tv_sec * 1000000ULL + nowtv.tv_usec;
        usleep (1000 - nowus % 1000);
        if (gettimeofday (&nowtv, NULL) < 0) ABORT ();
        nowus = nowtv.tv_sec * 1000000ULL + nowtv.tv_usec;

        // maybe see if PDP has a character to print
        if (nowus >= readnextprat) {
            uint8_t prreg;
            if (getprchar (&prreg)) {

                // print character to stdout
                uint8_t prchar = prreg & 0177;
                if (! readerquiet && ! punchquiet) {
                    if ((prchar == 7) && stdoutty) {
                        int rc = write (STDOUT_FILENO, "<BEL>", 5);
                        if (rc < 5) ABORT ();
                    } else {
                        int rc = write (STDOUT_FILENO, &prchar, 1);
                        if (rc <= 0) ABORT ();
                    }
                }

                // if punchfile, write character to file
                if (punchfile >= 0) {
                    uint8_t prbyte = prreg & punchmask;
                    int rc = write (punchfile, &prbyte, 1);
                    if (rc < 0) {
                        fprintf (stderr, "\r\nz8ltty: error writing punch file: %m\r\n");
                        break;
                    }
                    if (rc != 1) {
                        fprintf (stderr, "\r\nz8ltty: only wrote %d bytes of 1 to punch file\r\n", rc);
                        break;
                    }
                    if (punchstat) fprintf (stderr, "\r[%u]", ++ punchbytes);
                }

                // check for another char to print after 1000000/cps usec
                readnextprat = nowus + 1000000 / cps;

                // check stopons, stop if match
                for (TTYStopOn *stopon = stopons; stopon != NULL; stopon = stopon->next) {
                    if (stoponcheck (stopon, prchar)) {
                        Tcl_IncrRefCount (stopon->strobj);
                        Tcl_SetObjResult (interp, stopon->strobj);
                        goto stopped;
                    }
                }
            }
        }

        if (nowus >= readnextkbat) {

            // chars from stdin take precedence over reader file
            // ...so user can do ctrl-\ to get back even if there is a reader file loaded
            struct pollfd polls[1] = { STDIN_FILENO, POLLIN, 0 };
            int rc = nokb ? 0 : poll (polls, 1, 0);
            if ((rc < 0) && (errno != EINTR)) ABORT ();
            if ((rc > 0) && (polls[0].revents & POLLIN)) {

                // stdin char ready, read and pass along to pdp
                // but exit if it is a ctrl-backslash
                uint8_t kbchar;
                rc = read (STDIN_FILENO, &kbchar, 1);
                if ((rc == 0) && ! stdintty) break;
                if (rc <= 0) ABORT ();
                if ((kbchar == '\\' - '@') && stdintty) break;
                if (upcase && (kbchar >= 'a') && (kbchar <= 'z')) kbchar -= 'a' - 'A';
                putkbchar (0200 | kbchar);
                readnextkbat = nowus + 1000000 / cps;
            } else if (readerfile >= 0) {

                // stdin got nothing but there is a reader file,
                // send next reader byte to pdp
                uint8_t kbbyte;
                int rc = read (readerfile, &kbbyte, 1);
                if (rc < 0) {
                    fprintf (stderr, "\r\nz8ltty: error reading reader file: %m\r\n");
                    break;
                }
                if (rc == 0) {
                    fprintf (stderr, "\r\nz8ltty: reached end of reader file\r\n");
                    close (readerfile);
                    readerfile  = -1;
                    readerquiet = false;
                } else {
                    putkbchar (kbbyte | readermask);
                    if (readerstat) fprintf (stderr, "\r[%u/%u]", ++ readerbytes, readersize);
                    // little slower for reader so pdp doesn't get overrun echoing
                    readnextkbat = nowus + 1111111 / cps;
                }
            }
        }
    }

stopped:;
    if (stdintty && ! nokb) {
        tcsetattr (STDIN_FILENO, TCSADRAIN, &term_original);
        signal (SIGHUP,  SIG_DFL);
        signal (SIGTERM, SIG_DFL);
    } else {
        signal (SIGQUIT, SIG_DFL);
    }
    fprintf (stderr, "\n");
    retcode = TCL_OK;
reterr:;
    for (TTYStopOn *stopon; (stopon = stopons) != NULL;) {
        stopons = stopon->next;
        free (stopon);
    }
    return retcode;
}

// send single keyboard/reader character
static int cmd_sendchar (ClientData clientdata, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    if ((objc == 2) && (strcasecmp (Tcl_GetString (objv[1]), "help") == 0)) {
        puts ("");
        puts ("  sendchar [-7bit] [-int] [-time <timeoutms>] <character>");
        puts ("    -7bit = force top bit to one");
        puts ("    -int  = character argument is integer");
        puts ("    -time = timeout after given milliseconds, default 1000");
        puts ("  returns true if successful, false if timeout");
        puts ("");
        return TCL_OK;
    }

    bool intflag = false;
    char *p;
    int character = -1;
    int timeout = 1000;
    uint8_t mask = 0000;
    for (int i = 0; ++ i < objc;) {
        char const *arg = Tcl_GetString (objv[i]);
        if (strcasecmp (arg, "-7bit") == 0) {
            mask = 0200;
            continue;
        }
        if (strcasecmp (arg, "-int") == 0) {
            intflag = true;
            continue;
        }
        if (strcasecmp (arg, "-time") == 0) {
            if (++ i >= objc) {
                Tcl_SetResultF (interp, "missing timeout value");
                return TCL_ERROR;
            }
            int rc = Tcl_GetIntFromObj (interp, objv[i], &timeout);
            if (rc != TCL_OK) return rc;
            continue;
        }
        if ((character >= 0) || (intflag && (arg[0] == '-'))) {
            Tcl_SetResultF (interp, "unknown argument/option %s", arg);
            return TCL_ERROR;
        }
        if (intflag) {
            character = strtol (arg, &p, 0);
            if ((*p != 0) || (character < 0) || (character > 255)) {
                Tcl_SetResultF (interp, "bad character integer in range 0..255 %s", arg);
                return TCL_ERROR;
            }
        } else {
            character = (uint32_t) (uint8_t) arg[0];
        }
    }
    if (character < 0) {
        Tcl_SetResultF (interp, "missing character argument");
        return TCL_ERROR;
    }
    character |= mask;

    int rc = 0;
    while (! ctrlcflag && ! (rc = putkbchar (character))) {
        if (-- timeout < 0) break;
        usleep (1000);
    }
    Tcl_SetObjResult (interp, Tcl_NewIntObj (rc));
    return TCL_OK;
}

static void sigrunhand (int signum)
{
    if (signum == SIGQUIT) {
        if (! ctrlcflag) {
            ctrlcflag = true;
            return;
        }
    } else {
        tcsetattr (STDIN_FILENO, TCSADRAIN, &term_original);
    }
    dprintf (STDERR_FILENO, "\nz8ltty: terminated by signal %d\n", signum);
    exit (1);
}

// get printer character from terminal multiplexor

static bool dc_getprchar (uint8_t *prchar_r)
{
    uint32_t prreg = *dcreg;
    if (! (prreg & 0x20000000)) return false;
    *prchar_r = prreg >> 12;
    *dcreg = 0x4C000000;            // set prflag=1, prfull=0
    return true;
}

// put keyboard character to terminal multiplexor

static bool dc_putkbchar (uint8_t kbchar)
{
    if (*dcreg & 0x80000000U) return false;
    *dcreg = 0x91000000U | kbchar;  // set kbchar, kbflag=1
    return true;
}

// get printer character from single terminal interface

static bool tt_getprchar (uint8_t *prchar_r)
{
    uint32_t prreg = ttyat[Z_TTYPR];
    if (! (prreg & PR_FULL)) return false;
    *prchar_r = prreg;
    ttyat[Z_TTYPR] = PR_FLAG;
    return true;
}

// put keyboard character to single terminal interface

static bool tt_putkbchar (uint8_t kbchar)
{
    if (ttyat[Z_TTYKB] & KB_FLAG) return false;
    ttyat[Z_TTYKB] = KB_FLAG | KB_ENAB | kbchar;
    return true;
}
