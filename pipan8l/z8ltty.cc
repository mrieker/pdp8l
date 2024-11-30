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
static int punchfile = -1;
static int readerfile = -1;
static uint32_t cps = 10;
static uint32_t readerbytes;
static uint32_t readersize;
static uint32_t volatile *ttyat;
static uint8_t punchmask;
static uint8_t readermask;

static bool findtt (void *param, uint32_t volatile *ttyat)
{
    uint32_t port = *(uint32_t *) param;
    if (ttyat == NULL) {
        fprintf (stderr, "findtt: cannot find TT port %02o\n", port);
        ABORT ();
    }
    return (ttyat[Z_TTYPN] & 077) == port;
}

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

// load file to into punch
static int cmd_punch (ClientData clientdata, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    if (objc > 1) {
        char const *subcmd = Tcl_GetString (objv[1]);
        if (strcasecmp (subcmd, "help") == 0) {
            puts ("");
            puts ("  punch load [<option>...] <filename> - load file");
            puts ("      -7bit = mask top bit to zero when writing file");
            puts ("  punch nulls  - punch 16 nulls");
            puts ("  punch unload - unload file");
            puts ("");
            return TCL_OK;
        }

        if (strcasecmp (subcmd, "load") == 0) {
            char const *fn = NULL;
            punchmask = 0377;
            for (int i = 1; ++ i < objc;) {
                char const *arg = Tcl_GetString (objv[i]);
                if (strcasecmp (arg, "-7bit") == 0) {
                    punchmask = 0177;
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
            close (punchfile);
            punchfile = fd;
            return TCL_OK;
        }

        if (strcasecmp (subcmd, "nulls") == 0) {
            int rc = write (punchfile, nullbuf, sizeof nullbuf);
            if (rc < 0) {
                Tcl_SetResultF (interp, "error writing leader: %m");
                return TCL_ERROR;
            }
            if (rc < (int) sizeof nullbuf) {
                Tcl_SetResultF (interp, "only wrote %d bytes of %d bytes", rc, sizeof nullbuf);
                return TCL_ERROR;
            }
        }

        if (strcasecmp (subcmd, "unload") == 0) {
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
            puts ("  reader load [-7bit] <filename> - load file");
            puts ("    -7bit = force top bit to one when reading file");
            puts ("    -stat = print status as file is read");
            puts ("  reader unload - unload file");
            puts ("");
            return TCL_OK;
        }

        if (strcasecmp (subcmd, "load") == 0) {
            bool status = false;
            char const *fn = NULL;
            readermask = 0;
            for (int i = 1; ++ i < objc;) {
                char const *arg = Tcl_GetString (objv[i]);
                if (strcasecmp (arg, "-7bit") == 0) {
                    readermask = 0200;
                    continue;
                }
                if (strcasecmp (arg, "-stat") == 0) {
                    status = true;
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
            readersize  = 0;
            if (status) {
                struct stat statbuf;
                if ((fstat (fd, &statbuf) >= 0) && S_ISREG (statbuf.st_mode)) {
                    readersize = statbuf.st_size;
                }
            }
            close (readerfile);
            readerfile = fd;
            return TCL_OK;
        }

        if (strcasecmp (subcmd, "unload") == 0) {
            close (readerfile);
            readerfile = -1;
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
    for (int i = 0; ++ i < objc;) {
        char const *arg = Tcl_GetString (objv[i]);
        if ((objc == 2) && (strcasecmp (arg, "help") == 0)) {
            puts ("");
            puts ("  run [-cps <charspersec>] [-kb] [-nokb]");
            puts ("    -cps = characters per second");
            puts ("    -kb = enable keyboard processing");
            puts ("    -nokb = disable keyboard processing");
            puts ("");
            return TCL_OK;
        }
        if (strcasecmp (arg, "-cps") == 0) {
            if ((++ i >= objc) || (Tcl_GetString (objv[i])[0] == '-')) {
                Tcl_SetResultF (interp, "missing speed after -cps\n");
                return TCL_ERROR;
            }
            int icps;
            int rc = Tcl_GetIntFromObj (interp, objv[i], &icps);
            if (rc != TCL_OK) return rc;
            if ((icps < 1) || (icps > 1000)) {
                Tcl_SetResultF (interp, "cps %d must be in range 1..1000", icps);
                return TCL_ERROR;
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
        Tcl_SetResultF (interp, "unknown argument %s", arg);
        return TCL_ERROR;
    }

    struct termios term_modified, term_original;
    bool stdintty = isatty (STDIN_FILENO) > 0;
    if (stdintty && ! nokb) {
        if (tcgetattr (STDIN_FILENO, &term_original) < 0) ABORT ();
        term_modified = term_original;
        cfmakeraw (&term_modified);
        if (tcsetattr (STDIN_FILENO, TCSANOW, &term_modified) < 0) ABORT ();
        fprintf (stderr, "z8ltty: use control-\\ for stop char\r\n");
    }

    bool stdoutty = isatty (STDOUT_FILENO) > 0;
    uint64_t readnextkbat   = (nokb && (readerfile < 0)) ? 0xFFFFFFFFFFFFFFFFULL : 0;
    uint64_t setprintdoneat = 0xFFFFFFFFFFFFFFFFULL;

    // keep processing until control-backslash
    // control-C is recognized only if -nokb mode
    while (! ctrlcflag) {
        struct timeval nowtv;
        if (gettimeofday (&nowtv, NULL) < 0) ABORT ();
        uint64_t nowus = nowtv.tv_sec * 1000000ULL + nowtv.tv_usec;
        usleep (1000 - nowus % 1000);
        if (gettimeofday (&nowtv, NULL) < 0) ABORT ();
        nowus = nowtv.tv_sec * 1000000ULL + nowtv.tv_usec;

        // maybe set printing complete flag
        if (nowus >= setprintdoneat) {
            ttyat[Z_TTYPR] = PR_FLAG;
            setprintdoneat = 0xFFFFFFFFFFFFFFFFULL;
        }

        // maybe see if PDP has a character to print
        if (setprintdoneat == 0xFFFFFFFFFFFFFFFFULL) {
            uint32_t prreg = ttyat[Z_TTYPR];
            if (prreg & PR_FULL) {

                // print character to stdout
                uint8_t prchar = prreg & 0177;
                if ((prchar == 7) && stdoutty) {
                    int rc = write (STDOUT_FILENO, "<BEL>", 5);
                    if (rc < 5) ABORT ();
                } else {
                    int rc = write (STDOUT_FILENO, &prchar, 1);
                    if (rc <= 0) ABORT ();
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
                }

                // set done flag after 1000000/cps usec
                setprintdoneat = nowus + 1000000 / cps;
            }
        }

        if (nowus >= readnextkbat) {

            // chars from stdin take precedence over reader file
            // ...so user can do ctrl-\ to get back even if there is a reader file loaded
            struct pollfd polls[1] = { STDIN_FILENO, POLLIN, 0 };
            int rc = poll (polls, 1, 0);
            if ((rc < 0) && (errno != EINTR)) ABORT ();
            if ((rc >= 0) && (polls[0].revents & POLLIN)) {

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
                    readerfile = -1;
                } else {
                    ttyat[Z_TTYKB] = KB_FLAG | KB_ENAB | kbbyte | readermask;
                    if (readersize > 0) {
                        fprintf (stderr, "\r[%u/%u]", ++ readerbytes, readersize);
                    }
                    readnextkbat = nowus + 1111111 / cps;
                }
            }
        }
    }

    if (stdintty && ! nokb && (tcsetattr (STDIN_FILENO, TCSANOW, &term_original) < 0)) ABORT ();
    fprintf (stderr, "\n");

    return TCL_OK;
}
