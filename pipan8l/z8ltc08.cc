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

// Performs TC08 tape I/O for the PDP-8/L Zynq I/O board

//  ./z8ltc08 [-killit] [-loadro/-loadrw <driveno> <file>]... [<tclscriptfile>]
//  ./z8ltc08 -status [<ipaddress>]

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <tcl.h>
#include <unistd.h>

#include "tclmain.h"
#include "z8ldefs.h"
#include "z8lutil.h"

#define TC_ENABLE 0x80000000
#define TC_STATB  0x0FFF0000
#define TC_STATB0 0x00010000
#define TC_IOPEND 0x00008000
#define TC_STATA  0x00000FFF
#define TC_STATA0 0x00000001

/*
  status_a:
    | unitsel   |REV|GO |CON|  opcode   |IEN|PAF|PSF|
    +---+---+---+---+---+---+---+---+---+---+---+---+
    | 4   2   1 | 4   2   1 | 4   2   1 | 4   2   1 |

  status_b:
    |ERR|   |EOT|SEL|       | dma field |       |SUC|
    +---+---+---+---+---+---+---+---+---+---+---+---+
    | 4   2   1 | 4   2   1 | 4   2   1 | 4   2   1 |
*/

#define DBGPR if (debug > 0) dbgpr

#define MIN(a,b) (((a) < (b)) ? (a) : (b))

#define MAXDRIVES 8
#define BLOCKSPERTAPE 1474  // 02702
#define WORDSPERBLOCK 129
#define BYTESPERBLOCK (WORDSPERBLOCK*2)

#define UDPPORT 23457

#define CONTIN (status_a & 00100)
#define NORMAL (! CONTIN)

#define REVBIT 00400
#define GOBIT  00200

#define REVERS (status_a & REVBIT)
#define FORWRD (! REVERS)

#define DTFLAG 00001    // successful completion
#define ERFLAG 04000    // error completion
#define ERRORS 07700
#define ENDTAP 05000
#define SELERR 04400

#define INTENA 00004    // interrupt enable
#define INTREQ 04001    // request interrupt if error or success

#define IDWC 07754      // memory word containing 2s comp dma word count
#define IDCA 07755      // memory word containing dma address minus one

struct Drive {
    uint32_t filesize;  // size of file in bytes
    int dtfd;           // fd of file with tape contents
    uint16_t tapepos;   // current tape position
    bool locked;        // drive busy doing io, don't unload
    bool rdonly;        // read-only
    char fname[160];    // name of file
};

struct UDPPkt {
    uint64_t seq;
    uint16_t status_a;
    uint16_t status_b;
    Drive drives[MAXDRIVES];
};

// internal TCL commands
static Tcl_ObjCmdProc cmd_tcloadro;
static Tcl_ObjCmdProc cmd_tcloadrw;
static Tcl_ObjCmdProc cmd_tcunload;

static TclFunDef const fundefs[] = {
    { cmd_tcloadro,   "tcloadro",   "<drivenumber> <filename> - load file read-only" },
    { cmd_tcloadrw,   "tcloadrw",   "<drivenumber> <filename> - load file read/write" },
    { cmd_tcunload,   "tcunload",   "<drivenumber> - unload disk" },
    { NULL, NULL, NULL }
};

#define LOCKIT if (pthread_mutex_lock (&lock) != 0) ABORT ()
#define UNLKIT if (pthread_mutex_unlock (&lock) != 0) ABORT ()

static bool startdelay;
static bool volatile exiting;
static Drive drives[MAXDRIVES];
static int debug;
static pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static uint16_t ocarray[4096];
static uint16_t status_a, status_b;
static uint32_t cycles;
static uint32_t volatile *pdpat;
static uint32_t volatile *tcat;
static uint32_t volatile *xmemat;
static Z8LPage *z8p;

static int loadtape (bool readwrite, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]);
static bool loadfile (Tcl_Interp *interp, bool readwrite, int driveno, char const *filenm);
static char *lockfile (int fd, int how);
static void *thread (void *dummy);
static bool stepskip (Drive *drive);
static bool stepxfer (Drive *drive);
static void dumpbuf (Drive *drive, char const *label, uint16_t const *buff, int nwords);
static bool dmareadoverwritesinstructions (uint16_t field, uint16_t idca, uint16_t idwc);
static bool delayblk ();
static bool delayloop (int usec);
static void dbgpr (int level, char const *fmt, ...);
static bool writeformat (Tcl_Interp *interp, int fd);
static int showstatus (int argc, char **argv);
static void *udpthread (void *dummy);



int main (int argc, char **argv)
{
    // if first arg is -status, show tape status
    if ((argc >= 2) && (strcasecmp (argv[1], "-status") == 0)) {
        return showstatus (argc - 1, argv + 1);
    }

    for (int i = 0; i < MAXDRIVES; i ++) {
        drives[i].dtfd = -1;
    }

    setlinebuf (stdout);

    bool killit = false;
    bool loadit = false;
    int tclargs = argc;
    for (int i = 0; ++ i < argc;) {
        if (strcasecmp (argv[i], "-killit") == 0) {
            killit = true;
            continue;
        }
        if ((strcasecmp (argv[i], "-loadro") == 0) || (strcasecmp (argv[i], "-loadrw") == 0)) {
            if ((i + 2 >= argc) || (argv[i+1][0] == '-') || (argv[i+2][0] == '-')) {
                fprintf (stderr, "missing drivenumber and/or filename for -loadro/rw\n");
                return 1;
            }
            char *p;
            int driveno = strtol (argv[i+1], &p, 0);
            if ((*p != 0) || (driveno < 0) || (driveno >= MAXDRIVES)) {
                fprintf (stderr, "drivenumber %s must be integer in range 0..%d\n", argv[i+1], MAXDRIVES - 1);
                return 1;
            }
            drives[driveno].dtfd = i;
            drives[driveno].rdonly = strcasecmp (argv[i], "-loadro") == 0;
            loadit = true;
            i += 2;
            continue;
        }
        if (argv[i][0] == '-') {
            fprintf (stderr, "unknown option %s\n", argv[i]);
            return 1;
        }
        tclargs = i;
        break;
    }

    z8p    = new Z8LPage ();
    pdpat  = z8p->findev ("8L", NULL, NULL, false);
    tcat   = z8p->findev ("TC", NULL, NULL, true, killit);
    xmemat = z8p->findev ("XM", NULL, NULL, false);

    tcat[1] = TC_ENABLE;    // enable board to process io instructions

    char const *dbenv = getenv ("z8ltc08_debug");
    debug = (dbenv == NULL) ? 0 : atoi (dbenv);

    // initialize obverse complement lookup table
    //   abcdefghijkl <=> ~jklghidefabc
    for (int i = 0; i < 4096; i ++) {
        uint16_t reverse = 07777;
        for (int j = 0; j < 12; j += 3) {
            uint32_t threebits = (i >> j) & 7;
            reverse -= threebits << (9 - j);
        }
        ocarray[i] = reverse;
    }

    // spawn thread to send status over udp
    pthread_t udptid;
    int rc = pthread_create (&udptid, NULL, udpthread, NULL);
    if (rc != 0) ABORT ();

    // if -load option, just run io calls
    if (loadit) {
        for (int driveno = 0; driveno < MAXDRIVES; driveno ++) {
            int i = drives[driveno].dtfd;
            if (i >= 0) {
                drives[driveno].dtfd = -1;
                if (! loadfile (NULL, ! drives[driveno].rdonly, driveno, argv[i+2])) return 1;
            }
        }
        thread (NULL);
        return 0;
    }

    // spawn thread to do io
    pthread_t threadid;
    rc = pthread_create (&threadid, NULL, thread, NULL);
    if (rc != 0) ABORT ();

    // process tcl commands
    rc = tclmain (fundefs, argv[0], "z8ltc08", NULL, NULL, argc - tclargs, argv + tclargs, true);

    exiting = true;
    pthread_join (threadid, NULL);

    return rc;
}

