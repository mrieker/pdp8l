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

// Tests the pdp8l.v PDP-8/L implementation by sending random instructions and data and verifying the results

//  ./z8lrand.armv7l

#include <fcntl.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "disassemble.h"
#include "z8ldefs.h"

#define ABORT() do { fprintf (stderr, "abort() %s:%d\n", __FILE__, __LINE__); abort (); } while (0)
#define ASSERT(cond) do { if (__builtin_constant_p (cond)) { if (!(cond)) asm volatile ("assert failure line %c0" :: "i"(__LINE__)); } else { if (!(cond)) ABORT (); } } while (0)

static char const *const majstatenames[8] =
    { "START", "FETCH", "DEFER", "EXEC", "WC", "CA", "BRK", "DEPOS" };
static char const *const timestatenames[32] =
    { "IDLE", "TS1BODY", "TP1BEG", "TP1END", "TS2BODY", "TP2BEG", "TP2END", "TS3BODY",
        "TP3BEG", "TP3END", "BEGIOP1", "DOIOP1", "BEGIOP2", "DOIOP2", "BEGIOP4", "DOIOP4",
        "TS4BODY", "TP4BEG", "TP4END", "???19", "???20", "???21", "???22", "???23",
        "???24", "???25", "???26", "???27", "???28", "???29", "???30", "???31" };

static bool linc;
static uint16_t acum;
static uint16_t pctr;
static uint32_t volatile *zynqpage;

static void doldad (uint16_t addr, uint16_t data);
static void dostart ();
static void docont ();
static void memorycycle (uint32_t state, uint16_t addr, uint16_t rdata, uint16_t wdata);
static void debounce ();
static void clockit ();
static bool g2skip (uint16_t opcode);
static uint16_t randbits (int nbits);
static void verify12 (int index, uint32_t mask, uint16_t expect, char const *msg);
static void verifybit (int index, uint32_t mask, bool expect, char const *msg);
static void fatalerr (char const *fmt, ...);
static void dumpstate ();

