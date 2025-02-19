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

// simulate being the PDP-8/L to test pipan8l.cc
// does not require raspi or zynq (can run on pc)
// invoke with -sim option on pipan8l command line

// it simulates a PDP-8/L with 8K memory and a teletype
// to access the teletype:
//  write keyboard characters to pipe /tmp/pipan8l_ttykb
//  read printer characters from pipe /tmp/pipan8l_ttypr

// envars:
//  pipan8l_memfields = memory fields 1..8, default 2
//  pipan8l_ttycps = teletype chars per sec 1..1000000, default 10

#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <tcl.h>
#include <unistd.h>

#include "assemble.h"
#include "disassemble.h"
#include "padlib.h"
#include "pindefs.h"
#include "readprompt.h"

#define ABORT() do { fprintf (stderr, "abort() %s:%d\n", __FILE__, __LINE__); abort (); } while (0)
#define ASSERT(cond) do { if (__builtin_constant_p (cond)) { if (!(cond)) asm volatile ("assert failure line %c0" :: "i"(__LINE__)); } else { if (!(cond)) ABORT (); } } while (0)

// which of the pins are outputs (switches)
static uint16_t const wmsks[P_NU16S] = { P0_WMSK, P1_WMSK, P2_WMSK, P3_WMSK, P4_WMSK };

static uint8_t const acpins[12] = { P_AC00, P_AC01, P_AC02, P_AC03, P_AC04, P_AC05, P_AC06, P_AC07, P_AC08, P_AC09, P_AC10, P_AC11 };
static uint8_t const mapins[12] = { P_MA00, P_MA01, P_MA02, P_MA03, P_MA04, P_MA05, P_MA06, P_MA07, P_MA08, P_MA09, P_MA10, P_MA11 };
static uint8_t const mbpins[12] = { P_MB00, P_MB01, P_MB02, P_MB03, P_MB04, P_MB05, P_MB06, P_MB07, P_MB08, P_MB09, P_MB10, P_MB11 };
static uint8_t const srpins[12] = { P_SR00, P_SR01, P_SR02, P_SR03, P_SR04, P_SR05, P_SR06, P_SR07, P_SR08, P_SR09, P_SR10, P_SR11 };
static uint8_t const irpins[ 3] = { P_IR00, P_IR01, P_IR02 };



SimLib::SimLib ()
{
    dfldsw = false;
    eareg  = false;
    ifldsw = false;
    ionreg = false;
    lnreg  = false;
    mprtsw = false;
    stepsw = false;
    state  = NUL;
    acreg  = 0;
    irtop  = 0;
    mareg  = 0;
    mbreg  = 0;
    pcreg  = 0;
    swreg  = 0;
    runreg = false;
    runtid = 0;
    memset (memarray, 0, sizeof memarray);
    memset (wrpads, 0, sizeof wrpads);
    traceon = false;

    kbflag    = false;
    prflag    = false;
    prfull    = false;
    ttinten   = true;
    ttintrq   = false;
    kbreadfd  = -1;
    prwritefd = -1;
    kbchar    = 0;
    prchar    = 0;
    kbnextus  = 0;
    prnextus  = 0;

    intinhibiteduntiljump = false;
    dfld          = 0;
    ifld          = 0;
    ifldafterjump = 0;
    saveddfld     = 0;
    savedifld     = 0;
    memfields     = 0;
}