// tcloadro <drivenumber> <filename>
static int cmd_tcloadro (ClientData clientdata, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    return loadtape (false, interp, objc, objv);
}

// tcloadrw <drivenumber> <filename>
static int cmd_tcloadrw (ClientData clientdata, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    return loadtape (true, interp, objc, objv);
}

static int loadtape (bool readwrite, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    if ((objc == 2) && (strcasecmp (Tcl_GetString (objv[2]), "help") == 0)) {
        puts ("");
        puts ("  tcloadro <drivenumber> <filenane>");
        puts ("  tcloadrw <drivenumber> <filenane>");
        return TCL_OK;
    }

    if (objc == 3) {
        int driveno;
        int rc = Tcl_GetIntFromObj (interp, objv[1], &driveno);
        if (rc != TCL_OK) return rc;
        if ((driveno < 0) || (driveno > 7)) {
            Tcl_SetResultF (interp, "drivenumber %d not in range 0..7", driveno);
            return TCL_ERROR;
        }
        char const *filenm = Tcl_GetString (objv[2]);

        return loadfile (interp, readwrite, driveno, filenm) ? TCL_OK : TCL_ERROR;
    }

    Tcl_SetResultF (interp, "tcloadro/tcloadrw <drivenumber> <filename>");
    return TCL_ERROR;
}

static bool loadfile (Tcl_Interp *interp, bool readwrite, int driveno, char const *filenm)
{
    int fd = open (filenm, readwrite ? O_RDWR | O_CREAT : O_RDONLY, 0666);
    if (fd < 0) {
        if (interp == NULL) fprintf (stderr, "error opening %s: %m\n", filenm);
        else Tcl_SetResultF (interp, "%m");
        return false;
    }
    char *lockerr = lockfile (fd, readwrite ? F_WRLCK : F_RDLCK);
    if (lockerr != NULL) {
        if (interp == NULL) fprintf (stderr, "error locking %s: %s\n", filenm, lockerr);
        else Tcl_SetResultF (interp, "%s", lockerr);
        close (fd);
        free (lockerr);
        return false;
    }
    long oldsize = lseek (fd, 0, SEEK_END);
    if (readwrite && (ftruncate (fd, BLOCKSPERTAPE * BYTESPERBLOCK) < 0)) {
        if (interp == NULL) fprintf (stderr, "error extending %s: %m\n", filenm);
        else Tcl_SetResultF (interp, "%m");
        close (fd);
        return false;
    }
    if (readwrite && (oldsize == 0) && ! writeformat (interp, fd)) {
        close (fd);
        return false;
    }
    fprintf (stderr, "loadtape: drive %d loaded with read%s file %s\n", driveno, (readwrite ? "/write" : "-only"), filenm);
    Drive *drive = &drives[driveno];
    LOCKIT;
    while (drive->locked) {
        if (pthread_cond_wait (&cond, &lock) != 0) ABORT ();
    }
    close (drive->dtfd);
    drive->filesize = lseek (fd, 0, SEEK_END);
    drive->dtfd     = fd;
    drive->rdonly   = ! readwrite;
    drive->tapepos  = 0;
    strncpy (drive->fname, filenm, sizeof drive->fname);
    drive->fname[sizeof drive->fname-1] = 0;
    UNLKIT;
    return true;
}

// tcunload <drivenumber>
static int cmd_tcunload (ClientData clientdata, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    if (objc == 2) {
        int driveno;
        int rc = Tcl_GetIntFromObj (interp, objv[1], &driveno);
        if (rc != TCL_OK) return rc;
        if ((driveno < 0) || (driveno > 7)) {
            Tcl_SetResultF (interp, "drivenumber %d not in range 0..7", driveno);
            return TCL_ERROR;
        }
        fprintf (stderr, "IODevRK8JE::scriptcmd: drive %d unloaded\n", driveno);
        LOCKIT;
        close (drives[driveno].dtfd);
        drives[driveno].dtfd = -1;
        UNLKIT;
        return TCL_OK;
    }

    Tcl_SetResultF (interp, "tcunload <drivenumber>");
    return TCL_ERROR;
}

// try to lock the given file
//  input:
//   fd = file to lock
//   how = F_WRLCK: exclusive access
//         F_RDLCK: shared access
//  output:
//   returns NULL: successful
//           else: error message
static char *lockfile (int fd, int how)
{
    struct flock flockit;

trylk:;
    memset (&flockit, 0, sizeof flockit);
    flockit.l_type   = how;
    flockit.l_whence = SEEK_SET;
    flockit.l_len    = 4096;
    if (fcntl (fd, F_SETLK, &flockit) >= 0) return NULL;

    char *errmsg = NULL;
    if (((errno == EACCES) || (errno == EAGAIN)) && (fcntl (fd, F_GETLK, &flockit) >= 0)) {
        if (flockit.l_type == F_UNLCK) goto trylk;
        if (asprintf (&errmsg, "locked by pid %d", (int) flockit.l_pid) < 0) ABORT ();
    } else {
        if (asprintf (&errmsg, "%m") < 0) ABORT ();
    }
    return errmsg;
}