int main (int argc, char **argv)
{
    setlinebuf (stdout);

    int zynqfd = open ("/proc/zynqgpio", O_RDWR);
    if (zynqfd < 0) {
        fprintf (stderr, "z8lrand: error opening /proc/zynqgpio: %m\n");
        ABORT ();
    }

    void *zynqptr = mmap (NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, zynqfd, 0);
    if (zynqptr == MAP_FAILED) {
        fprintf (stderr, "z8lrand: error mmapping /proc/zynqgpio: %m\n");
        ABORT ();
    }

    zynqpage = (uint32_t volatile *) zynqptr;
    uint32_t ver = zynqpage[Z_VER];
    printf ("version %08X\n", ver);
    if ((ver & 0xFFFF0000U) != (('8' << 24) | ('L' << 16))) {
        fprintf (stderr, "z8lrand: bad magic number\n");
        ABORT ();
    }

    // select manual clocking and reset the pdp8l.v processor
    // this should also clear accumulator and link
    zynqpage[Z_RA] = a_nanocycle | a_softreset | a_i_AC_CLEAR | a_i_BRK_RQST | a_i_EMA | a_i_INT_RQST | a_i_IO_SKIP | a_i_MEMDONE | a_i_STROBE;
    zynqpage[Z_RB] = 0;
    zynqpage[Z_RC] = 0;
    zynqpage[Z_RD] = d_i_DMAADDR | d_i_DMADATA;
    zynqpage[Z_RE] = 0;
    zynqpage[Z_RF] = 0;
    zynqpage[Z_RG] = 0;
    zynqpage[Z_RH] = 0;
    zynqpage[Z_RI] = 0;
    zynqpage[Z_RJ] = 0;
    zynqpage[Z_RK] = 0;
    usleep (10);

    // release the reset
    zynqpage[Z_RA] &= ~ a_softreset;

    // set program counter to zero and give it some random bits for the contents of that memory location
    doldad (0, randbits (12));

    // flick the start switch to clear accumulator, link and start it running
    dostart ();

    // run random instructions, verifying the cycles
    long long unsigned cycleno = 0;
    while (true) {

        // step to MS_START state so we know AC,LINK have been updated
        for (int i = 0; (zynqpage[Z_RK] & k_majstate) != (MS_START * k_majstate0); i ++) {
            if (i > 1000) fatalerr ("timed out clocking to MS_START\n");
            clockit ();
        }

        // the accumulator and link should be up to date
        verify12  (Z_RI, i_lbAC,   acum, "AC mismatch at beginning of memory cycle");
        verifybit (Z_RG, g_lbLINK, linc, "LINK mismatch at beginning of memory cycle");

        // set random switch register contents
        uint16_t randsr = randbits (12);
        zynqpage[Z_RE]  = (zynqpage[Z_RE] & ~ e_swSR) | (randsr * e_swSR0);

        // send random opcode
        uint16_t opcode;
        do opcode = randbits (12);
        while (((opcode & 07007) == 06000) || ((opcode & 07401) == 07401));
        ////while (((opcode & 07000) == 06000) || ((opcode & 07401) == 07401));
        ////if ((opcode & 07000) < 06000) opcode &= 07377;  //????temp no defer????//
        printf ("%12llu  L.AC=%o.%04o PC=%04o : OP=%04o  %s", ++ cycleno, linc, acum, pctr, opcode, disassemble (opcode, pctr).c_str ());
        memorycycle (g_lbFET, pctr, opcode, opcode);
        uint16_t effaddr = ((opcode & 00200) ? (pctr & 07600) : 0) | (opcode & 00177);
        pctr = (pctr + 1) & 07777;

        // maybe do a defer cycle
        if (((opcode & 07000) < 06000) && (opcode & 0400)) {
            uint16_t pointer = randbits (12);
            uint16_t autoinc = (pointer + ((effaddr & 07770) == 00010)) & 07777;
            printf (" / %04o", autoinc);
            memorycycle (g_lbDEF, effaddr, pointer, autoinc);
            effaddr = autoinc;
        }

        // do the exec cycle
        switch (opcode >> 9) {

            // and, tad, isz, dca, jms
            case 0: case 1: case 2: case 3: case 4: {
                uint16_t operand = randbits (12);
                printf (" / %04o", operand);
                switch (opcode >> 9) {
                    case 0: {
                        memorycycle (g_lbEXE, effaddr, operand, operand);
                        acum &= operand;
                        printf (" => %04o\n", acum);
                        break;
                    }
                    case 1: {
                        memorycycle (g_lbEXE, effaddr, operand, operand);
                        uint16_t sum = acum + (linc ? 010000 : 0) + operand;
                        acum = sum & 07777;
                        linc = (sum >> 12) & 1;
                        printf (" => %o.%04o\n", linc, acum);
                        break;
                    }
                    case 2: {
                        uint16_t incd = (operand + 1) & 07777;
                        printf (" => %04o\n", incd);
                        memorycycle (g_lbEXE, effaddr, operand, incd);
                        if (incd == 0) pctr = (pctr + 1) & 07777;
                        break;
                    }
                    case 3: {
                        printf (" <= %04o\n", acum);
                        memorycycle (g_lbEXE, effaddr, operand, acum);
                        acum = 0;
                        break;
                    }
                    case 4: {
                        printf (" <= %04o\n", pctr);
                        memorycycle (g_lbEXE, effaddr, operand, pctr);
                        pctr = (effaddr + 1) & 07777;
                        break;
                    }
                    default: ABORT ();
                }
                break;
            }

            // jmp
            case 5: {
                printf ("\n");
                pctr = effaddr;
                break;
            }

            // iot
            case 6: {
                ASSERT ((f_oBIOP1 == 1) && (f_oBIOP2 == 2) && (f_oBIOP4 == 4));
                for (uint16_t m = 1; m <= 4; m += m) {
                    if (opcode & m) {

                        printf (" / IOP%u <= %04o", m, acum);

                        // wait for processor to assert IOPm
                        for (int i = 0; ! (zynqpage[Z_RF] & m); i ++) {
                            if (i > 1000) fatalerr ("timed out waiting for IOP%u asserted\n", m);
                            clockit ();
                        }

                        // it should be sending the accumulator out
                        verify12 (Z_RH, h_oBAC, acum, "AC invalid going out during IOP");

                        // get random bits for acclear, ioskip, acbits
                        bool acclear    = randbits  (1);
                        bool ioskip     = randbits  (1);
                        uint16_t acbits = randbits (12);
                        printf (" => %c%c%04o", (acclear ? 'C' : ' '), (ioskip ? 'S' : ' '), acbits);

                        // send those random bits to the processor
                        zynqpage[Z_RA] = (zynqpage[Z_RA] & ~ a_i_AC_CLEAR & ~ a_i_IO_SKIP) | (acclear ? 0 : a_i_AC_CLEAR) | (ioskip ? 0 : a_i_IO_SKIP);
                        zynqpage[Z_RC] = (zynqpage[Z_RC] & ~ c_iINPUTBUS) | (acbits * c_iINPUTBUS0);

                        // wait for processor to drop IOPm
                        for (int i = 0; zynqpage[Z_RF] & m; i ++) {
                            if (i > 1000) fatalerr ("timed out waiting for IOP%u negated\n", m);
                            clockit ();
                        }

                        // clear the random bits so it won't clock them again during nulled-out io pulses
                        zynqpage[Z_RA] |= a_i_AC_CLEAR | a_i_IO_SKIP;
                        zynqpage[Z_RC] &= ~ c_iINPUTBUS;

                        // update our shadow registers
                        if (acclear) acum = 0;
                        if (ioskip)  pctr = (pctr + 1) & 07777;
                        acum |= acbits;
                    }
                }
                printf ("\n");
                break;
            }

            // opr
            case 7: {
                verify12 (Z_RH, h_oBAC, acum, "AC bad at beginning of OPR instruction");
                if (opcode & 0400) {
                    if (g2skip (opcode)) {
                        printf (" SKIP");
                        pctr = (pctr + 1) & 07777;
                    }
                    if (opcode & 0200) acum  = 0;
                    if (opcode & 0004) acum |= randsr;
                    printf (" => %04o\n", acum);
                    if (opcode & 0002) {
                        for (int i = 0; ! (zynqpage[Z_RF] & f_o_B_RUN); i ++) {
                            if (i > 1000) fatalerr ("timed out waiting for RUN negated after HLT instruction\n");
                            clockit ();
                        }
                        docont ();
                    }
                } else {
                    if (opcode & 00200) acum  = 0;
                    if (opcode & 00100) linc  = false;
                    if (opcode & 00040) acum ^= 07777;
                    if (opcode & 00020) linc ^= true;
                    if (opcode & 00001) {
                        acum = (acum + 1) & 07777;
                        if (acum == 0) linc ^= true;
                    }
                    switch ((opcode >> 1) & 7) {
                        case 1: {                           // BSW
                            acum = ((acum & 077) << 6) | (acum >> 6);
                            break;
                        }
                        case 2: {                           // RAL
                            acum = (acum << 1) | (linc ? 1 : 0);
                            linc = (acum & 010000) != 0;
                            acum &= 07777;
                            break;
                        }
                        case 3: {                           // RTL
                            acum = (acum << 2) | (linc ? 2 : 0) | (acum >> 11);
                            linc = (acum & 010000) != 0;
                            acum &= 07777;
                            break;
                        }
                        case 4: {                           // RAR
                            uint16_t oldac = acum;
                            acum = (acum >> 1) | (linc ? 04000 : 0);
                            linc = (oldac & 1) != 0;
                            break;
                        }
                        case 5: {                           // RTR
                            acum = (acum >> 2) | (linc ? 02000 : 0) | ((acum & 3) << 11);
                            linc = (acum & 010000) != 0;
                            acum &= 07777;
                            break;
                        }
                    }
                    printf (" => %o.%04o\n", linc, acum);
                }
            }
        }
    }

    return 0;
}