void SimLib::openpads ()
{
    // set up number of memory fields
    memfields = 2;
    char const *mfenv = getenv ("pipan8l_memfields");
    if ((mfenv != NULL) && (mfenv[0] != 0)) {
        char *p;
        memfields = strtoul (mfenv, &p, 0);
        if ((*p != 0) || (memfields < 1) || (memfields > 8)) {
            fprintf (stderr, "SimLib::openpads: num mem fields %s must be in range 1..8\n", mfenv);
            ABORT ();
        }
    }
    fprintf (stderr, "SimLib::openpads: memory size %uK words\n", memfields * 4);

    // maybe turn tracing on
    char const *trenv = getenv ("pipan8l_simtrace");
    traceon = (trenv != NULL) && (trenv[0] & 1);

    // create named pipes for tty's keyboard and printer
    // tcl script can read printer output from pipan8l_ttypr
    // and can send keyboard input to pipan8l_ttykb to debug test scripts
    unlink ("/tmp/pipan8l_ttykb");
    unlink ("/tmp/pipan8l_ttypr");
    if (mkfifo ("/tmp/pipan8l_ttykb", 0666) < 0) {
        fprintf (stderr, "SimLib::openpads: error creating /tmp/pipan8l_ttykb fifo: %m\n");
        ABORT ();
    }
    if (mkfifo ("/tmp/pipan8l_ttypr", 0666) < 0) {
        fprintf (stderr, "SimLib::openpads: error creating /tmp/pipan8l_ttypr fifo: %m\n");
        ABORT ();
    }

    // open them in non-blocking mode on this end
    // use non-blocking mode so we don't get stuck in the open() call
    kbreadfd = open ("/tmp/pipan8l_ttykb", O_RDONLY | O_NONBLOCK);
    if (kbreadfd < 0) {
        fprintf (stderr, "SimLib::openpads: error opening /tmp/pipan8l_ttykb fifo: %m\n");
        ABORT ();
    }
    pthread_t tid;
    if (pthread_create (&tid, NULL, openttyprpipe, this) < 0) ABORT ();

    // set up simulated tty speed
    char const *env = getenv ("pipan8l_ttycps");
    if ((env != NULL) && (env[0] != 0)) {
        int cps = atoi (env);
        if (cps < 1) cps = 1;
        if (cps > 1000000) cps = 1000000;
        usperch = 1000000 / cps;
    } else {
        usperch = 100000;
    }
}

// O_NONBLOCK doesn't seem to work with O_WRONLY so do in blocking style then modify
void *SimLib::openttyprpipe (void *zhis)
{
    pthread_detach (pthread_self ());

    // O_NONBLOCK gets non-existing device error
    // so wait here until reader connects
    int fd = open ("/tmp/pipan8l_ttypr", O_WRONLY);
    if (fd < 0) {
        fprintf (stderr, "SimLib::openpads: error opening /tmp/pipan8l_ttypr fifo: %m\n");
        ABORT ();
    }

    // now set to non-blocking mode
    if (fcntl (fd, F_SETFL, O_NONBLOCK) < 0) ABORT ();

    fprintf (stderr, "SimLib::openpads: tty connected\n");

    // ok to let main program use it now
    ((SimLib *)zhis)->prwritefd = fd;

    return NULL;
}

// simulate reading the paddle pins
void SimLib::readpads (uint16_t *pads)
{
    // return latest written values for output pins
    for (int pad = 0; pad < P_NU16S; pad ++) {
        pads[pad] = wrpads[pad] & wmsks[pad];
    }

    // spread register bits among the paddle pins
    spreadreg (acreg, pads, 12, acpins);
    spreadreg (mareg, pads, 12, mapins);
    spreadreg (mbreg, pads, 12, mbpins);
    spreadreg (swreg, pads, 12, srpins);
    spreadreg (irtop, pads,  3, irpins);

    // return possible state pin
    uint8_t stpin;
    switch (state) {
        case NUL: stpin = 0377; break;
        case FET: stpin = P_FET; break;
        case EXE: stpin = P_EXE; break;
        case DEF: stpin = P_DEF; break;
        case WCT: stpin = P_WCT; break;
        case CAD: stpin = P_CAD; break;
        case BRK: stpin = P_BRK; break;
        default: ABORT ();
    }
    if (stpin != 0377) spreadpin (1, pads, stpin);

    // return other single-bit pins
    spreadpin (ionreg, pads, P_ION);
    spreadpin (runreg, pads, P_RUN);
    spreadpin (eareg,  pads, P_EMA);
    spreadpin (lnreg,  pads, P_LINK);
}

