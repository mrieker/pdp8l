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

//  ./z8ltty [-cps <charspersec>] [-nokb] [<octalportnumber>] [-tcl [<scriptfilename> [<scriptargs...>]]]
//     charspersec defaults to 10
//     octalportnumber defaults to 03

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
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
static Tcl_ObjCmdProc cmd_run;

static TclFunDef const fundefs[] = {
    { cmd_punch,  "punch",  "load file for punching" },
    { cmd_reader, "reader", "load file for reading" },
    { cmd_run,    "run",    "access tty i/o" },
    { NULL, NULL, NULL }
};

static uint8_t const nullbuf[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

static bool nokb;
static bool punchquiet;
static bool punchstat;
static bool readerquiet;
static bool readerstat;
static int punchfile = -1;
static int readerfile = -1;
static uint32_t cps = 10;
static uint32_t punchbytes;
static uint32_t readerbytes;
static uint32_t readersize;
static uint32_t volatile *ttyat;
static uint8_t punchmask;
static uint8_t readermask;

static bool findtt (void *param, uint32_t volatile *ttyat);
static bool stoponcheck (TTYStopOn *const stopon, char prchar);



int main (int argc, char **argv)
{
    bool dotcl = false;
    bool killit = false;
    int tclargs = argc;
    uint32_t port = 3;
    char *p;
    for (int i = 0; ++ i < argc;) {
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
        if (strcasecmp (argv[i], "-nokb") == 0) {
            nokb = true;
            continue;
        }
        if (strcasecmp (argv[i], "-tcl") == 0) {
            dotcl = true;
            tclargs = i + 1;
            break;
        }
        if (argv[i][0] == '-') {
            fprintf (stderr, "unknown option %s\n", argv[i]);
            return 1;
        }
        port = strtoul (argv[i], &p, 8);
        if ((*p != 0) || (port > 076)) {
            fprintf (stderr, "port number %s must be octal integer in range 0..076\n", argv[i]);
            return 1;
        }
    }

    Z8LPage z8p;
    ttyat = z8p.findev ("TT", findtt, &port, true, killit);

    ttyat[Z_TTYKB] = KB_ENAB;   // enable board to process io instructions

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
    uint32_t port = *(uint32_t *) param;
    if (ttyat == NULL) {
        fprintf (stderr, "findtt: cannot find TT port %02o\n", port);
        ABORT ();
    }
    return (ttyat[Z_TTYPN] & 077) == port;
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
            puts ("  run [-cps <charspersec>] [-kb] [-nokb]");
            puts ("    -cps = characters per second");
            puts ("    -kb = enable keyboard processing");
            puts ("    -nokb = disable keyboard processing");
            puts ("    -stopon <string> = stop when string printed");
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

    struct termios term_modified, term_original;
    stdintty = isatty (STDIN_FILENO) > 0;
    if (stdintty && ! nokb) {
        if (tcgetattr (STDIN_FILENO, &term_original) < 0) ABORT ();
        term_modified = term_original;
        cfmakeraw (&term_modified);
        if (tcsetattr (STDIN_FILENO, TCSANOW, &term_modified) < 0) ABORT ();
        fprintf (stderr, "z8ltty: use control-\\ for stop char\r\n");
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
            uint32_t prreg = ttyat[Z_TTYPR];
            if (prreg & PR_FULL) {

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

                // clear printing full flag, set printing complete flag
                ttyat[Z_TTYPR] = PR_FLAG;

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
                ttyat[Z_TTYKB] = KB_FLAG | KB_ENAB | 0200 | kbchar;
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
                    ttyat[Z_TTYKB] = KB_FLAG | KB_ENAB | kbbyte | readermask;
                    if (readerstat) fprintf (stderr, "\r[%u/%u]", ++ readerbytes, readersize);
                    // little slower for reader so pdp doesn't get overrun echoing
                    readnextkbat = nowus + 1111111 / cps;
                }
            }
        }
    }

stopped:;
    if (stdintty && ! nokb && (tcsetattr (STDIN_FILENO, TCSANOW, &term_original) < 0)) ABORT ();
    fprintf (stderr, "\n");
    retcode = TCL_OK;
reterr:;
    for (TTYStopOn *stopon; (stopon = stopons) != NULL;) {
        stopons = stopon->next;
        free (stopon);
    }
    return retcode;
}
