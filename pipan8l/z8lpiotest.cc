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

// Test PIO functionality using mostly the pdp8ltty.v module.
// Uses TTY 40 so the real TTY 03 won't interfere.
// Writes program to PDP memory that reads keyboard, adds a number, then echoes to printer.
// Interrupts are tested by incrementing the number added by 1 for each interrupt.

//  ./z8lpiotest [-man] [-sim]
//    -man = use manual clocking (only good for simulator mode)
//    -sim = use simulator (pdp8lsim.v) instead of real PDP-8/L

#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "assemble.h"
#include "z8ldefs.h"
#include "z8lutil.h"

#define DOTJMPDOT 05252U

static bool manclock;
static bool simulate;
static bool traceon;
static bool volatile exitflag;
static uint32_t volatile *cmemat;
static uint32_t volatile *extmem;
static uint32_t volatile *pdpat;
static uint32_t volatile *tt40at;
static uint32_t volatile *xmemat;

static bool findtt40 (void *param, uint32_t volatile *ttyat);
static void siginthand (int signum);
static void writeasm (uint16_t xaddr, char const *opstr);
static void writemem (uint16_t xaddr, uint16_t wdata);
static uint16_t readmem (uint16_t xaddr);
static void waitcmemidle ();
static uint16_t waitcmemread ();
static void clockit (int n);