// simulate writing the paddle pins
void SimLib::writepads (uint16_t const *pads)
{
    // update permanent switches
    swreg  = gatherreg (pads, 12, srpins);
    stepsw = gatherpin (pads, P_STEP);
    ifldsw = gatherpin (pads, P_IFLD);
    dfldsw = gatherpin (pads, P_DFLD);
    mprtsw = gatherpin (pads, P_MPRT);

    // momentary switches
    if (! runreg) {

        // deposit switch
        if (gatherpin (wrpads, P_DEP) && ! gatherpin (pads, P_DEP)) {
            writemem (dfld, pcreg, swreg);  // dep uses DFLD - ref MC8/L install Mar70 p 6 checkout procedure diagnostic tests
            pcreg = (pcreg + 1) & 07777;
            state = NUL;
        }

        // examine switch
        if (gatherpin (wrpads, P_EXAM) && ! gatherpin (pads, P_EXAM)) {
            mbreg = readmem (dfld, pcreg);  // exam uses DFLD - ref MC8/L install Mar70 p 6 checkout procedure diagnostic tests
            pcreg = (mareg + 1) & 07777;
            state = NUL;
        }

        // loadaddress switch
        if (gatherpin (wrpads, P_LDAD) && ! gatherpin (pads, P_LDAD)) {
            dfld  = dfldsw;
            ifld  = ifldafterjump = ifldsw;
            pcreg = swreg;
            mbreg = readmem (ifld, pcreg);
            state = NUL;
        }

        // continue switch
        if (gatherpin (wrpads, P_CONT) && ! gatherpin (pads, P_CONT)) {
            contswitch ();
        }

        // start switch
        if (gatherpin (wrpads, P_STRT) && ! gatherpin (pads, P_STRT)) {
            acreg = 0;
            lnreg = false;
            contswitch ();
        }
    }

    // stop switch
    if (gatherpin (wrpads, P_STOP) && ! gatherpin (pads, P_STOP)) {
        stopswitch ();
    }

    // step switch on is also a stop
    if (stepsw) {
        stopswitch ();
    }

    // save all written pins
    memcpy (wrpads, pads, sizeof wrpads);
}



// spread register value among its bits in the paddles
//  input:
//   reg = register value to spread
//   pads = paddle words
//   npins = number of pins to spread
//   pins = array of pin numbers to write
//  output:
//   *pads = filled in
void SimLib::spreadreg (uint16_t reg, uint16_t *pads, int npins, uint8_t const *pins)
{
    do spreadpin ((reg >> -- npins) & 1, pads, *(pins ++));
    while (npins > 0);
}

// spread single bit to its bit in the paddles
//  input:
//   val = bit to write to paddles
//   pads = paddle words
//   pin = pin number to write
//  output:
//   *pads = filled in
void SimLib::spreadpin (bool val, uint16_t *pads, uint8_t pin)
{
    int index = pin >> 4;
    int bitno = pin & 017;
    if (val) pads[index] |= 1U << bitno;
     else pads[index] &= ~ (1U << bitno);
}

// gather register value from its bits in the paddles
//  input:
//   pads = paddle words
//   npins = number of pins to gather
//   pins = array of pin numbers to read
//  output:
//   returns register value
uint16_t SimLib::gatherreg (uint16_t const *pads, int npins, uint8_t const *pins)
{
    uint16_t reg = 0;
    do reg += reg + gatherpin (pads, *(pins ++));
    while (-- npins > 0);
    return reg;
}

// gather single bit from its bit in the paddles
//  input:
//   pads = paddle words
//   pin = pin number to read
//  output:
//   returns register value
bool SimLib::gatherpin (uint16_t const *pads, uint8_t pin)
{
    int index = pin >> 4;
    int bitno = pin & 017;
    return (pads[index] >> bitno) & 1;
}



// if STEP switch, step through to end of next state
//   else, run steps until HLT or stopswitch() called
void SimLib::contswitch ()
{
    ASSERT (! runreg);
    if (stepsw) {
        singlestep ();
    } else {
        stopswitch ();
        runreg = true;
        int rc = pthread_create (&runtid, NULL, runthreadwrap, this);
        if (rc != 0) ABORT ();
    }
}