// do a 'load address' operation, verifying the memory access
//  input:
//   addr = address to load
//   data = data to send processor for the corresponding read
static void doldad (uint16_t addr, uint16_t data)
{
    // press load address switch
    zynqpage[Z_RB] |= b_swLDAD;

    // clock some cycles to arm the debounce circuit
    debounce ();

    // release load address switch
    zynqpage[Z_RB] &= ~ b_swLDAD;

    // it should now do a memory cycle, reading and writing that same data
    memorycycle (0, addr, data, data);
}

// do a 'start' operation
static void dostart ()
{
    // press start switch
    zynqpage[Z_RB] |= b_swSTART;

    // clock some cycles to arm the debounce circuit
    debounce ();

    // release start switch
    zynqpage[Z_RB] &= ~ b_swSTART;

    // clock until the run flipflop sets
    for (int i = 0; zynqpage[Z_RF] & f_o_B_RUN; i ++) {
        if (i > 1000) fatalerr ("timed out waiting for RUN after ficking START switch\n");
        clockit ();
    }

    verify12 (Z_RI, i_lbAC, 0, "AC not zero after START switch");
    if (zynqpage[Z_RG] & g_lbLINK) fatalerr ("LINK not zero after START switch\n");
}

// do a 'continue' operation
static void docont ()
{
    // press continue switch
    zynqpage[Z_RB] |= b_swCONT;

    // clock some cycles to arm the debounce circuit
    debounce ();

    // release continue switch
    zynqpage[Z_RB] &= ~ b_swCONT;

    // clock until the run flipflop sets
    for (int i = 0; zynqpage[Z_RB] & f_o_B_RUN; i ++) {
        if (i > 1000) fatalerr ("timed out waiting for RUN after ficking CONT switch\n");
        clockit ();
    }
}