static void *thread (void *dummy)
{
    bool oldgobit = false;

    while (! exiting) {
        usleep (1000);

        // check for an I/O request pending
        uint32_t status = tcat[1];
        if (status & TC_IOPEND) {
            tcat[1] = status &= ~ TC_IOPEND;

            status_a = (status & TC_STATA) / TC_STATA0;
            status_b = (status & TC_STATB) / TC_STATB0;

            DBGPR (2, "thread: start status_a=%04o status_b=%04o\n", status_a, status_b);

            bool newgobit  = (status_a / GOBIT) & 1;
            if (oldgobit  != newgobit) {
                oldgobit   = newgobit;
                startdelay = true;
            }

            uint16_t field = (status_b & 070) << 9;

            int driveno = (status_a >> 9) & 007;
            Drive *drive = &drives[driveno];

            // prevent file from being unloaded while in here so fd doesn't get munged
            LOCKIT;
            drive->locked = true;
            UNLKIT;

            // select error if no tape loaded
            if (drive->dtfd < 0) {
                status_b |= SELERR;
                DBGPR (2, "thread: no file for drive %d\n", driveno);
                goto finerror;
            }

            // if tape stopped, we can't do anything
            if (! newgobit) goto unlock;

            // blocks are conceptually stored:
            //  fwdmark          revmark
            //    <n>   <data-n>   <n>       - where 00000<=n<=02701
            //  ^     ^          ^     ^     - the tape head is at any one of these positions
            //  a     b          c     d     - letter shown on tc08status.cc output
            //  0+4n  1+4n       2+4n  3+4n  - value stored in tapepos
            // tapepos = 0 : at very beginning of tape
            // tapepos = BLOCKSPERTAPE*4-1 : at very end of tape
            // the file just contains the data, as 129 16-bit words, rjzf * 1474 blocks
            // seek forward always stops at 'b' or end-of-tape
            // seek reverse always stops at 'c' or beg-of-tape
            // read/write forward always stops at 'c' or end-of-tape
            // read/write reverse always stops at 'b' or beg-of-tape
            ASSERT (drive->tapepos < BLOCKSPERTAPE*4);

            switch ((status_a & 070) >> 3) {

                // MOVE (2.5.1.4 p 27)
                case 0: {

                    // step the first one with startup delay if needed
                    if (delayblk ()) goto finished;
                    if (stepskip (drive)) goto endtape;

                    // spend at most 5 seconds to rewind tape
                    uint16_t blknum   = drive->tapepos / 4;
                    uint16_t blkstogo = REVERS ? blknum : BLOCKSPERTAPE - blknum;
                    uint32_t usperblk = (blkstogo < 200) ? 25000 : 5000000 / blkstogo;

                    // skip the rest of the way
                    while (true) {
                        if (delayloop (usperblk)) goto finished;
                        if (stepskip (drive)) goto endtape;
                    }
                }

                // SEARCH (2.5.1.4 p 27)
                case 1: {
                    uint32_t cm = 0;
                    do {
                        // update tape position for the search
                        if (delayblk ()) goto finished;
                        if (stepskip (drive)) goto endtape;

                        // increment word count and write new tape position to memory
                        uint32_t tp = drive->tapepos / 4;
                        if (debug >= 3) {
                            uint16_t wc = (z8p->dmacycle (CM_ENAB | IDWC * CM_ADDR0, 0) & CM_DATA) / CM_DATA0;
                            uint16_t ca = (z8p->dmacycle (CM_ENAB | IDCA * CM_ADDR0, 0) & CM_DATA) / CM_DATA0;
                            DBGPR (3, "thread: skip writing tp=%04o to wc=%04o ca=%04o\n", tp, wc, ca);
                        }
                        cm = z8p->dmacycle (CM_ENAB | tp * CM_DATA0 | CM_WRITE | (field | IDWC) * CM_ADDR0, CM2_3CYCL);
                        DBGPR (2, "thread: skip cm=%08X tp=%04o\n", cm, tp);
                    } while (CONTIN && ! (cm & CM_WCOVF));
                    goto success;
                }

                // READ DATA (2.5.1.5 p 27)
                case 2: {
                    uint32_t cm = 0;
                    do {
                        // update tape position for the read
                        if (delayblk ()) goto finished;
                        if (stepxfer (drive)) goto endtape;

                        // read data from tape file
                        uint16_t buff[WORDSPERBLOCK];
                        int rc = pread (drive->dtfd, buff, BYTESPERBLOCK, drive->tapepos / 4 * BYTESPERBLOCK);
                        if (rc < 0) {
                            fprintf (stderr, "thread: error reading tape %d file: %m\n", driveno);
                            ABORT ();
                        }
                        if (rc != BYTESPERBLOCK) {
                            fprintf (stderr, "thread: only read %d of %d bytes from tape %d file\n", rc, BYTESPERBLOCK, driveno);
                            ABORT ();
                        }
                        if (debug >= 3) dumpbuf (drive, "read", buff, WORDSPERBLOCK);

                        DBGPR (2, "thread: read block=%04o\n", drive->tapepos / 4);
                        if (REVERS) {
                            for (int i = WORDSPERBLOCK; -- i >= 0;) {
                                uint16_t ocdat = buff[i] & 07777;
                                uint16_t data  = ocarray[ocdat];
                                DBGPR (4, "thread:  rev memory[] = %04o = ocarray[%04o]\n", data, ocdat);
                                cm = z8p->dmacycle (CM_ENAB | data * CM_DATA0 | CM_WRITE | (field | IDWC) * CM_ADDR0, CM2_3CYCL | CM2_CAINC);
                                if (cm & CM_WCOVF) break;
                            }
                        } else {
                            // madness: TC08 OS/8 boot block changes from data field 0 to data field 1 mid-read
                            // so pass data s-l-o-w-l-y when booting
                            uint16_t idwc = (z8p->dmacycle (CM_ENAB | IDWC * CM_ADDR0, 0) & CM_DATA) / CM_DATA0;
                            uint16_t idca = (z8p->dmacycle (CM_ENAB | IDCA * CM_ADDR0, 0) & CM_DATA) / CM_DATA0;
                            bool slow = dmareadoverwritesinstructions (field, idca, idwc);
                            for (int i = 0; i < WORDSPERBLOCK; i ++) {
                                if (slow) {
                                    cycles = pdpat[Z_RN];                   // wait for processor to run 100 cycles
                                    delayloop (1000);                       // ... at least 33 instructions
                                    status   = tcat[1];
                                    status_b = (status & TC_STATB) / TC_STATB0;
                                    field    = (status_b & 070) << 9;
                                }
                                if (debug >= 4) {
                                    idwc = (z8p->dmacycle (CM_ENAB | IDWC * CM_ADDR0, 0) & CM_DATA) / CM_DATA0;
                                    idca = (z8p->dmacycle (CM_ENAB | IDCA * CM_ADDR0, 0) & CM_DATA) / CM_DATA0;
                                }
                                uint16_t data = buff[i] & 07777;
                                cm = z8p->dmacycle (CM_ENAB | data * CM_DATA0 | CM_WRITE | (field | IDWC) * CM_ADDR0, CM2_3CYCL | CM2_CAINC);
                                DBGPR (4, "thread:  %s idwc=%04o idca=%04o memory[%o.%04o] = %04o => wcovf=%o\n",
                                    (slow ? "slo" : "fwd"), idwc, idca, field >> 12, (idca + 1) & 07777, data, (cm / CM_WCOVF) & 1);
                                if (cm & CM_WCOVF) break;
                            }
                        }
                    } while (CONTIN && ! (cm & CM_WCOVF));
                    goto success;
                }

                // READ ALL
                // - runs MAINDEC D3RA test (forward and reverse)
                //   let it run at least 2 passes over tape to get some read-all-reverses
                case 3: {
                    uint32_t cm = 0;
                    do {
                        if (delayblk ()) goto finished;
                        if (stepxfer (drive)) goto endtape;

                        // read 129 data words from file
                        // leave room for header words
                        uint16_t buff[5+WORDSPERBLOCK+5];
                        uint16_t blknum = drive->tapepos / 4;
                        uint16_t *databuff = &buff[5];
                        int rc = pread (drive->dtfd, databuff, BYTESPERBLOCK, blknum * BYTESPERBLOCK);
                        if (rc < 0) {
                            fprintf (stderr, "thread: error reading tape %d file: %m\n", driveno);
                            ABORT ();
                        }
                        if (rc != BYTESPERBLOCK) {
                            fprintf (stderr, "thread: only read %d of %d bytes from tape %d file\n", rc, BYTESPERBLOCK, driveno);
                            ABORT ();
                        }

                        // make up header words tacked on beginning and end of data words
                        ASSERT (blknum <= 07777);
                        if (REVERS) {
                            buff[WORDSPERBLOCK+9] = 0;
                            buff[WORDSPERBLOCK+8] = ocarray[blknum];
                            buff[WORDSPERBLOCK+7] = 0;
                            buff[WORDSPERBLOCK+6] = 0;
                            buff[WORDSPERBLOCK+5] = 07700;
                            uint16_t parity = 0;
                            for (int i = 0; i < WORDSPERBLOCK; i ++) parity ^= databuff[i] ^ (databuff[i] >> 6);
                            buff[4] = parity & 00077;
                            buff[3] = 0;
                            buff[2] = 0;
                            buff[1] = blknum;
                            buff[0] = 0;
                        } else {
                            buff[0] = 0;
                            buff[1] = blknum;
                            buff[2] = 0;
                            buff[3] = 0;
                            buff[4] = 00077;
                            uint16_t parity = 0;
                            for (int i = 0; i < WORDSPERBLOCK; i ++) parity ^= databuff[i] ^ (databuff[i] << 6);
                            buff[WORDSPERBLOCK+5] = parity & 07700;
                            buff[WORDSPERBLOCK+6] = 0;
                            buff[WORDSPERBLOCK+7] = 0;
                            buff[WORDSPERBLOCK+8] = ocarray[blknum];
                            buff[WORDSPERBLOCK+9] = 0;
                        }

                        if (debug >= 3) dumpbuf (drive, "rall", buff, WORDSPERBLOCK + 10);

                        // copy out to dma buffer
                        DBGPR (2, "thread: rall block=%04o\n", blknum);
                        if (REVERS) {
                            for (int i = 5 + WORDSPERBLOCK + 5; -- i >= 0;) {
                                uint16_t ocdat = buff[i] & 07777;
                                uint16_t data  = ocarray[ocdat];
                                DBGPR (4, "thread:  rev memory[] = %04o = ocarray[%04o]\n", data, ocdat);
                                cm = z8p->dmacycle (CM_ENAB | data * CM_DATA0 | CM_WRITE | (field | IDWC) * CM_ADDR0, CM2_3CYCL | CM2_CAINC);
                                if (cm & CM_WCOVF) break;
                            }
                        } else {
                            uint16_t idwc = 0, idca = 0;
                            for (int i = 0; i < 5 + WORDSPERBLOCK + 5; i ++) {
                                if (debug >= 4) {
                                    idwc = (z8p->dmacycle (CM_ENAB | IDWC * CM_ADDR0, 0) & CM_DATA) / CM_DATA0;
                                    idca = (z8p->dmacycle (CM_ENAB | IDCA * CM_ADDR0, 0) & CM_DATA) / CM_DATA0;
                                }
                                uint16_t data = buff[i] & 07777;
                                cm = z8p->dmacycle (CM_ENAB | data * CM_DATA0 | CM_WRITE | (field | IDWC) * CM_ADDR0, CM2_3CYCL | CM2_CAINC);
                                DBGPR (4, "thread:  fwd idwc=%04o idca=%04o memory[%o.%04o] = %04o => wcovf=%o\n",
                                    idwc, idca, field >> 12, (idca + 1) & 07777, data, (cm / CM_WCOVF) & 1);
                                if (cm & CM_WCOVF) break;
                            }
                        }
                    } while (CONTIN && ! (cm & CM_WCOVF));
                    goto success;
                }

                // WRITE DATA (2.5.1.7 p 28)
                case 4: {
                    uint32_t cm = 0;
                    if (drive->rdonly) {
                        DBGPR (2, "thread: write attempt on read-only drive %d\n", driveno);
                        status_b |= SELERR;
                        goto finerror;
                    }

                    do {
                        if (delayblk ()) goto finished;
                        if (stepxfer (drive)) goto endtape;

                        uint16_t buff[WORDSPERBLOCK];
                        DBGPR (2, "thread: write block=%04o\n", drive->tapepos / 4);
                        if (REVERS) {
                            for (int i = WORDSPERBLOCK; i > 0;) {
                                cm = z8p->dmacycle (CM_ENAB | (field | IDWC) * CM_ADDR0, CM2_3CYCL | CM2_CAINC);
                                uint16_t data = (cm & CM_DATA) / CM_DATA0;
                                buff[--i] = ocarray[data];
                                if (cm & CM_WCOVF) {
                                    while (-- i >= 0) buff[i] = 07777;
                                    break;
                                }
                            }
                        } else {
                            for (int i = 0; i < WORDSPERBLOCK;) {
                                cm = z8p->dmacycle (CM_ENAB | (field | IDWC) * CM_ADDR0, CM2_3CYCL | CM2_CAINC);
                                uint16_t data = (cm & CM_DATA) / CM_DATA0;
                                buff[i++] = data;
                                if (cm & CM_WCOVF) {
                                    memset (&buff[i], 0, (WORDSPERBLOCK - i) * 2);
                                    break;
                                }
                            }
                        }

                        if (debug >= 3) dumpbuf (drive, "write", buff, WORDSPERBLOCK);
                        int rc = pwrite (drive->dtfd, buff, BYTESPERBLOCK, drive->tapepos / 4 * BYTESPERBLOCK);
                        if (rc < 0) {
                            fprintf (stderr, "thread: error writing tape %d file: %m\n", driveno);
                            ABORT ();
                        }
                        if (rc != BYTESPERBLOCK) {
                            fprintf (stderr, "thread: only wrote %d of %d bytes to tape %d file\n", rc, BYTESPERBLOCK, driveno);
                            ABORT ();
                        }
                    } while (CONTIN && ! (cm & CM_WCOVF));
                    goto success;
                }

                default: {
                    fprintf (stderr, "thread: unsupported command %u\n", ((status_a & 00070) >> 3));
                    status_b |= SELERR;
                    goto finerror;
                }
            }
            ABORT ();
        endtape:;
            DBGPR (2, "thread: - end of tape\n");
            status_b |=  ENDTAP;                // set up end-of-tape status
        finerror:;
            tcat[1] &= ~ (GOBIT * TC_STATA0);   // any error shuts the GO bit off
            goto finished;
        success:;
            status_b |=  DTFLAG;                // errors do not get DTFLAG
            // if normal mode with non-zero remaining IDWC,
            //  real dectapes will start the next transfer when the next bits come under the tape head in a few milliseconds
            //    failing with timing error if DTFLAG has not been cleared by the processor by then
            //  in our case, we process the next block when the processor clears DTFLAG because the tubes may take a while to clear DTFLAG
        finished:;
            DBGPR (1, "thread: done st_A=%04o st_B=%04o\n", status_a, status_b);
            tcat[1] = (tcat[1] & ~ (07707 * TC_STATB0)) | ((status_b & 07707) * TC_STATB0);

            // drive can now be unloaded
        unlock:;
            LOCKIT;
            drive->locked = false;
            if (pthread_cond_broadcast (&cond) != 0) ABORT ();
            UNLKIT;
        }
    }

    return NULL;
}