// if runthread running, tell it to stop then wait for it to stop
void SimLib::stopswitch ()
{
    runreg = false;
    if (runtid != 0) {
        pthread_join (runtid, NULL);
        runtid = 0;
    }
}

// run instructions until runreg is cleared,
//  either by an HLT instruction or STEP or STOP switch
void *SimLib::runthreadwrap (void *zhis)
{
    ((SimLib *) zhis)->runthread ();
    return NULL;
}

void SimLib::runthread ()
{
    do singlestep ();
    while (runreg);
}

// step through to end of next state
void SimLib::singlestep ()
{
    ////printf ("singlestep*: beg PC=%04o IR=%o ST=%s\n", pcreg, irtop, ststr ());

    switch (state) {

        // continue or start switch just pressed after doing ldad/dep/exam, step to end of fetch cycle
        case NUL: {
            dofetch ();
            break;
        }

        // at end of FET state, step to end of next cycle
        case FET: {

            // mareg = address instr was fetched from
            // mbreg = full 12-bit instruction
            // irtop = top 3 bits of instruction
            // pcreg = incremented

            // if instruction is direct JMP, IOT or OPR, it has already been executed, so do another fetch
            if (((mbreg & 07400) == 05000) || (irtop >= 6)) {
                dofetch ();
                break;
            }

            // memory reference, set up memory address
            mareg = ((mbreg & 00200) ? (mareg & 07600) : 0) | (mbreg & 00177);

            // if indirect memory reference, do defer cycle
            // and if it's a JMP, do the jump at end of defer cycle
            if (mbreg & 00400) {
                state = DEF;
                mbreg = readmem (ifld, mareg);
                if ((mareg & 07770) == 00010) {
                    writemem (ifld, mareg, (mbreg + 1) & 07777);
                }
                if (irtop == 5) {
                    intinhibiteduntiljump = false;
                    ifld  = ifldafterjump;
                    pcreg = mbreg;
                }
                break;
            }

            // direct memory reference, do the exec cycle
            eareg = ifld;
            domemref ();
            break;
        }

        // at end of DEF state, step on to end of exec cycle
        case DEF: {

            // if indirect JMP, the PC was already updated, so do a fetch
            if (irtop == 5) {
                dofetch ();
                break;
            }

            // not a JMP, execute the instruction
            eareg = dfld;   // gets overridden to ifld for JMS
            mareg = mbreg;
            domemref ();
            break;
        }

        // at end of EXE state, step on to end of next fetch cycle
        case EXE: {
            dofetch ();
            break;
        }

        default: ABORT ();
    }
}

// at end of EXE state for previous instruction,
// get us to the end of FET state for next instruction
void SimLib::dofetch ()
{
    polltty ();
    if (ionreg && ttintrq && ! intinhibiteduntiljump) {
        if (traceon) printf ("SimLib::dofetch:  PC=%o.%04o  L.AC=%o.%04o  IF=%o  DF=%o  interrupt\n",
                ifld, pcreg, lnreg, acreg, ifld, dfld);

        saveddfld = dfld;
        savedifld = ifld;
        eareg  = dfld = ifld = ifldafterjump = 0;
        idelay = false;
        ionreg = false;
        state  = EXE;
        mareg  = 0;
        writemem (0, 0, pcreg);
        pcreg  = 1;
        return;
    }
    ionreg = idelay;

    state = FET;
    mbreg = readmem (ifld, pcreg);
    irtop = mbreg >> 9;
    pcreg = (pcreg + 1) & 07777;

    if ((irtop & 6) == 4) intinhibiteduntiljump = false;

    if (traceon) printf ("SimLib::dofetch:  PC=%o.%04o  L.AC=%o.%04o  IF=%o  DF=%o  IR=%04o  %s\n",
            eareg, mareg, lnreg, acreg, ifld, dfld, mbreg, disassemble (mbreg, mareg).c_str ());

    // we can do direct JMP, IOT and OPR as part of the fetch
    if ((mbreg & 07400) == 05000) {
        intinhibiteduntiljump = false;
        ifld  = ifldafterjump;
        pcreg = ((mbreg & 00200) ? (mareg & 07600) : 0) | (mbreg & 00177);
    } else if (irtop == 6) {
        doioinst ();
    } else if (irtop == 7) {
        dooperate ();
    }
}