// clock through a memory cycle, verifying the address and data
//  input:
//   state = g_lb<state>
//   addr  = address it should be accessing
//   rdata = data to send it for the read
//   wdata = data it should send for writing
//   processor could possibly be somewhere near end of previous memory cycle
//  output:
//   returns with processor just exited TS3
static void memorycycle (uint32_t state, uint16_t addr, uint16_t rdata, uint16_t wdata)
{
    // clock until we see TS1
    // give it extra time cuz it could be at end of TS3 for previous instruction
    for (int i = 0; ! (zynqpage[Z_RF] & f_oBTS_1); i ++) {
        if (i > 3000) fatalerr ("timed out waiting for TS1 asserted\n");
        clockit ();
    }

    // we should have MEMSTART now
    if (! (zynqpage[Z_RF] & f_oMEMSTART)) fatalerr ("MEMSTART missing at beginning of TS1\n");

    // it should be outputting address we expect
    verify12 (Z_RI, i_oMA, addr, "MA bad at beginning of mem cycle");

    // should now be fully transitioned to the major state
    if ((state != 0) && ! (zynqpage[Z_RG] & state)) fatalerr ("not in state %08X during mem cycle\n", state);

    // send the read data and strobe to indicate data is valid
    zynqpage[Z_RC]  = (zynqpage[Z_RC] & ~ c_iMEM) | (rdata * c_iMEM0);
    zynqpage[Z_RA] &= ~ a_i_STROBE;

    // end of TS1 means it got the data into MB
    for (int i = 0; zynqpage[Z_RF] & f_oBTS_1; i ++) {
        if (i > 1000) fatalerr ("timed out waiting for TS1 negated\n");
        clockit ();
    }

    verify12 (Z_RH, h_oBMB, rdata, "MB during read");
    zynqpage[Z_RA] |= a_i_STROBE;

    // wait for it to enter TS3, it should be sending the write data
    for (int i = 0; ! (zynqpage[Z_RF] & f_oBTS_3); i ++) {
        if (i > 1000) fatalerr ("timed out waiting fot TS3 asserted\n");
        clockit ();
    }

    verify12 (Z_RH, h_oBMB, wdata, "MB during writeback");

    // tell it we wrote the value to memory by asserting memdone
    zynqpage[Z_RA] &= ~ a_i_MEMDONE;

    // wait for it to leave TS3
    for (int i = 0; zynqpage[Z_RF] & f_oBTS_3; i ++) {
        if (i > 1000) fatalerr ("timed out waiting for TS3 negated\n");
        clockit ();
    }

    // drop memdone now that we know processor has seen it
    zynqpage[Z_RA] |= a_i_MEMDONE;
}

static void debounce ()
{
    for (int i = 0; i < 1000; i ++) {
        clockit ();
    }
}

// gate through one fpga clock cycle
static void clockit ()
{
    usleep (10);
    zynqpage[Z_RA] |=   a_nanostep;
    usleep (10);
    zynqpage[Z_RA] &= ~ a_nanostep;
    usleep (10);
}

// compute if a Group 2 instruction would skip
static bool g2skip (uint16_t opcode)
{
    bool skip = false;
    if ((opcode & 0100) && (acum & 04000)) skip = true;     // SMA
    if ((opcode & 0040) && (acum ==    0)) skip = true;     // SZA
    if ((opcode & 0020) &&           linc) skip = true;     // SNL
    if  (opcode & 0010)                    skip = ! skip;   // reverse
    return skip;
}

// generate a random number
static uint16_t randbits (int nbits)
{
    static uint64_t seed = 0x123456789ABCDEF0ULL;

    uint16_t randval = 0;

    while (-- nbits >= 0) {

        // https://www.xilinx.com/support/documentation/application_notes/xapp052.pdf
        uint64_t xnor = ~ ((seed >> 63) ^ (seed >> 62) ^ (seed >> 60) ^ (seed >> 59));
        seed = (seed << 1) | (xnor & 1);

        randval += randval + (seed & 1);
    }

    return randval;
}

static void verify12 (int index, uint32_t mask, uint16_t expect, char const *msg)
{
    uint16_t actual = (zynqpage[index] & mask) / (mask & - mask);
    if (actual != expect) {
        fatalerr ("%s, is %04o should be %04o\n", msg, actual, expect);
    }
}

static void verifybit (int index, uint32_t mask, bool expect, char const *msg)
{
    uint16_t actual = (zynqpage[index] & mask) / (mask & - mask);
    if (actual != expect) {
        fatalerr ("%s, is %o should be %o\n", msg, actual, expect);
    }
}

static void fatalerr (char const *fmt, ...)
{
    printf ("\n");
    va_list ap;
    va_start (ap, fmt);
    vprintf (fmt, ap);
    va_end (ap);
    dumpstate ();
    ABORT ();
}

static void dumpstate ()
{
    uint32_t majstate  = (zynqpage[Z_RK] & k_majstate)  / k_majstate0;
    uint32_t timestate = (zynqpage[Z_RK] & k_timestate) / k_timestate0;
    uint32_t timedelay = (zynqpage[Z_RK] & k_timedelay) / k_timedelay0;
    printf ("majstate=%s timestate=%s timedelay=%u\n",
        majstatenames[majstate], timestatenames[timestate], timedelay);
}