// step tape just before data in the current tape direction
//  returns false: tapepos updated as described
//           true: hit end-of-tape
//                 tapepos set to whichever end we're at
static bool stepskip (Drive *drive)
{
    if (REVERS) {
        if (drive->tapepos <= 2) {
            drive->tapepos = 0;
            return true;
        }
        switch (drive->tapepos & 3) {
            case 0: drive->tapepos -= 2; break;
            case 1: drive->tapepos -= 3; break;
            case 2: drive->tapepos -= 4; break;
            case 3: drive->tapepos --;   break;
        }
    } else {
        switch (drive->tapepos & 3) {
            case 0: drive->tapepos ++;   break;
            case 1: drive->tapepos += 4; break;
            case 2: drive->tapepos += 3; break;
            case 3: drive->tapepos += 2; break;
        }
        if (drive->tapepos >= BLOCKSPERTAPE*4) {
            drive->tapepos = BLOCKSPERTAPE*4 - 1;
            return true;
        }
    }
    return false;
}

// step tape just after data in the current tape direction
//  returns false: tapepos updated as described
//           true: hit end-of-tape
//                 tapepos set to whichever end we're at
static bool stepxfer (Drive *drive)
{
    if (REVERS) {
        if (drive->tapepos <= 1) {
            drive->tapepos = 0;
            return true;
        }
        switch (drive->tapepos & 3) {
            case 0: drive->tapepos -= 3; break;
            case 1: drive->tapepos -= 4; break;
            case 2: drive->tapepos --;   break;
            case 3: drive->tapepos -= 2; break;
        }
    } else {
        switch (drive->tapepos & 3) {
            case 0: drive->tapepos += 2; break;
            case 1: drive->tapepos ++;   break;
            case 2: drive->tapepos += 4; break;
            case 3: drive->tapepos += 3; break;
        }
        if (drive->tapepos >= BLOCKSPERTAPE*4) {
            drive->tapepos = BLOCKSPERTAPE*4 - 1;
            return true;
        }
    }
    return false;
}