// at end of FET or DEF state, perform memory reference instruction
// gets us to the end of EXE state
// should not have a JMP instruction here, that was done as part of FET or DEF states
void SimLib::domemref ()
{
    state = EXE;
    switch (irtop) {
        case 0: {
            acreg &= readmem (eareg, mareg);
            break;
        }
        case 1: {
            acreg += readmem (eareg, mareg);
            lnreg ^= acreg >> 12;
            acreg &= 07777;
            break;
        }
        case 2: {
            mbreg = (readmem (eareg, mareg) + 1) & 07777;
            writemem (eareg, mareg, mbreg);
            if (mbreg == 0) pcreg = (pcreg + 1) & 07777;
            break;
        }
        case 3: {
            writemem (eareg, mareg, acreg);
            acreg = 0;
            break;
        }
        case 4: {
            intinhibiteduntiljump = false;
            ifld  = ifldafterjump;
            writemem (ifld, mareg, pcreg);
            pcreg = (mareg + 1) & 07777;
            break;
        }
        default: ABORT ();
    }
}

// read memory location
// set eareg, mareg, mbreg
uint16_t SimLib::readmem (uint16_t field, uint16_t addr)
{
    eareg = field;
    mareg = addr;
    mbreg = 0;
    if (field < memfields) {
        uint16_t xaddr = (eareg << 12) | mareg;
        mbreg = memarray[xaddr];
    }
    return mbreg;
}

// write memory location
// set eareg, mareg, mbreg
void SimLib::writemem (uint16_t field, uint16_t addr, uint16_t data)
{
    eareg = field;
    mareg = addr;
    mbreg = data;
    if (field < memfields) {
        uint16_t xaddr = (eareg << 12) | mareg;
        memarray[xaddr] = mbreg;
    }
}

// end of fetch with operate instruciton, do the operation as part of fetch cycle
void SimLib::dooperate ()
{
    if (! (mbreg & 00400)) {

        if (mbreg & 00200) acreg  = 0;
        if (mbreg & 00100) lnreg  = false;
        if (mbreg & 00040) acreg ^= 07777;
        if (mbreg & 00020) lnreg ^= true;
        if (mbreg & 00001) {
            lnreg ^= (++ acreg) >> 12;
            acreg &= 07777;
        }

        switch ((mbreg >> 1) & 7) {
            case 0: break;
            case 1: {                           // BSW
                acreg = ((acreg & 077) << 6) | (acreg >> 6);
                break;
            }
            case 2: {                           // RAL
                acreg = (acreg << 1) | (lnreg ? 1 : 0);
                lnreg = (acreg & 010000) != 0;
                acreg &= 07777;
                break;
            }
            case 3: {                           // RTL
                acreg = (acreg << 2) | (lnreg ? 2 : 0) | (acreg >> 11);
                lnreg = (acreg & 010000) != 0;
                acreg &= 07777;
                break;
            }
            case 4: {                           // RAR
                uint16_t oldac = acreg;
                acreg = (acreg >> 1) | (lnreg ? 04000 : 0);
                lnreg = (oldac & 1) != 0;
                break;
            }
            case 5: {                           // RTR
                acreg = (acreg >> 2) | (lnreg ? 02000 : 0) | ((acreg & 3) << 11);
                lnreg = (acreg & 010000) != 0;
                acreg &= 07777;
                break;
            }
        }
    }

    else if (! (mbreg & 00001)) {
        bool skip = false;
        if ((mbreg & 0100) && (acreg & 04000)) skip = true;     // SMA
        if ((mbreg & 0040) && (acreg ==    0)) skip = true;     // SZA
        if ((mbreg & 0020) &&           lnreg) skip = true;     // SNL
        if  (mbreg & 0010)                     skip = ! skip;   // reverse
        if (skip) pcreg = (pcreg + 1) & 07777;

        if (mbreg & 00200) acreg  = 0;
        if (mbreg & 00004) acreg |= swreg;
        if (mbreg & 00002) runreg = false;
    }

    else {
        fprintf (stderr, "dooperate: EAE %04o at %o.%04o\n", mbreg, eareg, mareg);
    }
}