int main (int argc, char **argv)
{
    setlinebuf (stdout);

    for (int i = 0; ++ i < argc;) {
        if (strcasecmp (argv[i], "-man") == 0) {
            manclock = true;
            continue;
        }
        if (strcasecmp (argv[i], "-sim") == 0) {
            simulate = true;
            continue;
        }
        fprintf (stderr, "unknown argument %s\n", argv[i]);
        return 1;
    }

    Z8LPage z8p;
    pdpat  = z8p.findev ("8L", NULL, NULL, true);
    cmemat = z8p.findev ("CM", NULL, NULL, true);
    xmemat = z8p.findev ("XM", NULL, NULL, true);
    extmem = z8p.extmem ();
    tt40at = z8p.findev ("TT", findtt40, NULL, true);

    printf ("8L VERSION=%08X\n", pdpat[0]);
    printf ("CM VERSION=%08X\n", cmemat[0]);
    printf ("TT VERSION=%08X\n", tt40at[0]);
    printf ("XM VERSION=%08X\n", xmemat[0]);

    // select simulator and reset it or select real pdp and leave sim reset
    pdpat[Z_RA] = ZZ_RA;
    pdpat[Z_RB] = 0;
    pdpat[Z_RC] = ZZ_RC;
    pdpat[Z_RD] = ZZ_RD;
    pdpat[Z_RE] = (simulate ? e_simit | e_softreset : 0) | (manclock ? 0 : e_nanocontin);
    xmemat[1]   = 0;
    clockit (1000);
    pdpat[Z_RE] = (simulate ? e_simit : 0) | (manclock ? 0 : e_nanocontin);   // e_simit off leaves sim in reset
    clockit (1000);

    // a real PDP needs to be running so we can do DMA
    // tell user to put a JMP . at 5252 and start it
    if (simulate) {
        pdpat[Z_RB] = DOTJMPDOT * b_swSR0;
        clockit (1000);
        pdpat[Z_RB] = DOTJMPDOT * b_swSR0 | b_swLDAD;
        clockit (1000);
        pdpat[Z_RB] = DOTJMPDOT * b_swSR0;
        clockit (1000);
        pdpat[Z_RB] = DOTJMPDOT * b_swSR0 | b_swDEP;
        clockit (1000);
        pdpat[Z_RB] = DOTJMPDOT * b_swSR0;
        clockit (1000);
        pdpat[Z_RB] = DOTJMPDOT * b_swSR0 | b_swLDAD;
        clockit (1000);
        pdpat[Z_RB] = DOTJMPDOT * b_swSR0;
        clockit (1000);
        pdpat[Z_RB] = DOTJMPDOT * b_swSR0 | b_swSTART;
        clockit (1000);
        pdpat[Z_RB] = DOTJMPDOT * b_swSR0;
        clockit (1000);
    } else {
        printf ("\n");
        printf ("  set dfld,ifld,mem prot,step to 0\n");
        printf ("  set sr to %04o\n", DOTJMPDOT);
        printf ("  load address\n");
        printf ("  deposit\n");
        printf ("  load address\n");
        printf ("  start\n");
        printf ("> ");
        fflush (stdout);
        char temp[8];
        if (fgets (temp, sizeof temp, stdin) == NULL) {
            printf ("\n");
            return 0;
        }
    }

    if (pdpat[Z_RF] & f_o_B_RUN) {
        fprintf (stderr, "processor not running\n");
        ABORT ();
    }

    uint16_t verdjd = readmem (DOTJMPDOT);
    if (verdjd != DOTJMPDOT) {
        fprintf (stderr, "DOTJMPDOT %04o contains %04o\n", DOTJMPDOT, verdjd);
        ABORT ();
    }

    tt40at[Z_TTYKB] = KB_ENAB;      // clear TTY 40 controller
    tt40at[Z_TTYPR] = 0;

    uint16_t counter = 1;           // initialize counter value

    // write test program to memory using dma
    writeasm (0001, "isz  00004");  // increment counter
    writeasm (0002, "jmpi 00000");  // return from interrupt
    writeasm (0003, "jmp  00001");  // counter zero, inc again
    writemem (0004, counter);

    writeasm (0020, "cla");         //   cla
    writeasm (0021, "iot  06405");  //   kie - clear int enabled
                                    // loop,
    writeasm (0022, "iot  06401");  //   ksf - skip if kb ready (test IOP1, IO_SKIP, ~IO_SKIP)
    writeasm (0023, "jmp  00022");  //   jmp .-1
    writeasm (0024, "iot  06002");  //   iof
    writeasm (0025, "iot  06406");  //   krb - clear accum, read kb and clear flag (test AC_CLEAR, INPUTBUS[11:00])
    writeasm (0026, "tad  00004");  //   tad counter - sum is always non-zero
    writeasm (0027, "iot  06414");  //   tpc - start printing, do not clear ac (test IOP4, ~AC_CLEAR, BAC[11:00])
    writeasm (0030, "sna");         //   sna - skip if accum non-zero
    writeasm (0031, "hlt");         //   hlt - tpc cleared accum
    writeasm (0032, "iot  06411");  //   tsf - skip if print done
    writeasm (0033, "jmp  00032");  //   jmp .-1
    writeasm (0034, "iot  06001");  //   ion
    writeasm (0035, "iot  06412");  //   tcf - clear printer flag (test IOP2)
    writeasm (0036, "iot  06411");  //   tsf - skip if flag still set
    writeasm (0037, "jmp  00022");  //   jmp loop - leave ac nz to make sure krb clears it
    writeasm (0040, "hlt");         //   hlt - printer flag didn't clear

    // overwrite the DOTJMPDOT to start the test program going
    traceon = manclock;
    writeasm (DOTJMPDOT, "jmp 020");

    signal (SIGINT, siginthand);
    uint32_t pass = 0;
    while (! exitflag) {
        if (++ pass % 4096 == 0) { putchar ('*'); fflush (stdout); }

        // pdp is waiting for a character typed on keyboard
        // give it a random number, but make sure sum is non-zero so sna/hlt will work
        // pdp8ltty.v has a 12 bit register so give it full 12 bits
        uint16_t kbchar, summed;
        do {
            kbchar = randbits (12);
            summed = (kbchar + counter) & 07777;
        } while (summed == 0);
        ////printf ("main*: sending %04o to keyboard\n", kbchar);
        tt40at[Z_TTYKB] = KB_ENAB | KB_FLAG | kbchar;

        // now wait for pdp to echo the character plus counter
        ////printf ("main*: waiting for echo to printer\n");
        uint32_t prreg;
        for (int j = 0; ! ((prreg = tt40at[Z_TTYPR]) & PR_FULL); j ++) {
            if (j > 10000) {
                fprintf (stderr, "timed out waiting for PR_FULL\n");
                ABORT ();
            }
            clockit (1);
        }
        ////printf ("main*: got %04o from printer\n", prreg & 07777);

        // make sure it echoed the correct value
        uint16_t prchar = prreg & 07777;
        if (prchar != summed) {
            fprintf (stderr, "sent %04o, counter %04o, received %04o, should be %04o\n", kbchar, counter, prchar, summed);
            ABORT ();
        }

        // sometimes we request an interrupt which causes the pdp's counter to increment
        // otherwise, make sure interrupt request is clear from last time
        bool incounter = (randbits (4) == 0);
        if (incounter) {
            pdpat[Z_RA] = ZZ_RA & ~ a_i_INT_RQST;
            counter = (counter + 1) & 07777;
            if (counter == 0) counter = 1;
            ////printf ("main*: incremented counter to %04o\n", counter);
        } else {
            pdpat[Z_RA] = ZZ_RA;
        }

        // pdp is waiting for us to set the PR_FLAG
        tt40at[Z_TTYPR] = PR_FLAG;
    }
    printf ("\n");
    return 0;
}