// dump block buffer
static void dumpbuf (Drive *drive, char const *label, uint16_t const *buff, int nwords)
{
    fprintf (stderr, "dumpbuf: %o  %5s %s  block %04o\n", (int) (drive - drives), label, (REVERS ? "REV" : "FWD"), drive->tapepos / 4);
    for (int i = 0; i < nwords; i += 16) {
        fprintf (stderr, "  %04o:", i);
        for (int j = 0; (j < 16) && (i + j < nwords); j ++) {
            fprintf (stderr, " %04o", buff[i+j]);
        }
        fprintf (stderr, "\n");
    }
}

// see if a dma read will overwrite instructions currently executing
//  input:
//   field = dma transfer field in <14:12>, rest is zeroes
//   idca  = start of transfer - 1
//   idwc  = 2's comp transfer word count
static bool dmareadoverwritesinstructions (uint16_t field, uint16_t idca, uint16_t idwc)
{
    // field must match exactly
    uint16_t curif = (xmemat[2] & XM2_IFLD) / XM2_IFLD0;
    if (curif != field >> 12) return false;

    // current PC must be within transfer area
    // we use the MA cuz PDP-8/L doesn't make PC available
    // but if it's doing a JMP . in the boot code, PC should be the only thing in there
    uint16_t endaddr = (idca - idwc) & 07777;
    idca = (idca + 1) & 07777;
    uint16_t curpc = (pdpat[Z_RI] & i_oMA) / i_oMA0;
    if (endaddr >= idca) {
        if ((curpc < idca) || (curpc > endaddr)) return false;
    } else {
        if ((curpc < idca) && (curpc > endaddr)) return false;
    }
    return true;
}

// delay processing for a block
//  returns:
//   false = it's ok to keep going as is
//    true = something changed, re-decode
static bool delayblk ()
{
    // 25ms standard per-block delay
    int usec = 25000;

    // 375ms additional for startup delay
    if (startdelay) {
        startdelay = false;
        usec += 375000;
    }

    return delayloop (usec);
}

// delay the given number of microseconds, unlocking during the wait
// check for changed command during the wait
// also make sure processor has executed at least 100 cycles since i/o instruction
// ...so processor has time to set up dma descriptor words after starting the i/o
//  output:
//   returns true: command changed during wait
//          false: command remained the same
static bool delayloop (int usec)
{
    while (! exiting) {

        // wait
        usleep (usec);

        // the TC08 diagnostics do the I/O instruction to start the I/O THEN write IDCA and IDWC,
        // so make sure CPU has executed at least 100 cycles (20..30 instructions)
        uint32_t ncycles = pdpat[Z_RN] - cycles;                // see how many cycles since I/O started
        if (ncycles >= 100) return false;                       // if 100 cycles, assume IDCA,IDWC set up

        // not yet, wait another millisecond
        usec = 1000;
    }

    // tell caller to abort if exiting
    return true;
}

// print message if debug is at or above the given level
static void dbgpr (int level, char const *fmt, ...)
{
    if (debug >= level) {
        va_list ap;
        va_start (ap, fmt);
        vfprintf (stderr, fmt, ap);
        va_end (ap);
    }
}

