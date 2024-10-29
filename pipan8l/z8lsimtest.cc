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

// Tests the pdp8lsim.v PDP-8/L implementation by sending random instructions and data and verifying the results

//  ./z8lsimtest

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
#include "z8lutil.h"

#define FIELD(index,mask) ((pdpat[index] & mask) / (mask & - mask))

#define XM_ADDR   (077777U << 0)
#define XM_WRITE  (1U << 15)
#define XM_DATA   (07777U << 16)
#define XM_BUSY   (1U << 28)
#define XM_ENLO4K (1U << 30)
#define XM_ENABLE (1U << 31)
#define XM_ADDR0  (1U << 0)
#define XM_DATA0  (1U << 16)

static char const *const majstatenames[8] =
    { "START", "FETCH", "DEFER", "EXEC", "WC", "CA", "BRK", "DEPOS" };
static char const *const timestatenames[32] =
    { "IDLE", "TS1BODY", "TP1BEG", "TP1END", "TS2BODY", "TP2BEG", "TP2END", "TS3BODY",
        "TP3BEG", "TP3END", "BEGIOP1", "DOIOP1", "BEGIOP2", "DOIOP2", "BEGIOP4", "DOIOP4",
        "TS4BODY", "TP4BEG", "TP4END", "???19", "???20", "???21", "???22", "???23",
        "???24", "???25", "???26", "???27", "???28", "???29", "???30", "???31" };

static bool intdelayed;
static bool intenabled;
static bool intrequest;
static bool linc;
static int nseqs;
static uint16_t acum;
static uint16_t pctr;
static uint16_t *seqs;
static uint32_t clockno;
static uint32_t zrawrite, zrcwrite, zrdwrite;
static uint32_t volatile *pdpat;
static uint32_t volatile *xmemat;

static void doldad (uint16_t addr, uint16_t data);
static void dostart ();
static void docont ();
static void memorycycle (uint32_t state, uint16_t addr, uint16_t rdata, uint16_t wdata);
static void debounce ();
static void clockit ();
static bool g2skip (uint16_t opcode);
static uint16_t xrandbits (int nbits);
static void verify12 (int index, uint32_t mask, uint16_t expect, char const *msg);
static void verifybit (int index, uint32_t mask, bool expect, char const *msg);
static void fatalerr (char const *fmt, ...);
static void dumpstate ();