static bool findtt40 (void *param, uint32_t volatile *ttyat)
{
    if (ttyat == NULL) {
        fprintf (stderr, "cannot find TT port 40\n");
        ABORT ();
    }
    return (ttyat[Z_TTYPN] & 077) == 040;
}

static void siginthand (int signum)
{
    exitflag = true;
}



#define OPMAX 64
static char const *opstrs[OPMAX];

// write assembly language instruction to memory
static void writeasm (uint16_t xaddr, char const *opstr)
{
    uint16_t opcode;
    char *error = assemble (opstr, xaddr, &opcode);
    if (error != NULL) {
        fprintf (stderr, "error assembling <%s>: %s\n", opstr, error);
        ABORT ();
    }
    if (xaddr < OPMAX) opstrs[xaddr] = opstr;
    writemem (xaddr, opcode);
}

// write data to memory via DMA cycle
//  input:
//   xaddr = 15-bit address to write to
//   wdata = 12-bit data to write to that location
//  output:
//   value written to memory
static void writemem (uint16_t xaddr, uint16_t wdata)
{
    waitcmemidle ();
    cmemat[2] = 0;
    cmemat[1] = CM_ENAB | wdata * CM_DATA0 | CM_WRITE | xaddr * CM_ADDR0;
}

// read data from memory via DMA cycle
//  input:
//   xaddr = 15-bit address to write to
//  output:
//   returns value read from memory
static uint16_t readmem (uint16_t xaddr)
{
    waitcmemidle ();
    cmemat[2] = 0;
    cmemat[1] = CM_ENAB | xaddr * CM_ADDR0;
    return waitcmemread ();
}

// wait for pdp8lcmem.v interface able to accept new command
static void waitcmemidle ()
{
    for (int j = 0; cmemat[1] & CM_BUSY; j ++) {
        clockit (1);
        if (j > 10000) {
            fprintf (stderr, "timed out waiting for cmem ready\n");
            ABORT ();
        }
    }
}

// wait for pdp8lcmem.v read data ready
static uint16_t waitcmemread ()
{
    uint32_t cm1;
    for (int j = 0; ! ((cm1 = cmemat[1]) & CM_DONE); j ++) {
        clockit (1);
        if (j > 10000) {
            fprintf (stderr, "timed out waiting for cmem ready\n");
            ABORT ();
        }
    }
    return (cm1 & CM_DATA) / CM_DATA0;
}

static char const *const majstatenames[] = { MS_NAMES };
static char const *const timestatenames[] = { TS_NAMES };
static uint32_t traceseq;

// gate through at least n fpga clock cycles
static void clockit (int n)
{
    if (manclock) {
        while (-- n >= 0) pdpat[Z_RE] |= e_nanotrigger;
        if (traceon) {
            uint32_t ms = (pdpat[Z_RK] & k_majstate)  / k_majstate0;
            uint32_t ts = (pdpat[Z_RK] & k_timestate) / k_timestate0;
            uint32_t ma = (pdpat[Z_RI] & i_oMA)  / i_oMA0;
            uint32_t mb = (pdpat[Z_RH] & h_oBMB) / h_oBMB0;
            uint32_t ac = (pdpat[Z_RH] & h_oBAC) / h_oBAC0;
            char const *opstr = ((ms == MS_FETCH) && (ts == TS_TSIDLE) && (ma < OPMAX)) ? opstrs[ma] : NULL;
            if (opstr == NULL) opstr = "";
            printf ("%10u  %-5s  %-7s  MA=%04o  MB=%04o  AC=%04o  %s\n",
                ++ traceseq, majstatenames[ms], timestatenames[ts], ma, mb, ac, opstr);
        }
    } else {
        usleep (n / 100 + 1);
    }
}