// contents of a freshly zeroed dectape in OS/8
static uint16_t const formatofs = 0001000;
static uint16_t const formatbuf[] = {
    000000, 000000, 007777, 000007, 000000, 000000, 007777, 000000,
    006446, 000000, 003032, 001777, 003024, 005776, 001375, 003267,
    006201, 001744, 003667, 007040, 001344, 003344, 007040, 001267,
    002020, 005215, 007200, 006211, 005607, 000201, 000440, 002331,
    002324, 000515, 004010, 000501, 000400, 001774, 001373, 003774,
    001774, 000372, 001371, 003770, 001263, 003767, 003032, 001777,
    003024, 006202, 004424, 000200, 003400, 000007, 005766, 004765,
    005776, 000000, 001032, 007650, 005764, 005667, 001725, 002420,
    002524, 004005, 002222, 001722, 000000, 003344, 001763, 003267,
    003204, 007346, 003207, 001667, 007112, 007012, 007012, 000362,
    007440, 004761, 002204, 005340, 002267, 002207, 005311, 001344,
    007640, 005760, 001667, 007650, 005760, 001357, 004756, 002344,
    007240, 005310, 007240, 003204, 001667, 005315, 000000, 003020,
    001411, 003410, 002020, 005346, 005744, 000000, 000000, 000000,
    005172, 000256, 004620, 004627, 000077, 005533, 005622, 006443,
    003110, 007622, 007621, 006360, 000017, 007760, 007617, 006777,
    002600, 006023, 000000, 000000, 001777, 003024, 001776, 003025,
    003775, 003774, 003773, 001776, 003772, 005600, 000000, 007346,
    004771, 001770, 007041, 001027, 007500, 005233, 003012, 001770,
    007440, 004771, 003410, 002012, 005227, 005613, 003012, 001027,
    007440, 004771, 001011, 001012, 003011, 005613, 000000, 006201,
    001653, 006211, 001367, 007640, 005766, 005643, 003605, 000000,
    004765, 007700, 004764, 000005, 001025, 001363, 007650, 002032,
    005654, 000000, 003254, 001762, 007650, 005761, 001254, 007650,
    004760, 005666, 000000, 003200, 001357, 003030, 006211, 004765,
    007700, 005315, 006202, 004600, 000210, 001400, 000001, 005756,
    003007, 001755, 001354, 007650, 005326, 001200, 001363, 007640,
    005330, 001353, 003030, 005677, 003205, 002217, 004023, 003123,
    007700, 007700, 005342, 004764, 000002, 004764, 000000, 001352,
    003751, 005750, 000000, 002030, 002014, 003205, 000070, 007710,
    001401, 005626, 000007, 003700, 003023, 007600, 000171, 003522,
    002473, 006141, 000600, 007204, 006344, 002215, 002223, 002222,
    002352, 006065, 006023, 000000, 003034, 007240, 001200, 003224,
    001377, 003624, 001776, 000375, 007650, 005600, 001374, 004773,
    005600, 001723, 005770, 004020, 001120, 004026, 006164, 000100,
    003033, 003506, 001772, 007450, 007001, 000371, 007041, 005625,
    000000, 001770, 003246, 006202, 004646, 000011, 000000, 007667,
    006212, 005634, 000000, 007643, 000000, 000000, 000000, 000000,
    000000, 000000, 000000, 000000, 000000, 000000, 000000, 000000,
    000000, 000000, 000000, 000000, 000000, 000000, 000000, 000000,
    000000, 000000, 000000, 000000, 000000, 000000, 000000, 000000,
    000000, 000000, 000000, 000000, 001367, 001354, 000275, 006443,
    000000, 000000, 000000, 000000, 000000, 000000, 000000, 000000,
    000000, 000000, 000000, 000000, 000000, 000000, 000000, 000000,
    000000, 000000, 000000, 000000, 000000, 000000, 000000, 000000,
    000000, 000000, 000000, 000000, 000000, 000000, 000000, 000000,
    000000, 000000, 000000, 000000, 000000, 000000, 000000, 000000,
    000000, 000000, 000000, 000000, 003457, 000077, 007646, 004600,
    006615, 000007, 007644, 000020, 000000, 005402, 002036, 004777,
    000013, 006201, 001433, 006211, 001376, 007640, 005225, 001234,
    003775, 001235, 003243, 001236, 003242, 003637, 001244, 003640,
    001311, 003641, 004774, 007240, 003773, 004772, 001771, 003034,
    005600, 000174, 007747, 000005, 005422, 006033, 005636, 000017,
    007756, 005242, 000000, 003252, 007344, 004770, 006201, 000000,
    006211, 007414, 004767, 005645, 000000, 001304, 007440, 001020,
    007640, 005305, 001020, 007041, 003304, 001033, 003022, 001304,
    004766, 007650, 005311, 002022, 006201, 001422, 006211, 004765,
    005657, 000000, 004764, 006677, 004764, 006330, 007600, 004764,
    006652, 007200, 004764, 006667, 004764, 007200, 004311, 001414,
    000507, 000114, 004052, 004017, 002240, 007700, 004017, 002024,
    001117, 001640, 002516, 001316, 001727, 001600, 004302, 000104,
    004023, 002711, 002403, 001040, 001720, 002411, 001716, 000000,
    000000, 000000, 000000, 000000, 000000, 000000, 000000, 000000,
    000000, 004200, 007324, 006305, 007400, 002670, 002220, 006433,
    002723, 006713, 002724, 002600, 000200, 000000, 004302, 000104,
    004005, 003024, 000516, 002311, 001716, 000000, 005410, 007240,
    003244, 003030, 003031, 004777, 001376, 007650, 005231, 003247,
    001375, 004774, 006211, 000023, 006201, 006773, 005610, 007201,
    001773, 001247, 007640, 005245, 001023, 003030, 002244, 005245,
    001024, 005214, 007777, 004772, 006750, 000000, 004771, 005647,
    000000, 003315, 001770, 007006, 007700, 005767, 001766, 007640,
    005767, 001765, 007650, 001770, 000364, 007650, 005274, 001763,
    000362, 003765, 001715, 007450, 004761, 003715, 002315, 001715,
    007640, 005652, 001770, 000364, 007740, 007240, 001360, 004774,
    006201, 006773, 006211, 000000, 005652, 006061, 007106, 007006,
    007006, 005717, 000000, 007450, 005724, 003332, 004732, 005724,
    000000, 001023, 000357, 007650, 005756, 004755, 004772, 007342,
    004017, 002024, 001117, 001640, 000115, 000211, 000725, 001725,
    002300, 000000, 000000, 004240, 007474, 000077, 007775, 006525,
    000017, 007617, 000200, 007600, 002723, 007111, 002537, 003426,
    004200, 006103, 002670, 007774, 007506, 006000, 000000, 000000,
    001777, 007012, 007630, 001376, 001375, 003234, 001634, 007640,
    005600, 004774, 000012, 000000, 000000, 000000, 005773, 001215,
    003634, 001234, 000376, 007650, 005600, 002234, 001372, 004771,
    006201, 006773, 006211, 007600, 005600, 000000, 003304, 004770,
    003306, 004307, 005767, 007240, 001347, 003305, 004307, 007410,
    005766, 006201, 001705, 002305, 000365, 007640, 005253, 001705,
    006211, 000364, 007440, 004763, 001306, 003032, 001032, 001362,
    007650, 004761, 005636, 001023, 007112, 007012, 007012, 001256,
    000365, 001256, 005262, 000000, 000000, 000000, 000000, 001360,
    003351, 007346, 003350, 006201, 001704, 006211, 002304, 007450,
    005707, 003347, 001751, 007450, 005345, 000365, 007640, 001365,
    001357, 006201, 000747, 006211, 007041, 001751, 007640, 005310,
    002347, 002351, 002350, 005323, 002307, 005707, 000000, 000000,
    000000, 000000, 000000, 000000, 000000, 000000, 007700, 000023,
    003416, 007506, 006400, 000377, 000077, 007333, 006352, 006000,
    002670, 007774, 006615, 000200, 007600, 000005, 002537, 000000,
    000013, 000000, 000000, 000000, 000000, 000000, 000000, 000000,
    000000, 000000, 000000, 000000, 000000, 000000, 000000, 000000,
    000000, 000000, 000000, 000000, 000000, 000000, 000000, 000000,
    000000, 000000, 000000, 000000, 000000, 000000, 000000, 000000,
    000000, 000000, 004000, 000000, 000000, 002000, 000000, 007607,
    007621, 007607, 007621, 000000, 000000, 000000, 000000, 000000,
    000000, 007210, 007211, 000000, 000000, 000000, 000000, 006202,
    004207, 001000, 000000, 000007, 007746, 006203, 005677, 000400,
    003461, 003340, 006214, 001275, 003336, 006201, 001674, 007010,
    006211, 007630, 005321, 006202, 004207, 005010, 000000, 000027,
    007402, 006202, 004207, 000610, 000000, 000013, 007602, 005020,
    006202, 004207, 001010, 000000, 000027, 007402, 006213, 005700,
    000000, 000000, 000000, 000000, 000000, 000000, 000000, 000000,
    000000, 000000, 000000, 000000, 000000, 000000, 000000, 000000,
    004230, 004230, 004230, 004230, 004230, 004230, 004230, 004230,
    000000, 001040, 004160, 004160, 001020, 002010, 000000, 000000,
    000000, 000000, 000070, 000002, 001472, 007777, 000102, 002314,
    000000, 002326, 003017, 007773, 000303, 001400, 000000, 000000,
    000000, 001441, 003055, 001573, 003574, 001572, 003442, 000000,
    007004, 007720, 001572, 001043, 007650, 005547, 005444, 000564,
    000772, 000000, 007665, 001010, 001565, 000000, 000000, 000000,
    007661, 007776, 000000, 000017, 000000, 000000, 000013, 000000,
    004000, 006202, 004575, 000210, 001000, 000015, 007402, 005467,
    001011, 002224, 000000, 002326, 003457, 007747, 002205, 002317,
    000400, 007741, 000070, 007642, 007400, 007643, 007760, 007644,
    006001, 000323, 007773, 007770, 000252, 000701, 000767, 000610,
    000760, 001403, 007666, 000371, 006006, 000651, 000752, 000007,
    000377, 001273, 000613, 000622, 000251, 004210, 001402, 000343,
    007774, 001404, 001400, 000250, 001401, 007757, 007646, 000771,
    000247, 000015, 000017, 000617, 000177, 000246, 007600, 000100,
    000321, 000544, 000240, 000020, 000563, 007761, 007740, 000620,
    000220, 007730, 007746, 007736, 007700, 000200, 007607, 007764,
    006203, 000000, 003461, 003055, 003040, 006214, 001177, 003345,
    001345, 003241, 004220, 002200, 007100, 001176, 007430, 005247,
    001244, 003220, 000467, 001200, 004343, 005620, 000247, 000401,
    000603, 001011, 001177, 000263, 000306, 000253, 000324, 000341,
    000400, 001343, 000620, 006213, 003600, 006213, 005640, 002057,
    002057, 002057, 002057, 002057, 002057, 007200, 006202, 004575,
    000210, 001400, 000056, 007402, 005657, 007330, 004351, 004220,
    003041, 006202, 004575, 000601, 000000, 000051, 005246, 001241,
    006203, 004574, 003241, 001241, 003345, 004351, 007120, 005637,
    004220, 003041, 006202, 004575, 000101, 007400, 000057, 005246,
    001041, 006203, 005713, 007200, 002200, 001040, 007110, 001200,
    003573, 001241, 003572, 007221, 006201, 000571, 006211, 007010,
    007730, 005572, 005570, 007120, 005325, 000223, 003240, 006213,
    001640, 006213, 005743, 000000, 001374, 003364, 006201, 001571,
    006211, 007012, 007630, 005751, 006202, 004575, 000000, 000000,
    000033, 005246, 005751, 006202, 004575, 000111, 001000, 000026,
    005246, 005774, 000000, 007201, 003054, 001055, 007440, 005246,
    004567, 002574, 007450, 005566, 003052, 004567, 007450, 005221,
    001052, 007004, 007130, 003052, 001165, 003017, 001164, 003042,
    001417, 007041, 001052, 007650, 005241, 002042, 005225, 001017,
    007700, 005566, 001163, 005222, 001042, 001162, 004561, 004567,
    002574, 004560, 007450, 001442, 007450, 005557, 007044, 007620,
    001156, 001156, 003301, 001041, 003047, 001441, 001054, 007640,
    005332, 004567, 007010, 007520, 005557, 007004, 000155, 003302,
    004334, 003303, 006202, 004575, 000100, 007200, 000021, 005554,
    001164, 003044, 001044, 004560, 007040, 001302, 007130, 001301,
    007700, 003441, 004334, 007041, 001303, 007640, 005330, 001442,
    000153, 001302, 003441, 002044, 005307, 001447, 005552, 000520,
    001442, 007106, 007006, 007006, 000151, 001150, 005734, 000511,
    000151, 007450, 005547, 003052, 001052, 001146, 003042, 001052,
    001145, 003041, 001052, 001144, 003050, 001441, 005744, 004631,
    005723, 006373, 006473, 006374, 006474, 006375, 006475, 005524,
    004020, 004604, 004605, 000000, 004024, 004224, 000000, 007100,
    004222, 005213, 004301, 005220, 001045, 007041, 001543, 004561,
    002574, 001046, 007041, 004561, 002574, 005557, 000000, 007440,
    005251, 003045, 003046, 001055, 004560, 007450, 005542, 003051,
    004567, 003044, 007420, 007030, 007012, 000450, 007640, 005220,
    001450, 007700, 005622, 002222, 007201, 003367, 001051, 000153,
    007106, 007004, 001367, 007041, 001007, 007450, 005270, 007041,
    001007, 003007, 007330, 004360, 001541, 007064, 007530, 005370,
    007070, 003053, 001140, 003017, 005622, 000000, 001417, 007650,
    005340, 007240, 001017, 003017, 001137, 003046, 001044, 003047,
    001047, 004536, 007041, 001417, 007640, 005335, 002047, 002046,
    005314, 004352, 001417, 007450, 005341, 007041, 003046, 002301,
    005701, 001046, 007001, 004352, 001417, 001045, 003045, 002053,
    005302, 003045, 001535, 007440, 005251, 005701, 000000, 001540,
    007041, 001017, 003017, 005752, 000000, 001134, 003365, 006202,
    004451, 004210, 001400, 000001, 005533, 005760, 000000, 000000,
    000000, 000000, 000222, 000223, 000000, 000224, 000225, 004576,
    000605, 001010, 001011, 001200, 001312, 000000, 003056, 004532,
    005531, 004530, 003042, 001140, 003017, 001055, 007112, 007012,
    000127, 007041, 003043, 001450, 000126, 007640, 005566, 001417,
    007650, 005343, 007346, 004525, 001417, 001042, 003042, 002053,
    005232, 001056, 007640, 005263, 001535, 007440, 005524, 001044,
    004536, 007640, 001046, 007650, 005566, 001054, 002056, 005524,
    001017, 003041, 001137, 004525, 001540, 007041, 001017, 001123,
    007700, 005522, 001441, 003417, 007240, 001041, 003041, 007344,
    001017, 003017, 001041, 007161, 001052, 007640, 005275, 001137,
    003041, 001052, 003017, 001044, 004536, 003417, 002044, 002041,
    005316, 001521, 003417, 007001, 004525, 003417, 001017, 003520,
    007240, 001541, 003541, 001450, 001054, 003450, 004517, 005516,
    001417, 007141, 003041, 001043, 001046, 007200, 001043, 007450,
    007020, 001041, 007220, 001041, 007041, 001046, 007670, 005374,
    001041, 003046, 007344, 001017, 003052, 001515, 003054, 001042,
    003045, 001041, 007041, 005240, 004532
};