int main (int argc, char **argv)
{
    setlinebuf (stdout);

    // if numbers given on command line, use them instead of randoms
    // exit when the randoms are used up
    nseqs = -1;
    if (argc > 1) {
        char *p;
        nseqs = argc - 1;
        seqs = (uint16_t *) malloc (nseqs * sizeof *seqs);
        for (int i = 0; ++ i < argc;) {
            seqs[nseqs-i] = strtoul (argv[i], &p, 0);
            if (*p != 0) {
                fprintf (stderr, "z8lsimtest: bad number %s\n", argv[i]);
                return -1;
            }
        }
    }

    // access the zynq io page
    // hopefully it has our pdp8l.v code indicated by magic number in first word
    Z8LPage z8p;
    pdpat = z8p.findev ("8L", NULL, NULL, true);
    if (pdpat == NULL) {
        fprintf (stderr, "z8lsimtest: bad magic number\n");
        ABORT ();
    }
    printf ("version %08X\n", pdpat[Z_VER]);
    xmemat = z8p.findev ("XM", NULL, NULL, true);
    if (xmemat == NULL) {
        fprintf (stderr, "z8lsimtest: can't find xmem device\n");
        ABORT ();
    }
    printf ("version %08X\n", xmemat[0]);

    // select simulator with manual clocking and reset the pdp8lsim.v processor
    pdpat[Z_RA] = zrawrite = a_nanocycle | a_softreset | a_simit | a_i_AC_CLEAR | a_i_BRK_RQST | a_i_EA | a_i_EMA | a_i_INT_INHIBIT | a_i_INT_RQST | a_i_IO_SKIP | a_i_MEMDONE | a_i_STROBE;
    pdpat[Z_RB] = 0;
    pdpat[Z_RC] = zrcwrite = 0;
    pdpat[Z_RD] = zrdwrite = d_i_DMAADDR | d_i_DMADATA;
    pdpat[Z_RE] = 0;
    pdpat[Z_RF] = 0;
    pdpat[Z_RG] = 0;
    pdpat[Z_RH] = 0;
    pdpat[Z_RI] = 0;
    pdpat[Z_RJ] = 0;
    pdpat[Z_RK] = 0;

    // make low 4K memory accesses go to the external memory block by leaving _EA asserted all the time
    // ...so we can directly access its contents, feeding in random numbers as needed
    xmemat[1] = XM_ENLO4K;

    // clock the synchronous reset through
    for (int i = 0; i < 5; i ++) clockit ();

    // release the reset
    pdpat[Z_RA] = zrawrite &= ~ a_softreset;

    // set program counter to zero
    doldad (0, 0);

    // flick the start switch to clear accumulator, link and start it running
    dostart ();

    // run random instructions, verifying the cycles
    uint32_t instrno = 0;
    while (true) {

        // once through this loop per instruction

        printf ("%10u  L.AC=%o.%04o PC=%04o : ", ++ instrno, linc, acum, pctr);

        uint32_t startclockno = clockno;

        uint16_t effaddr = 0;
        uint16_t opcode  = 0;
        uint16_t randsr  = 0;

        // clock to TS_IDLE so we know it is about to check for breaks and interrupt requests
        for (int i = 0; FIELD (Z_RK, k_timestate) != TS_IDLE; i ++) {
            if (i > 1000) fatalerr ("timed out waiting for TS_IDLE before fetch\n");
            clockit ();
        }

        // maybe do a dma cycle
        if ((FIELD (Z_RK, k_majstate) == MS_START) && (xrandbits (8) == 0)) {

            // have both TS_IDLE and MS_START, raise BRK_RQST and next clock should be in it
            bool threecycle = xrandbits (1);
            uint16_t dmaaddr = xrandbits (12);
            printf ("DMA %d-cycle", (threecycle ? 3 : 1));
            pdpat[Z_RA] = zrawrite = (zrawrite & ~ a_i_BRK_RQST & ~ a_iTHREECYCLE) | (threecycle ? a_iTHREECYCLE : 0);
            pdpat[Z_RD] = zrdwrite = (zrdwrite | d_i_DMAADDR) & ~ (dmaaddr * d_i_DMAADDR0);
            clockit ();
            if (threecycle) {

                // WC state reads wordcount from dmaaddr, increments it and writes it back
                uint16_t oldwordcount = xrandbits (12);
                uint16_t newwordcount = (oldwordcount + 1) & 07777;
                printf ("  WC:%04o / %04o <= %04o", dmaaddr, oldwordcount, newwordcount);
                memorycycle (g_lbWC, dmaaddr, oldwordcount, newwordcount);
                verifybit (Z_RF, f_oBWC_OVERFLOW, newwordcount == 0, "BWC OVERFLOW at end of WC cycle");

                // CA state reads pointer from dmaaddr + 1, increments it and writes it back
                // then we use the incremented pointer for the transfer address
                dmaaddr = (dmaaddr + 1) & 07777;
                bool cainc = xrandbits (1);
                uint16_t oldcuraddr = xrandbits (12);
                uint16_t newcuraddr = (oldcuraddr + cainc) & 07777;
                printf ("  CA:%04o / %04o <= %04o", dmaaddr, oldwordcount, newwordcount);
                pdpat[Z_RA] = zrawrite = (zrawrite & ~ a_iCA_INCREMENT) | (cainc ? a_iCA_INCREMENT : 0);
                memorycycle (g_lbCA, dmaaddr, oldcuraddr, newcuraddr);
                dmaaddr = newcuraddr;
            }

            // transfer word at address dmaaddr
            bool dmadatain    = xrandbits (1);      // true: writing memory from d_i_DMADATA; false: writing MB as is back to memory
            bool dmameminc    = xrandbits (1);      // true: increment value read; false: leave value as read
            uint16_t dmardata = xrandbits (12);     // fake value we read from from memory location dmaaddr
                                                    // - processor will put this in its MB register at end of TS1
            uint16_t dmawrand = xrandbits (12);     // random value to send out over dmadata bus
                                                    // - ignored by processor when dmadatain = false
                                                    //   written to MB by processor at end of TS2 when dmadatain = true
            pdpat[Z_RA] = zrawrite = (zrawrite & ~ a_iDATA_IN & ~ a_iMEMINCR) | (dmadatain ? a_iDATA_IN : 0) | (dmameminc ? a_iMEMINCR : 0);
            pdpat[Z_RD] = zrdwrite = (zrdwrite | d_i_DMADATA) & ~ (dmawrand * d_i_DMADATA0);
            uint16_t dmawdata = ((dmadatain ? dmawrand : dmardata) + dmameminc) & 07777;  // what processor should send us to write back to memory
            printf ("  BRK:%04o / %04o <= %04o", dmaaddr, dmardata, dmawdata);
            memorycycle (g_lbBRK, dmaaddr, dmardata, dmawdata);
            pdpat[Z_RA] = zrawrite |= a_i_BRK_RQST;
        } else {

            // maybe interrupt request is next
            if (intrequest && intenabled && FIELD (Z_RA, a_i_INT_INHIBIT) && (FIELD (Z_RK, k_majstate) == MS_START)) {
                for (int i = 0; FIELD (Z_RF, f_o_LOAD_SF); i ++) {
                    if (i > 10) fatalerr ("timed out waiting for LOAD_SF interrupt acknowledge\n");
                    clockit ();
                }
                printf ("interrupt acknowledge");

                intrequest = false; //TODO: drop request random number of cycles later
                pdpat[Z_RA] = zrawrite |= a_i_INT_RQST;

                // disable interrupts then execute a JMS 0 instruction
                intdelayed = false;
                intenabled = false;
                opcode     = 04000;
            } else {

                // delayed ION active now that we are about to do a fetch
                intenabled = intdelayed;

                // send random opcode
                do opcode = xrandbits (12);  // HLT with interrupt barfs
                while (intrequest && intenabled && (opcode & 07402) == 07402);
                printf ("OP=%04o  %s", opcode, disassemble (opcode, pctr).c_str ());

                // set random switch register contents if about to do an OSR instruction
                if (((opcode & 07401) == 07400) && (opcode & 0004)) {
                    randsr = xrandbits (12);
                    pdpat[Z_RE] = (pdpat[Z_RE] & ~ e_swSR) | (randsr * e_swSR0);
                }

                // perform fetch memory cycle
                memorycycle (g_lbFET, pctr, opcode, opcode);
                effaddr = ((opcode & 00200) ? (pctr & 07600) : 0) | (opcode & 00177);
                pctr = (pctr + 1) & 07777;

                // maybe do a defer cycle
                if (((opcode & 07000) < 06000) && (opcode & 0400)) {
                    uint16_t pointer = xrandbits (12);
                    uint16_t autoinc = (pointer + ((effaddr & 07770) == 00010)) & 07777;
                    printf (" / %04o", autoinc);
                    memorycycle (g_lbDEF, effaddr, pointer, autoinc);
                    effaddr = autoinc;
                }
            }

            // do the exec cycle
            switch (opcode >> 9) {

                // and, tad, isz, dca, jms
                case 0: case 1: case 2: case 3: case 4: {
                    uint16_t operand = xrandbits (12);
                    printf (" / %04o", operand);
                    switch (opcode >> 9) {
                        case 0: {
                            memorycycle (g_lbEXE, effaddr, operand, operand);
                            acum &= operand;
                            printf (" => %04o", acum);
                            break;
                        }
                        case 1: {
                            memorycycle (g_lbEXE, effaddr, operand, operand);
                            uint16_t sum = acum + (linc ? 010000 : 0) + operand;
                            acum = sum & 07777;
                            linc = (sum >> 12) & 1;
                            printf (" => %o.%04o", linc, acum);
                            break;
                        }
                        case 2: {
                            uint16_t incd = (operand + 1) & 07777;
                            printf (" => %04o", incd);
                            memorycycle (g_lbEXE, effaddr, operand, incd);
                            if (incd == 0) pctr = (pctr + 1) & 07777;
                            break;
                        }
                        case 3: {
                            printf (" <= %04o", acum);
                            memorycycle (g_lbEXE, effaddr, operand, acum);
                            acum = 0;
                            break;
                        }
                        case 4: {
                            printf (" <= %04o", pctr);
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
                            for (int i = 0; ! (pdpat[Z_RF] & m); i ++) {
                                if (i > 1000) fatalerr ("timed out waiting for IOP%u asserted\n", m);
                                clockit ();
                            }

                            // it should be sending the accumulator out
                            verify12 (Z_RH, h_oBAC, acum, "AC invalid going out during IOP");

                            // get random bits for acclear, ioskip, acbits
                            bool acclear    = xrandbits  (1);
                            bool ioskip     = xrandbits  (1);
                            uint16_t acbits = xrandbits (12);
                            printf (" => %c%c%04o", (acclear ? 'C' : ' '), (ioskip ? 'S' : ' '), acbits);

                            // send those random bits to the processor
                            pdpat[Z_RA] = zrawrite = (zrawrite & ~ a_i_AC_CLEAR & ~ a_i_IO_SKIP) | (acclear ? 0 : a_i_AC_CLEAR) | (ioskip ? 0 : a_i_IO_SKIP);
                            pdpat[Z_RC] = zrcwrite = (zrcwrite & ~ c_iINPUTBUS) | (acbits * c_iINPUTBUS0);

                            // wait for processor to drop IOPm
                            for (int i = 0; pdpat[Z_RF] & m; i ++) {
                                if (i > 1000) fatalerr ("timed out waiting for IOP%u negated\n", m);
                                clockit ();
                            }

                            // clear the random bits so it won't clock them again during nulled-out io pulses
                            pdpat[Z_RA] = zrawrite |= a_i_AC_CLEAR | a_i_IO_SKIP;
                            pdpat[Z_RC] = zrcwrite &= ~ c_iINPUTBUS;

                            // update our shadow registers
                            if (acclear) acum = 0;
                            if (ioskip)  pctr = (pctr + 1) & 07777;
                            acum |= acbits;
                        }
                    }
                    if (opcode == 06001) {
                        if (! intdelayed) printf ("  <<intdelayed=1 intrequest=%d>>  ", intrequest);
                        intdelayed = true;
                    }
                    if (opcode == 06002) {
                        if (  intdelayed) printf ("  <<intdelayed=0 intrequest=%d>>  ", intrequest);
                        intdelayed = false;
                        intenabled = false;
                    }
                    break;
                }

                // opr
                case 7: {
                    verify12 (Z_RH, h_oBAC, acum, "AC bad at beginning of OPR instruction");
                    if (! (opcode & 0400)) {
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
                        printf (" => %o.%04o", linc, acum);
                    } else if (! (opcode & 0001)) {
                        if (g2skip (opcode)) {
                            printf (" SKIP");
                            pctr = (pctr + 1) & 07777;
                        }
                        if (opcode & 0200) acum  = 0;
                        if (opcode & 0004) acum |= randsr;
                        printf (" => %04o", acum);
                        if (opcode & 0002) {
                            for (int i = 0; ! FIELD (Z_RF, f_o_B_RUN); i ++) {
                                if (i > 1000) fatalerr ("timed out waiting for RUN negated after HLT instruction\n");
                                clockit ();
                            }
                            printf ("  cont");
                            docont ();
                        }
                    } else {
                        printf (" ignored");
                    }
                }
            }
        }

        // step to MS_START state so we know AC,LINK have been updated
        for (int i = 0; FIELD (Z_RK, k_majstate) != MS_START; i ++) {
            if (i > 1000) fatalerr ("timed out clocking to MS_START\n");
            clockit ();
        }

        // the accumulator and link should be up to date
        verify12  (Z_RI, i_lbAC,   acum, "AC mismatch at end of cycle");
        verifybit (Z_RG, g_lbLINK, linc, "LINK mismatch at end of cycle");

        // print how many cycles it took (each clock is 10nS)
        uint32_t clocks = clockno - startclockno;
        printf ("  (%u.%02u uS)\n", clocks / 100, clocks % 100);
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
    pdpat[Z_RB] |= b_swLDAD;

    // clock some cycles to arm the debounce circuit
    debounce ();

    // release load address switch
    pdpat[Z_RB] &= ~ b_swLDAD;

    // it should now do a memory cycle, reading and writing that same data
    memorycycle (0, addr, data, data);
}

// do a 'start' operation
static void dostart ()
{
    // press start switch
    pdpat[Z_RB] |= b_swSTART;

    // clock some cycles to arm the debounce circuit
    debounce ();

    // release start switch
    pdpat[Z_RB] &= ~ b_swSTART;

    // clock until the run flipflop sets
    for (int i = 0; FIELD (Z_RF, f_o_B_RUN); i ++) {
        if (i > 1000) fatalerr ("timed out waiting for RUN after flicking START switch\n");
        clockit ();
    }

    verify12 (Z_RI, i_lbAC, 0, "AC not zero after START switch");
    if (FIELD (Z_RG, g_lbLINK)) fatalerr ("LINK not zero after START switch\n");
}

// do a 'continue' operation
static void docont ()
{
    // press continue switch
    pdpat[Z_RB] |= b_swCONT;

    // clock some cycles to arm the debounce circuit
    debounce ();

    // release continue switch
    pdpat[Z_RB] &= ~ b_swCONT;

    // clock until the run flipflop sets
    for (int i = 0; FIELD (Z_RB, f_o_B_RUN); i ++) {
        if (i > 1000) fatalerr ("timed out waiting for RUN after flicking CONT switch\n");
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
//
// steps through:
//   TS1 = reading memory location
//   TS2 = computing any modification to value
//   TS3 = writing memory location
// after that:
//   IOP1,2,4 = only if this was a fetch of an io instruction
//   TS4 = any post-processing for the cycle
static void memorycycle (uint32_t state, uint16_t addr, uint16_t rdata, uint16_t wdata)
{
    // write the read data to the given address so cpu can read it
    // we have to use pdp8lxmem.v's low 4K because we can access it directly from the arm processor
    // the pip8lsim.v's 4K memory can only be accessed via front panel switches and lights
    // ...(analagous to real PDP-8/L's core memory, slow and processor would have to be halted)
    xmemat[1] = XM_ENLO4K | (rdata * XM_DATA0) | XM_WRITE | (addr * XM_ADDR0);
    for (int i = 0; xmemat[1] & XM_BUSY; i ++) {
        if (i > 1000) fatalerr ("timed out writing memory\n");
        clockit ();
    }

    // clock until we see TS1
    // give it extra time cuz it could be at end of TS3 for previous instruction
    for (int i = 0; ! FIELD (Z_RF, f_oBTS_1); i ++) {
        if (i > 3000) fatalerr ("timed out waiting for TS1 asserted\n");
        clockit ();
    }

    // we should have MEMSTART now
    if (! FIELD (Z_RF, f_oMEMSTART)) fatalerr ("MEMSTART missing at beginning of TS1\n");

    // cpu should be outputting address we expect
    verify12 (Z_RI, i_oMA, addr, "MA bad at beginning of mem cycle");

    // should now be fully transitioned to the major state
    if ((state != 0) && ! FIELD (Z_RG, state)) fatalerr ("not in state %08X during mem cycle\n", state);

    // end of TP2 means it finished reading that memory location into MB
    for (int i = 0; FIELD (Z_RF, f_oBTS_1) | FIELD (Z_RF, f_oBTP2); i ++) {
        if (i > 1000) fatalerr ("timed out waiting for TS1 and TP2 negated\n");
        clockit ();
    }

    // MB should now have the value we wrote to the memory location
    verify12 (Z_RH, h_oBMB, rdata, "MB during read");

    // wait for it to enter TS3, meanwhile cpu (pdp8lsim.v) should possibly be modifying
    // the value and sending the value (modified or not) back to the memory
    for (int i = 0; ! FIELD (Z_RF, f_oBTS_3); i ++) {
        if (i > 1000) fatalerr ("timed out waiting fot TS3 asserted\n");
        clockit ();
    }

    // beginning of TS3, MB should now have the possibly modified value
    verify12 (Z_RH, h_oBMB, wdata, "MB during writeback");

    // wait for it to leave TS3, where pdp8lxmem.v writes the value to memory
    for (int i = 0; FIELD (Z_RF, f_oBTS_3); i ++) {
        if (i > 1000) fatalerr ("timed out waiting for TS3 negated\n");
        clockit ();
    }

    // verify the memory contents
    xmemat[1] = XM_ENLO4K | (addr * XM_ADDR0);
    for (int i = 0; xmemat[1] & XM_BUSY; i ++) {
        if (i > 1000) fatalerr ("timed out reading memory\n");
        clockit ();
    }
    uint16_t vdata = (xmemat[1] & XM_DATA) / XM_DATA0;
    if (vdata != wdata) {
        printf ("address %04o contained %04o at end of cycle, should be %04o\n", addr, vdata, wdata);
        fatalerr ("memory validation error\n");
    }

    // maybe start requesting an interrupt
    // don't do it if we are about to execute an HLT instruction or it will puque
    if (! intrequest && ((state != g_lbFET) || ((rdata & 07402) != 07402))) {
        intrequest = xrandbits (10) == 0;
        if (intrequest) {
            printf ("  <<intrequest=1 intenabled=%d>>  ", intenabled);
            pdpat[Z_RA] = zrawrite &= ~ a_i_INT_RQST;
        }
    }
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
    pdpat[Z_RA] = zrawrite | a_nanostep;
    pdpat[Z_RA] = zrawrite;
    clockno ++;
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
static uint16_t xrandbits (int nbits)
{
    if (nseqs > 0) {
        uint16_t randval = seqs[--nseqs];
        if (randval >= 1U << nbits) {
            fprintf (stderr, "xrandbits: value 0%o too big for %d bits\n", randval, nbits);
            ABORT ();
        }
        return randval;
    }
    if (nseqs == 0) {
        printf ("\nxrandbits: end of numbers on command line\n");
        exit (0);
    }

    return randbits (nbits);
}

static void verify12 (int index, uint32_t mask, uint16_t expect, char const *msg)
{
    uint16_t actual = FIELD (index, mask);
    if (actual != expect) {
        fatalerr ("%s, is %04o should be %04o\n", msg, actual, expect);
    }
}

static void verifybit (int index, uint32_t mask, bool expect, char const *msg)
{
    uint16_t actual = FIELD (index, mask);
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
    uint32_t majstate  = FIELD (Z_RK, k_majstate);
    uint32_t timestate = FIELD (Z_RK, k_timestate);
    uint32_t timedelay = FIELD (Z_RK, k_timedelay);
    uint32_t ir = FIELD (Z_RG, g_lbIR);
    printf ("clockno=%u ir=%o majstate=%s timestate=%s timedelay=%u\n",
        clockno, ir, majstatenames[majstate], timestatenames[timestate], timedelay);
}
