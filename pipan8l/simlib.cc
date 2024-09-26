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

// simulate being the PDP-8/L to test pipanel.cc

#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
    dfreg  = false;
    eareg  = false;
    ifldsw = false;
    ifreg  = false;
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
}

void SimLib::openpads ()
{ }

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
            mareg = pcreg;
            eareg = dfreg;  // dep uses DFLD - ref MC8/L install Mar70 p 6 checkout procedure diagnostic tests
            uint16_t addr = (((uint16_t) eareg) << 12) | mareg;
            memarray[addr] = mbreg = swreg;
            pcreg = (mareg + 1) & 07777;
            state = NUL;
        }

        // examine switch
        if (gatherpin (wrpads, P_EXAM) && ! gatherpin (pads, P_EXAM)) {
            mareg = pcreg;
            eareg = dfreg;  // exam uses DFLD - ref MC8/L install Mar70 p 6 checkout procedure diagnostic tests
            uint16_t addr = (((uint16_t) eareg) << 12) | mareg;
            mbreg = memarray[addr];
            pcreg = (mareg + 1) & 07777;
            state = NUL;
        }

        // loadaddress switch
        if (gatherpin (wrpads, P_LDAD) && ! gatherpin (pads, P_LDAD)) {
            dfreg = dfldsw;
            ifreg = ifldsw;
            eareg = ifldsw;
            mareg = pcreg = swreg;
            uint16_t addr = (((uint16_t) eareg) << 12) | mareg;
            mbreg = memarray[addr];
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

            // if instruction is direct JMP or OPR, it has already been executed, so do another fetch
            if (((mbreg & 07400) == 05000) || (irtop == 7)) {
                dofetch ();
                break;
            }

            // if memory reference, set up memory address
            if (irtop < 6) {
                mareg = ((mbreg & 00200) ? (mareg & 07600) : 0) | (mbreg & 00177);
            }

            // if indirect memory reference, do defer cycle
            // and if it's a JMP, do the jump at end of defer cycle
            if ((irtop < 6) && (mbreg & 00400)) {
                state = DEF;
                ASSERT (eareg == ifreg);
                uint16_t addr = (((uint16_t) eareg) << 12) | mareg;
                mbreg = memarray[addr];
                if ((addr & 07770) == 00010) {
                    memarray[addr] = mbreg = (mbreg + 1) & 07777;
                }
                if (irtop == 5) {
                    pcreg = mbreg;
                }
                break;
            }

            // if direct memory reference, do the exec cycle
            if (irtop < 6) {
                ASSERT (eareg == ifreg);
                domemref ();
                break;
            }

            // the only thing left is an I/O instruction
            doioinst ();
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
            mareg = mbreg;
            eareg = (irtop < 4) ? dfreg : ifreg;
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

    ////printf ("singlestep*: end PC=%04o IR=%o ST=%s\n", pcreg, irtop, ststr ());
}

// at end of EXE state for previous instruction,
// get us to the end of FET state for next instruction
void SimLib::dofetch ()
{
    state = FET;
    mareg = pcreg;
    eareg = ifreg;
    uint16_t addr = (((uint16_t) eareg) << 12) | mareg;
    mbreg = memarray[addr];
    irtop = mbreg >> 9;
    pcreg = (pcreg + 1) & 07777;

    // we can do direct JMP and OPR as part of the fetch
    if ((mbreg & 07400) == 05000) {
        pcreg = ((mbreg & 00200) ? (mareg & 07600) : 0) | (mbreg & 00177);
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
    uint16_t addr = (((uint16_t) eareg) << 12) | mareg;
    switch (irtop) {
        case 0: {
            acreg &= mbreg = memarray[addr];
            break;
        }
        case 1: {
            acreg += mbreg = memarray[addr];
            lnreg ^= acreg >> 12;
            acreg &= 07777;
            break;
        }
        case 2: {
            mbreg = (memarray[addr] + 1) & 07777;
            memarray[addr] = mbreg;
            if (mbreg == 0) pcreg = (pcreg + 1) & 07777;
            break;
        }
        case 3: {
            memarray[addr] = mbreg = acreg;
            acreg = 0;
            break;
        }
        case 4: {
            memarray[addr] = mbreg = pcreg;
            pcreg = (mareg + 1) & 07777;
            break;
        }
        default: ABORT ();
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
        if (mbreg & 00002) {
            runreg = false;
            pthread_exit (NULL);
        }
    }

    else {
        fprintf (stderr, "dooperate: EAE %04o at %o.%04o\n", mbreg, eareg, mareg);
    }
}

// end of fetch with I/O instruction, do the I/O as part of the next execute cycle
void SimLib::doioinst ()
{
    state = EXE;
    if (mbreg == 06001) ionreg = true;
    if (mbreg == 06002) ionreg = false;
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