static bool writeformat (Tcl_Interp *interp, int fd)
{
    int rc = pwrite (fd, formatbuf, sizeof formatbuf, formatofs);
    if (rc != (int) sizeof formatbuf) {
        if (interp == NULL) {
            if (rc < 0) fprintf (stderr, "error writing format: %m\n");
            else fprintf (stderr, "only wrote %d of %d-byte format\n", rc, (int) sizeof formatbuf);
        } else {
            if (rc < 0) Tcl_SetResultF (interp, "error writing format: %m");
            else Tcl_SetResultF (interp, "only wrote %d of %d-byte format", rc, (int) sizeof formatbuf);
        }
        return false;
    }
    return true;
}



// display drive status

#define ESC_HOMEC "\033[H"          // home cursor
#define ESC_NORMV "\033[m"          // go back to normal video
#define ESC_REVER "\033[7m"         // turn reverse video on
#define ESC_UNDER "\033[4m"         // turn underlining on
#define ESC_BLINK "\033[5m"         // turn blink on
#define ESC_BOLDV "\033[1m"         // turn bold on
#define ESC_REDBG "\033[41m"        // red background
#define ESC_YELBG "\033[44m"        // yellow background
#define ESC_EREOL "\033[K"          // erase to end of line
#define ESC_EREOP "\033[J"          // erase to end of page