// end of fetch with I/O instruction, do the I/O as part of the fetch cycle
void SimLib::doioinst ()
{
    switch (mbreg) {

        // interrupt enable/disable
        case 06001: idelay = true; break;
        case 06002: idelay = false; ionreg = false; break;

        // tty access
        case 06031: if (kbflag) pcreg = (pcreg + 1) & 07777; break;
        case 06032: acreg = 0; kbflag = 0; break;
        case 06034: acreg |= kbchar; break;
        case 06035: ttinten = acreg & 1; break;
        case 06036: acreg = kbchar; kbflag = 0; break;
        case 06041: if (prflag) pcreg = (pcreg + 1) & 07777; break;
        case 06042: prflag = 0; break;
        case 06044: prchar = acreg; prfull = 1; break;
        case 06045: if (ttintrq) pcreg = (pcreg + 1) & 07777; break;
        case 06046: prchar = acreg; prflag = 0; prfull = 1; break;

        // extended memory
        case 06201: case 06202: case 06203:
        case 06211: case 06212: case 06213:
        case 06221: case 06222: case 06223:
        case 06231: case 06232: case 06233:
        case 06241: case 06242: case 06243:
        case 06251: case 06252: case 06253:
        case 06261: case 06262: case 06263:
        case 06271: case 06272: case 06273: {
            uint16_t f = (mbreg >> 3) & 7;
            if (mbreg & 1) dfld = f;
            if (mbreg & 2) {
                ifldafterjump = f;
                intinhibiteduntiljump = true;
            }
            break;
        }
        case 06214:
            acreg |= dfld << 3;
            break;

        case 06224:
            acreg |= ifld << 3;
            break;

        case 06234:
            acreg |= saveddfld << 0;
            acreg |= savedifld << 3;
            break;

        case 06244:
            dfld          = saveddfld;
            ifldafterjump = savedifld;
            break;
    }
}

// update tty state
void SimLib::polltty ()
{
    struct timeval nowtv;
    if (gettimeofday (&nowtv, NULL) < 0) ABORT ();
    uint64_t nowus = nowtv.tv_sec * 1000000ULL + nowtv.tv_usec;

    if (prfull && (nowus >= prnextus) && (prwritefd >= 0)) {
        int rc = write (prwritefd, &prchar, 1);
        if (rc > 0) {
            prflag   = 1;
            prfull   = 0;
            prnextus = nowus + usperch;
        } else if ((rc == 0) || ((errno != EAGAIN) && (errno != EWOULDBLOCK))) {
            if (rc == 0) fprintf (stderr, "SimLib::polltty: eof writing to tty pipe\n");
            else fprintf (stderr, "SimLib::polltty: error writing to tty pipe: %m\n");
            ABORT ();
        }
    }

    if (nowus >= kbnextus) {
        int rc = read (kbreadfd, &kbchar, 1);
        if (rc > 0) {
            kbflag   = 1;
            kbnextus = nowus + usperch;
        } else if ((rc < 0) && (errno != EAGAIN) && (errno != EWOULDBLOCK)) {
            fprintf (stderr, "SimLib::polltty: error reading from tty pipe: %m\n");
            ABORT ();
        }
    }

    ttintrq = ttinten & (kbflag | prflag);
}

char const *SimLib::ststr ()
{
    switch (state) {
        case NUL: return "NUL";
        case FET: return "FET";
        case DEF: return "DEF";
        case EXE: return "EXE";
        default: ABORT ();
    }
}