static char const *const funcmnes[8] = { "MOVE", "SRCH", "RDAT", "RALL", "WDAT", "WALL", "WTIM", "ERR7" };

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

    uint64_t seq = 0;

    setvbuf (stdout, outbuf, _IOFBF, sizeof outbuf);

    sigset_t sigintmask;
    sigemptyset (&sigintmask);
    sigaddset (&sigintmask, SIGINT);

    char bargraphs[MAXDRIVES*66];
    uint32_t filesizes[MAXDRIVES];
    memset (bargraphs, 0, sizeof bargraphs);
    memset (filesizes, -1, sizeof filesizes);
    int twirly = 0;

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

        if (pthread_sigmask (SIG_BLOCK, &sigintmask, NULL) != 0) ABORT ();

        // decode and print status line
        int driveno = (udppkt.status_a >> 9) & 7;
        int func    = (udppkt.status_a >> 3) & 7;
        bool go     = (udppkt.status_a & 00200) != 0;
        bool rev    = (udppkt.status_a & 00400) != 0;
        printf (ESC_HOMEC ESC_EREOL "\nstatus_A %04o <%o %s %s %s %s %s>  status_B %04o" ESC_EREOL "\n",
            udppkt.status_a, driveno, (rev ? "REV" : "FWD"), (go ? " GO " : "STOP"),
            ((udppkt.status_a & 000100) ? "CON" : "NOR"), funcmnes[func], ((udppkt.status_a & 00004) ? "IENA" : "IDIS"),
            udppkt.status_b);

        // display line for each drive with a tape file loaded
        char rwfc = (func != 0) ? "  rRwWW "[func] : (rev ? '<' : '>');
        for (int i = 0; i < MAXDRIVES; i ++) {
            Drive *drive = &udppkt.drives[i];
            if (drive->dtfd >= 0) {
                char *bargraph = &bargraphs[i*66];
                if (filesizes[i] == 0xFFFFFFFFU) {
                    filesizes[i] = drive->filesize;
                    memset (&bargraphs[i*66], '-', 64);
                    int j = filesizes[i] * 64 / BLOCKSPERTAPE / WORDSPERBLOCK / 2;
                    bargraph[j]  = '|';
                    bargraph[64] = ']';
                }
                uint16_t blocknumber  = drive->tapepos / 4;

                bool gofwd = go && ! rev && (i == driveno);
                bool gorev = go &&   rev && (i == driveno);
                char rorw  = drive->rdonly ? '-' : '+';
                printf (ESC_EREOL "\n  %o: %c%s", i, rorw, drive->fname);
                printf (ESC_EREOL "\n");
                printf (" [%s" ESC_EREOL "\n", bargraph);
                printf ("%*s%c%c^%04o%c%c%c" ESC_EREOL "\n",
                    drive->tapepos * 16 / BLOCKSPERTAPE, "",
                    (gorev ? rwfc : ' '), (gorev ? '<' : ' '),
                    blocknumber, 'a' + drive->tapepos % 4,
                    (gofwd ? '>' : ' '), (gofwd ? rwfc : ' '));
            } else {
                filesizes[i] = 0xFFFFFFFFU;
            }
        }

        printf (ESC_EREOL "\n    0000a = beg-of-tape"
                ESC_EREOL "\n    ____b = between leading block number and data"
                ESC_EREOL "\n    ____c = between data and trailing block number"
                ESC_EREOL "\n    2701d = end-of-tape" ESC_EREOL "\n");

        // little twirly to show we are cycling
        printf (ESC_EREOL "\n  %c" ESC_EREOP "\r", "-\\|/"[++twirly&3]);
        fflush (stdout);

        if (pthread_sigmask (SIG_UNBLOCK, &sigintmask, NULL) != 0) ABORT ();
    }

    return 0;
}



// pass state of tapes to whoever asks via udp
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

        LOCKIT;
        uint32_t status = tcat[1];
        udppkt.status_a = (status & TC_STATA) / TC_STATA0;
        udppkt.status_b = (status & TC_STATB) / TC_STATB0;
        memcpy (udppkt.drives, drives, sizeof udppkt.drives);
        UNLKIT;

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
