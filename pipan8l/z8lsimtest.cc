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
// Can also similarly test a real PDP-8/L

#include <fcntl.h>
#include <signal.h>
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

#define DOTJMPDOT 05252

#define FIELD(index,mask) ((pdpat[index] & mask) / (mask & - mask))

#define TESTOS8ZAP true

static char const *const majstatenames[] = { MS_NAMES };
static char const *const timestatenames[] = { TS_NAMES };

static bool brkrequest;
static bool volatile ctrlcflag;
static bool dma3cycle;
static bool dmacainc;
static bool dmadatain;
static bool fullreal;
static bool halfreal;
static bool intdelayed;
static bool intenabled;
static bool intrequest;
static bool linc;
static bool memcycwcover;
static bool perclock;
static bool realmode;
static uint16_t acum;
static uint16_t dmaaddr;
static uint16_t dmafield;
static uint16_t dmawdata;
static uint16_t pctr;
static uint32_t clockno;
static uint32_t zrawrite, zrcwrite, zrdwrite, zrewrite;
static uint32_t volatile *extmemptr;
static uint32_t volatile *pdpat;
static uint32_t volatile *cmemat;
static uint32_t volatile *shat;
static uint32_t volatile *xmemat;

static bool xmem_intdisableduntiljump;
static uint8_t xmem_dfld;
static uint8_t xmem_ifld;
static uint8_t xmem_ifldafterjump;
static uint8_t xmem_saveddfld;
static uint8_t xmem_savedifld;

static void sighand (int signum);
static void doldad (uint16_t addr, uint16_t data);
static void dostart ();
static void docont ();
static void memorycycle (uint32_t state, uint8_t field, uint16_t addr, uint16_t rdata, uint16_t wdata);
static void memorycyclx (uint32_t state, uint8_t field, uint16_t addr, uint16_t rdata, uint16_t rdbkdata, uint16_t wtbkdata, uint16_t vfywaddr, uint16_t vfywdata);
static void requestdmaorint (uint32_t state, uint16_t rdata);
static void debounce ();
static void clockit ();
static bool g2skip (uint16_t opcode);
static void verify12 (int index, uint32_t mask, uint16_t expect, char const *msg);
static void verifybit (int index, uint32_t mask, bool expect, char const *msg);
static void printshadow (FILE *out);
static void fatalerr (char const *fmt, ...);
static void dumpstate ();

int main (int argc, char **argv)
{
    setlinebuf (stdout);

    // access the zynq io page
    // hopefully it has our pdp8l.v code indicated by magic number in first word
    Z8LPage z8p;
    pdpat  = z8p.findev ("8L", NULL, NULL, true);
    cmemat = z8p.findev ("CM", NULL, NULL, true);
    xmemat = z8p.findev ("XM", NULL, NULL, true);

    printf ("8L version %08X\n", pdpat[Z_VER]);
    printf ("CM version %08X\n", cmemat[0]);
    printf ("XM version %08X\n", xmemat[0]);

    for (int i = 0; ++ i < argc;) {
        if (strcmp (argv[i], "-?") == 0) {
            puts ("");
            puts ("     Generates random instructions and operands to test sim or PDP");
            puts ("     Also generates random interrupt and DMA requests");
            puts ("");
            puts ("  ./z8lsimtest [-clear | -half | -real]");
            puts ("");
            puts ("    -clear = clear stepping mode from FPGA so PDP will run normally");
            puts ("    -half = use sim but operate in same manner as using real mode");
            puts ("    -real = test real PDP, just tests end-of-cycle memory contents");
            puts ("");
            puts ("    by default, tests sim in detail");
            puts ("");
            return 0;
        }
        if (strcasecmp (argv[1], "-clear") == 0) {

            // do all the stuff as in the hardreset.tcl script
            // ...seems to work

            char temp[20];
            fputs ("turn STEP switch ON, then press enter > ", stderr);
            if (fgets (temp, sizeof temp, stdin) == NULL) return 0;

            pdpat[Z_RA]  = ZZ_RA;
            pdpat[Z_RO]  = o_r_BAC | o_r_BMB | o_r_MA | o_x_DMAADDR | o_x_DMADATA | o_x_INPUTBUS | o_x_MEM;
            pdpat[Z_RE] |=   e_bareit;
            pdpat[Z_RO]  = o_r_BAC | o_r_BMB | o_r_MA | o_x_DMAADDR | o_x_DMADATA | o_x_INPUTBUS;
            pdpat[Z_RA]  = ZZ_RA & ~ a_i_MEMDONE;
            usleep (10);
            pdpat[Z_RA]  = ZZ_RA |   a_i_MEMDONE;
            usleep (10);
            pdpat[Z_RA]  = ZZ_RA & ~ a_i_STROBE;
            usleep (10);
            pdpat[Z_RA]  = ZZ_RA |   a_i_STROBE;
            usleep (10);
            pdpat[Z_RE] &= ~ e_bareit;
            pdpat[Z_RO]  = o_r_BAC | o_r_BMB | o_r_MA | o_x_DMAADDR | o_x_DMADATA | o_x_INPUTBUS | o_x_MEM;
            pdpat[Z_RA]  = ZZ_RA;

            usleep (10);
            pdpat[Z_RE] = e_nanocontin | e_nanotrigger | e_fpgareset;   // no e_simit
            usleep (10);
            pdpat[Z_RA] = ZZ_RA;
            pdpat[Z_RB] = 0;
            pdpat[Z_RC] = ZZ_RC;
            pdpat[Z_RD] = ZZ_RD;
            pdpat[Z_RF] = 0;
            pdpat[Z_RG] = 0;
            pdpat[Z_RH] = 0;
            pdpat[Z_RI] = 0;
            pdpat[Z_RJ] = 0;
            pdpat[Z_RK] = 0;
            xmemat[1]   = XM_ENABLE | XM_ENLO4K;
            usleep (10);
            pdpat[Z_RE] = e_nanocontin | e_nanotrigger;                 // release reset
                                    // omitting e_simit holds sim in power-on reset state
            xmemat[1]   = XM_ENABLE | XM_ENLO4K;

            fputs ("flick STOP switch, turn STEP switch OFF, then press enter > ", stderr);
            if (fgets (temp, sizeof temp, stdin) == NULL) return 0;
            return 0;
        }

        if (strcasecmp (argv[i], "-half") == 0) {
            fullreal = false;
            halfreal = true;
            continue;
        }
        if (strcasecmp (argv[i], "-real") == 0) {
            fullreal = true;
            halfreal = false;
            continue;
        }
        fprintf (stderr, "unknown argument %s\n", argv[i]);
        return 1;
    }

    realmode = (fullreal | halfreal);

    bool cmnobrk = (cmemat[2] & CM2_NOBRK);
    cmemat[1] = 0;  // disable outputs until we request a cycle
    cmemat[2] = 0;

    // get pointer to the 32K-word ram
    // maps each 12-bit word into low 12 bits of 32-bit word
    // upper 20 bits discarded on write, readback as zeroes
    extmemptr = z8p.extmem ();

    // select simulator with manual clocking and reset the pdp8lsim.v processor
    // if using real PDP, disable simulator (it stays in reset state)
    pdpat[Z_RA] = zrawrite = ZZ_RA;
    pdpat[Z_RB] = 0;
    pdpat[Z_RC] = zrcwrite = ZZ_RC;
    pdpat[Z_RD] = zrdwrite = ZZ_RD;
    pdpat[Z_RE] = zrewrite =
         fullreal ? e_fpgareset | e_nanocontin :
        (halfreal ? e_simit | e_fpgareset | e_nanocontin : e_simit | e_fpgareset);
    pdpat[Z_RF] = 0;
    pdpat[Z_RG] = 0;
    pdpat[Z_RH] = 0;
    pdpat[Z_RI] = 0;
    pdpat[Z_RJ] = 0;
    pdpat[Z_RK] = 0;

    // maybe using nobrk mode - does dma directly to extmem array
    // ...otherwise dma done via WC/CA/B processor cycles
    cmemat[2] = cmnobrk ? CM2_NOBRK : 0;

    // clock the synchronous reset through
    for (int i = 0; i < 5; i ++) clockit ();

    // release the reset
    pdpat[Z_RE] = zrewrite &= ~ e_fpgareset;
    for (int i = 0; i < 5; i ++) clockit ();

    // make low 4K memory accesses go to the external memory block by leaving _EA asserted all the time
    // ...so we can directly access its contents via extmemptr, feeding in random numbers as needed
    // TESTOS8ZAP: also tell it to convert ISZ x / JMP .-1 to ISZ x / NOP ; x <= 0
    // make sure XM_MWHOLD is clear from previous run so we can initialize
    xmemat[1] = XM_ENABLE | XM_ENLO4K | (TESTOS8ZAP ? XM_OS8ZAP : 0);
    for (int i = 0; i < 5; i ++) clockit ();

    // if using a real PDP, user must manually halt it (or use pipan8l)
    if (fullreal && FIELD (Z_RF, f_oB_RUN)) {
        fputs ("flick STOP switch to stop processor ", stderr);
        for (int i = 0; FIELD (Z_RF, f_oB_RUN); i ++) {
            if (i == 5) fputs ("\nif fails to stop, try -clear ", stderr);
            sleep (1);
            fputc ('.', stderr);
        }
        fputc ('\n', stderr);
    }

    // reset pdp8lshad.v shadow errors
    shat = z8p.findev ("SH", NULL, NULL, false);
    shat[1] = 0;

    // set up a DOT: JMP DOT instruction to begin with
    extmemptr[DOTJMPDOT] = DOTJMPDOT;
    pctr = DOTJMPDOT;

    // get processor started
    if (fullreal) {

        // set up to hold just before doing TP4 of the JMP instruction
        // pdp8lxmem.v will hold off sending MEMDONE pulse until we set XM_MWSTEP again
        // ...causing the PDP to wait
        // pdp8lxmem.v clears XM_MWSTEP when it has stopped the processor
        xmemat[1] |= XM_MWHOLD | XM_MWSTEP;

        // user must manually start it
        fprintf (stderr, "set SR %04o ; LD ADDR ; START ", DOTJMPDOT);
        while (! FIELD (Z_RF, f_oB_RUN)) {
            xmemat[1] |= XM_MWHOLD | XM_MWSTEP;
            sleep (1);
            fputc ('.', stderr);
        }
        fputc ('\n', stderr);
    } else if (halfreal) {

        // put address in switch register and press load address switch
        pdpat[Z_RB] = DOTJMPDOT * b_swSR0 | b_swLDAD;

        // clock some cycles to arm the debounce circuit
        debounce ();

        // release load address switch, leave address there
        pdpat[Z_RB] = DOTJMPDOT * b_swSR0;

        usleep (100);

        // set up to hold just before doing TP4 of the JMP instruction
        // pdp8lxmem.v will hold off sending MEMDONE pulse until we set XM_MWSTEP again
        // ...causing the sim (pdp8lsim.v) to wait
        xmemat[1] |= XM_MWHOLD | XM_MWSTEP;

        // flick the start switch to clear accumulator, link and start it running
        dostart ();
    } else {

        // set program counter to DOTJMPDOT and tell processor what to read from there
        // sim requires us to pulse e_nanotrigger to step it
        doldad (DOTJMPDOT, DOTJMPDOT);

        // flick the start switch to clear accumulator, link and start it running
        dostart ();
    }

    // real PDP: holding before TP4 with PC=DOTJMPDOT
    // simulator: just about to fetch the DOTJMPDOT with PC=DOTJMPDOT

    signal (SIGINT, sighand);

    // run random instructions, verifying the cycles
    bool contforcesfetch = false;
    uint16_t lastiszptr = 0xFFFFU;
    uint16_t lastwasisz = 0xFFFFU;
    uint32_t instrno = 0;
    while (true) {

        // once through this loop per instruction

        printf ("%10u  L.AC=%o.%04o PC=%o%04o : ", ++ instrno, linc, acum, xmem_ifld, pctr);

        ////if (instrno == 41) perclock = true;

        uint32_t startclockno = clockno;

        uint16_t effaddr = 0;
        uint16_t opcode  = 0;
        uint16_t randsr  = 0;

        // maybe do a dma cycle
        if (brkrequest && ! contforcesfetch &&
            (! realmode || (dma3cycle ? ! FIELD (Z_RF, f_o_SP_CYC_NEXT) :
                                        ! FIELD (Z_RF, f_o_BF_ENABLE)))) {

            printf ("DMA %d-cycle", (dma3cycle ? 3 : 1));
            if (dma3cycle) {

                // WC state reads wordcount from dmaaddr, increments it and writes it back
                // make wordcount overflow somewhat common for testing
                uint16_t oldwordcount = randbits (12);
                if (oldwordcount & 04000) oldwordcount |= 07700;
                uint16_t newwordcount = (oldwordcount + 1) & 07777;
                printf ("  WC:%04o / %04o => %04o", dmaaddr, oldwordcount, newwordcount);
                memorycycle (g_lbWC, 0, dmaaddr, oldwordcount, newwordcount);

                if (! realmode) {
                    bool wcshouldover = (newwordcount == 0);
                    if (memcycwcover != wcshouldover) {
                        fatalerr ("BWC OVERFLOW is %o should be %o\n", memcycwcover, wcshouldover);
                    }
                }

                // CA state reads pointer from dmaaddr + 1, increments it and writes it back
                // then we use the incremented pointer for the transfer address
                dmaaddr = (dmaaddr + 1) & 07777;
                uint16_t oldcuraddr = randbits (12);
                uint16_t newcuraddr = (oldcuraddr + dmacainc) & 07777;
                printf ("  CA:%04o / %04o => %04o", dmaaddr, oldcuraddr, newcuraddr);
                memorycycle (g_lbCA, 0, dmaaddr, oldcuraddr, newcuraddr);
                dmaaddr = newcuraddr;
            }

            // transfer word at address dmafield:dmaaddr
            uint16_t dmardata = randbits (12);      // random value that will be sent to processor as content of memory location dmafield:dmaaddr
            if (! dmadatain) dmawdata = dmardata;   // if writing, write will be value written to CM_DATA; if reading, write will be same as dmardata

            printf ("  BRK:%o%04o / %04o => %04o", dmafield, dmaaddr, dmardata, dmawdata);
            memorycycle (g_lbBRK, dmafield, dmaaddr, dmardata, dmawdata);

            lastiszptr = 0xFFFFU;
            lastwasisz = 0xFFFFU;
        } else {

            uint8_t os8stepbeforefetch = (xmemat[3] & XM3_OS8STEP) / XM3_OS8STEP0;

            // maybe interrupt request is next
            uint16_t extfetaddr = 0xFFFFU;
            uint8_t datafield = xmem_ifld;
            if (intrequest && ! xmem_intdisableduntiljump && intenabled && ! contforcesfetch &&
                (! realmode || ! FIELD (Z_RF, f_o_LOAD_SF))) {
                printf ("interrupt acknowledge");

                intrequest = false; //TODO: drop request random number of cycles later
                pdpat[Z_RA] = zrawrite &= ~ a_iINT_RQST;

                // save and reset fields
                xmem_saveddfld = xmem_dfld;
                xmem_savedifld = xmem_ifld;
                xmem_dfld = 0;
                xmem_ifld = 0;
                xmem_ifldafterjump = 0;

                // disable interrupts then execute a JMS 0 instruction
                intdelayed = false;
                intenabled = false;
                opcode     = 04000;
            } else {

                // stop when processor is ready to fetch
                if (ctrlcflag) break;

                // we are about to do the fetch after the cont switch
                contforcesfetch = false;

                // delayed ION active now that we are about to do a fetch
                intenabled = intdelayed;

                // send random opcode
                // for IOs, only allow processor (600x) and xmem (62xx)
                while (true) {
                    opcode = randbits (12);
                    if ((opcode & 07400) == 07400) {
                        opcode &= 07770;    // don't do OSR,HLT,Group 3
                    }
                    if ((opcode & 07400) == 07000) {
                        if ((opcode & 016) == 002) continue;    // bsw
                        if ((opcode & 014) == 014) continue;    // 6,7
                    }
                    if ((opcode & 07000) != 06000) break;
                    if (opcode == 06001) break;
                    if (opcode == 06002) break;
                    if ((opcode & 07700) == 06200) break;
                }
                printf ("OP=%04o  %s", opcode, disassemble (opcode, pctr).c_str ());

                // set random switch register contents if about to do an OSR instruction
                if (((opcode & 07401) == 07400) && (opcode & 0004)) {
                    randsr = randbits (12);
                    pdpat[Z_RB] = randsr * b_swSR0;
                }

                // perform fetch memory cycle
                extfetaddr = (xmem_ifld << 12) | pctr;
                if (TESTOS8ZAP && ((lastwasisz + 1) == ((xmem_ifld << 12) | pctr)) && (opcode == (05200 | (lastwasisz & 00177)))) {
                    // pdp8lxmem.v is changing the fetch JMP .-1 to fetch NOP, writeback ISZ value with 0
                    printf (" os8zapped");
                    memorycyclx (g_lbFET,
                        xmem_ifld, pctr,    // pdp8lxmem.v reads opcode from usual ifld:pctr
                        opcode,             // it gets the the JMP .-1 opcode from memory
                        07000,              // it sends a NOP to the processor
                        07000,              // the processor sends a NOP as writeback for the fetch
                        lastiszptr, 0);     // pdp8lxmem.v writes a zero to the ISZs counter
                    opcode = 07000;         // process the NOP just like the processor is
                } else {
                    // anything else, fetch from ifld/pctr
                    memorycycle (g_lbFET, xmem_ifld, pctr, opcode, opcode);
                }
                effaddr = ((opcode & 00200) ? (pctr & 07600) : 0) | (opcode & 00177);
                pctr = (pctr + 1) & 07777;

                // maybe do a defer cycle
                if ((opcode & 07000) < 06000) {
                    printf (" =");
                    if (opcode & 0400) {
                        uint16_t pointer = randbits (12);
                        uint16_t autoinc = (pointer + ((effaddr & 07770) == 00010)) & 07777;
                        printf (" %o%04o /", xmem_ifld, effaddr);
                        memorycycle (g_lbDEF, xmem_ifld, effaddr, pointer, autoinc);
                        effaddr = autoinc;
                        datafield = xmem_dfld;
                    }
                }
            }

            // do the exec cycle
            lastiszptr = 0xFFFFU;
            lastwasisz = 0xFFFFU;
            switch (opcode >> 9) {

                // and, tad, isz, dca
                case 0: case 1: case 2: case 3: {
                    uint16_t operand = randbits (12);
                    printf (" %o%04o / %04o", datafield, effaddr, operand);
                    switch (opcode >> 9) {
                        case 0: {
                            memorycycle (g_lbEXE, datafield, effaddr, operand, operand);
                            acum &= operand;
                            printf (" => %04o", acum);
                            break;
                        }
                        case 1: {
                            memorycycle (g_lbEXE, datafield, effaddr, operand, operand);
                            uint16_t sum = acum + (linc ? 010000 : 0) + operand;
                            acum = sum & 07777;
                            linc = (sum >> 12) & 1;
                            printf (" => %o.%04o", linc, acum);
                            break;
                        }
                        case 2: {
                            uint16_t incd = (operand + 1) & 07777;
                            printf (" => %04o", incd);
                            memorycycle (g_lbEXE, datafield, effaddr, operand, incd);
                            if (incd == 0) pctr = (pctr + 1) & 07777;

                            // if ISZ direct page, save operand 15-bit address and opcode 15-bit address
                            // but if last instruction had an exec cycle that read something that looked like an ISZ instruction,
                            // ...pdp8lxmem.v won't detect this ISZ so skip this test in that case
                            //   TAD xxxx ; last instruction, xxxx contained 2347 so it sets os8step=1 cuz it thinks 2347 might be an ISZ opcode
                            //   ISZ yyyy ; this instruction, the fetch doesn't do read/modify+1/write so it knows the 2347 isn't an opcode and sets os8step=0
                            //   JMP .-1  ; next instruction, will be processed as normal cuz os8step=0
                            // the TAD could be an ISZ in which case we get os8step=2 with the same result
                            //   ISZ xxxx ; last instruction, sets os8step=2 cuz it is an ISZ and looking for JMP next
                            //   ISZ yyyy ; this instruction, not a JMP so it sets os8step=0
                            //   JMP .-1  ; next instruction, will be processed as normal cuz os8step=0
                            // in either case, it should successfully zap after JMP .-1 is taken cuz the JMP .-1 leaves os8step=0
                            if ((os8stepbeforefetch == 0) && ((opcode & 07600) == 02200) && ((extfetaddr & 0177) != 0177)) {
                                lastiszptr = (datafield << 12) | effaddr;
                                lastwasisz = extfetaddr;
                            }
                            break;
                        }
                        case 3: {
                            printf (" <= %04o", acum);
                            memorycycle (g_lbEXE, datafield, effaddr, operand, acum);
                            acum = 0;
                            break;
                        }
                        default: ABORT ();
                    }
                    break;
                }

                // jms
                case 4: {
                    xmem_intdisableduntiljump = false;
                    xmem_ifld = xmem_ifldafterjump;
                    uint16_t operand = randbits (12);
                    printf (" %o%04o / %04o <= %04o", xmem_ifld, effaddr, operand, pctr);
                    memorycycle (g_lbEXE, xmem_ifld, effaddr, operand, pctr);
                    pctr = (effaddr + 1) & 07777;
                    break;
                }

                // jmp
                case 5: {
                    xmem_intdisableduntiljump = false;
                    xmem_ifld = xmem_ifldafterjump;
                    pctr = effaddr;
                    printf (" %o%04o", xmem_ifld, pctr);
                    break;
                }

                // iot
                case 6: {
                    ASSERT ((f_oBIOP1 == 1) && (f_oBIOP2 == 2) && (f_oBIOP4 == 4));
                    printf (" / %04o", acum);
                    for (uint16_t m = 1; m <= 4; m += m) {
                        if (opcode & m) {

                            // if -half or -real, the processor is waiting just before TP4 so has already performed the I/O instruction
                            //    so we can't wait for the IOPm pulses because they have already gone by
                            // for full simulator mode, we manually clock it so will see the IOPm pulses
                            if (! realmode) {

                                // wait for processor to assert IOPm
                                for (int i = 0; ! (pdpat[Z_RF] & m); i ++) {
                                    if (i > 1000) fatalerr ("timed out waiting for IOP%u asserted\n", m);
                                    clockit ();
                                }

                                // it should be sending the accumulator out
                                verify12 (Z_RH, h_oBAC, acum, "AC invalid going out during IOP");
                            }

                            // maybe it is an io opcode that some fpga device we have enabled processes
                            bool acclear    = false;
                            bool ioskip     = false;
                            uint16_t acbits = 0;

                            // - pdp8lsim.v itself processes these io opcodes
                            if (opcode == 06001) {
                                intdelayed = true;
                                goto iodone;
                            }
                            if (opcode == 06002) {
                                intdelayed = false;
                                intenabled = false;
                                goto iodone;
                            }

                            // - pdp8lxmem.v processes these io opcodes (manipulate memory field bits)
                            if ((opcode & 07700) == 06200) {
                                if (! (opcode & 4)) {
                                    if (opcode & 1) xmem_dfld = (opcode >> 3) & 7;
                                    if (opcode & 2) {
                                        xmem_ifldafterjump = (opcode >> 3) & 7;
                                        xmem_intdisableduntiljump = true;
                                    }
                                } else if ((opcode & 7) == 4) {
                                    switch ((opcode >> 3) & 7) {
                                        case 1: {   // 6214
                                            acbits = xmem_dfld << 3;
                                            break;
                                        }
                                        case 2: {   // 6224
                                            acbits = xmem_ifld << 3;
                                            break;
                                        }
                                        case 3: {   // 6234
                                            acbits = (xmem_savedifld << 3) | xmem_saveddfld;
                                            break;
                                        }
                                        case 4: {   // 6244
                                            printf (" [ifaj=%o dfld=%o]", xmem_savedifld, xmem_saveddfld);
                                            xmem_dfld          = xmem_saveddfld;
                                            xmem_ifldafterjump = xmem_savedifld;
                                            break;
                                        }
                                    }
                                }
                                goto iodone;
                            }

                            // those are the only I/O instructions we know how to process
                            ABORT ();

                        iodone:;
                            printf (" => IOP%o => %c%c%04o", m, (acclear ? 'C' : ' '), (ioskip ? 'S' : ' '), acbits);

                            if (! realmode) {

                                // wait for processor to drop IOPm
                                for (int i = 0; pdpat[Z_RF] & m; i ++) {
                                    if (i > 1000) fatalerr ("timed out waiting for IOP%u negated\n", m);
                                    clockit ();
                                }

                                // clear the random bits so it won't clock them again during nulled-out io pulses
                                pdpat[Z_RA] = zrawrite &= ~ a_iAC_CLEAR & ~ a_iIO_SKIP;
                                pdpat[Z_RC] = zrcwrite &= ~ c_iINPUTBUS;
                            }

                            // update our shadow registers
                            if (acclear) acum = 0;
                            if (ioskip)  pctr = (pctr + 1) & 07777;
                            acum |= acbits;
                        }
                    }
                    break;
                }

                // opr
                case 7: {
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
                            printf (" skip");
                            pctr = (pctr + 1) & 07777;
                        }
                        if (opcode & 0200) acum  = 0;
                        if (opcode & 0004) acum |= randsr;
                        printf (" => %04o", acum);
                        if (opcode & 0002) {
                            for (int i = 0; FIELD (Z_RF, f_oB_RUN); i ++) {
                                if (i > 1000) fatalerr ("timed out waiting for RUN negated after HLT instruction\n");
                                clockit ();
                            }
                            printf (" cont");
                            docont ();
                            contforcesfetch = true; // cont switch in pdp8lsim.v always does fetch next (no brk, irq, etc)
                            ASSERT (! realmode);
                            goto vfyacetc;          // pdp8lsim.v already at beginning of TS1BODY of next instruction
                        }
                    }
                }
            }
        }

        if (realmode) {
            printf ("\n");
        } else {

            // step to TS_TSIDLE so we know everything has been updated
            for (int i = 0; FIELD (Z_RK, k_timestate) != TS_TSIDLE; i ++) {
                if (i > 1000) fatalerr ("timed out clocking to TS_TSIDLE\n");
                clockit ();
            }

            // the accumulator and link should be up to date
        vfyacetc:;
            verify12  (Z_RI, i_lbAC,   acum, "AC mismatch at end of cycle");
            verifybit (Z_RG, g_lbLINK, linc, "LINK mismatch at end of cycle");

            // print how many cycles it took (each clock is 10nS)
            uint32_t clocks = clockno - startclockno;
            printf ("  (%u.%02u uS)\n", clocks / 100, clocks % 100);
        }

        ////printshadow (stdout);
    }

    fprintf (stderr, "stopping for control-C\n");
    if (realmode) {
        memorycycle (g_lbFET, xmem_ifld, pctr, 07402, 07402);
        xmemat[1] &= ~ XM_MWHOLD & ~ XM_MWSTEP;
        usleep (20);
        if (FIELD (Z_RF, f_oB_RUN)) {
            fprintf (stderr, "processor failed to halt, probably need to do -clear\n");
        }
    }

    return 0;
}

static void sighand (int signum)
{
    if (ctrlcflag) {
        dprintf (STDERR_FILENO, "\naborting from control-C\n");
        exit (1);
    }
    ctrlcflag = true;
}

// do a 'load address' operation, verifying the memory access
//  input:
//   addr = address to load
//   data = data to send processor for the corresponding read
static void doldad (uint16_t addr, uint16_t data)
{
    // put address in switch register and press load address switch
    pdpat[Z_RB] = addr * b_swSR0 | b_swLDAD;

    // clock some cycles to arm the debounce circuit
    debounce ();

    // release load address switch, leave address there
    pdpat[Z_RB] = addr * b_swSR0;

    // it should now do a memory cycle, reading and writing that same data
    memorycycle (0, xmem_ifld, addr, data, data);
}

// do a 'start' operation
static void dostart ()
{
    // wait for TS_TSIDLE so businit doesn't mess up memory cycle
    for (int i = 0; FIELD (Z_RK, k_timestate) != TS_TSIDLE; i ++) {
        if (i > 1000) fatalerr ("timed out waiting for TS_TSIDLE before pressing start switch\n");
        clockit ();
    }

    // press start switch
    pdpat[Z_RB] |= b_swSTART;

    // clock some cycles to arm the debounce circuit
    debounce ();

    // release start switch
    pdpat[Z_RB] &= ~ b_swSTART;

    // clock until the run flipflop sets
    for (int i = 0; ! FIELD (Z_RF, f_oB_RUN); i ++) {
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
    for (int i = 0; ! FIELD (Z_RF, f_oB_RUN); i ++) {
        if (i > 1000) fatalerr ("timed out waiting for RUN after flicking CONT switch\n");
        clockit ();
    }

    uint32_t ts = FIELD (Z_RK, k_timestate);
    if (ts != TS_TS1BODY) {
        fatalerr ("not in TS1BODY after CONT switch (%u)\n", ts);
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
//   memcycwcover = processor's ! o_BWC_OVERFLOW at beg of TS3
//
// steps through:
//   TS1 = reading memory location
//   TS2 = computing any modification to value
//   TS3 = writing memory location
// after that:
//   IOP1,2,4 = only if this was a fetch of an io instruction
//   TS4 = any post-processing for the cycle
static void memorycycle (uint32_t state, uint8_t field, uint16_t addr, uint16_t rdata, uint16_t wdata)
{
    memorycyclx (state, field, addr, rdata, rdata, wdata, (field << 12) | addr, wdata);
}

// - rdbkdata = data sent to processor as result of read
// - wtbkdata = data received back from processor
// - vfywaddr = address in extmem[] to verify the write happened
// - vfywdata = data that should be at vfywaddr
static void memorycyclx (uint32_t state, uint8_t field, uint16_t addr, uint16_t rdata, uint16_t rdbkdata, uint16_t wtbkdata, uint16_t vfywaddr, uint16_t vfywdata)
{
    if (realmode) {

        // holding just before TP4 of previous cycle

        requestdmaorint (state, rdata);

        // write the read data to the given address so PDP can read it
        // we have to use pdp8lxmem.v's low 4K because we can access it directly from the arm processor
        // and because it is the only way we can hold the PDP
        uint16_t xaddr = (field << 12) | addr;
        extmemptr[xaddr] = rdata;

        // tell pdp8lxmem.v to let the PDP execute TP4 of the previous cycle up to just before TP4 of this cycle
        xmemat[1] |= XM_MWHOLD | XM_MWSTEP;
        for (int i = 0; xmemat[1] & XM_MWSTEP; i ++) {
            if (i > 1000) fatalerr ("timed out waiting for MWSTEP to clear\n");
        }

        // holding just before TP4 of this cycle

        // verify the memory contents after writeback
        ASSERT (vfywaddr <= 077777);
        uint16_t vdata = extmemptr[vfywaddr];
        if (vdata != vfywdata) {
            fatalerr ("address %05o contained %04o at end of cycle, should be %04o\n", vfywaddr, vdata, vfywdata);
        }
    } else {

        // clock until we see TS1
        for (int i = 0; ! FIELD (Z_RF, f_oBTS_1); i ++) {
            if (i > 1000) fatalerr ("timed out waiting for TS1 asserted\n");
            clockit ();
        }

        // we should have MEMSTART now
        if (! FIELD (Z_RF, f_oMEMSTART)) fatalerr ("MEMSTART missing at beginning of TS1\n");

        // cpu should be outputting address we expect
        verify12 (Z_RI, i_oMA, addr, "MA bad at beginning of mem cycle");
        uint8_t actualfield = (xmemat[2] & XM2_FIELD) / XM2_FIELD0;
        if (actualfield != field) {
            fatalerr ("actual memory field %o expected %o", actualfield, field);
        }

        // should now be fully transitioned to the major state
        if ((state != 0) && ! FIELD (Z_RG, state)) fatalerr ("not in state %08X during mem cycle\n", state);

        // write the read data to the given address so cpu can read it
        // we have to use pdp8lxmem.v's low 4K because we can access it directly from the arm processor
        // the pip8lsim.v's 4K memory can only be accessed via front panel switches and lights
        // ...(analagous to real PDP-8/L's core memory, slow and processor would have to be halted)
        uint16_t xaddr = (field << 12) | addr;
        extmemptr[xaddr] = rdata;

        // end of TS1 means it finished reading that memory location into MB
        for (int i = 0; FIELD (Z_RF, f_oBTS_1); i ++) {
            if (i > 1000) fatalerr ("timed out waiting for TS1 negated\n");
            clockit ();
        }

        // MB should now have the value we wrote to the memory location
        verify12 (Z_RH, h_oBMB, rdbkdata, "MB during read");

        requestdmaorint (state, rdata);

        // wait for it to enter TS3, meanwhile cpu (pdp8lsim.v) should possibly be modifying
        // the value and sending the value (modified or not) back to the memory
        for (int i = 0; ! FIELD (Z_RF, f_oBTS_3); i ++) {
            if (i > 1000) fatalerr ("timed out waiting fot TS3 asserted\n");
            clockit ();
        }

        // beginning of TS3, MB should now have the possibly modified value
        verify12 (Z_RH, h_oBMB, wtbkdata, "MB during writeback");

        // BWC_OVERFLOW is valid
        // - clocked with TP2 (vol 2, p5 D-5)
        // - cleared with TP3
        memcycwcover = ! FIELD (Z_RF, f_o_BWC_OVERFLOW);

        // wait for MEMDONE, where pdp8lxmem.v has written the value to memory
        for (int i = 0; FIELD (Z_RA, a_i_MEMDONE); i ++) {
            if (i > 1000) fatalerr ("timed out waiting for MEMDONE asserted\n");
            clockit ();
        }

        // verify the memory contents
        ASSERT (vfywaddr <= 077777);
        uint16_t vdata = extmemptr[vfywaddr];
        if (vdata != vfywdata) {
            fatalerr ("address %05o contained %04o at end of cycle, should be %04o\n", vfywaddr, vdata, vfywdata);
        }
    }

    if (shat[1] & 0xFFFFU) {
        printf ("\n");
        printshadow (stderr);
        fatalerr ("shadow error %08X\n", shat[1]);
    }
}

// maybe request dma or interrupt
// processor samples it at end of TS3
static void requestdmaorint (uint32_t state, uint16_t rdata)
{
    if ((state == g_lbWC) || (state == g_lbCA) || (state == g_lbBRK)) brkrequest = false;
    else if (! brkrequest) {
        brkrequest = randbits (8) == 0;
        if (brkrequest) {
            dmafield  = randbits (3);
            dmaaddr   = randbits (12);
            dmadatain = randbits (1);       // 0=read memory; 1=write memory
            dmawdata  = randbits (12);
            dma3cycle = randbits (2) == 0;
            dmacainc  = dma3cycle && randbits (1);
            printf ("  <<brkreq=1 3cycle=%o addr=%o%04o", dma3cycle, dmafield, dmaaddr);
            if (dmadatain) printf (" wdata=%04o", dmawdata);
            printf (">> ");
            if (cmemat[1] & CM_BUSY) {
                fatalerr ("CM_BUSY set when about to request dma cycle");
            }
            cmemat[2] = (cmemat[2] & CM2_NOBRK) | (dma3cycle ? CM2_3CYCL : 0) | (dmacainc ? CM2_CAINC : 0);
            cmemat[1] = CM_ENAB | dmawdata * CM_DATA0 | (dmadatain ? CM_WRITE : 0) | (dmafield * 4096 + dmaaddr) * CM_ADDR0;
            if (! (cmemat[1] & CM_BUSY)) {
                fatalerr ("CM_BUSY clear just after requested dma cycle");
            }
        }
    }
    if (! brkrequest) pdpat[Z_RA] = zrawrite &= ~ a_iBRK_RQST;
    if (! intrequest && ((state != g_lbFET) || ((rdata & 07403) != 07402))) {
        intrequest = randbits (10) == 0;
        if (intrequest) {
            printf ("  <<intreq=1 intena=%d>> ", intenabled);
            pdpat[Z_RA] = zrawrite |= a_iINT_RQST;
        }
    }
}

// one of the debounceable switches was just pressed
// wait for debounced signal to be set
static void debounce ()
{
    // wait for TS_TSIDLE so switch release will be detected
    for (int i = 0; FIELD (Z_RK, k_timestate) != TS_TSIDLE; i ++) {
        if (i > 1000) fatalerr ("timed out waiting for TS_TSIDLE before waiting for debounce\n");
        clockit ();
    }

    // wait for debounce counter to be clocked
    for (int i = 0; ! FIELD (Z_RG, g_debounced); i ++) {
        if (i > 10000000) fatalerr ("debounced flag failed to set while switch held down\n");
        clockit ();
    }
}

// gate through one fpga clock cycle
static void clockit ()
{
    pdpat[Z_RE] = zrewrite | e_nanotrigger;
    clockno ++;

    if (perclock) {
        uint32_t timestate = FIELD (Z_RK, k_timestate);
        //uint8_t dfld = (xmemat[2] & XM2_DFLD) / XM2_DFLD0;
        //uint8_t ifld = (xmemat[2] & XM2_IFLD) / XM2_IFLD0;
        //uint8_t ifaj = (xmemat[2] & XM2_IFLDAFJMP) / XM2_IFLDAFJMP0;
        //uint8_t savedifld = (xmemat[2] & XM2_SAVEDIFLD) / XM2_SAVEDIFLD0;
        //uint8_t saveddfld = (xmemat[2] & XM2_SAVEDDFLD) / XM2_SAVEDDFLD0;
        //uint16_t os8step = (xmemat[3] & XM3_OS8STEP) / XM3_OS8STEP0;

        printf ("clockit*: %9u timestate=%-7s timedelay=%2u majstate=%-5s nextmajst=%-5s oMEMSTART=%o i_STROBE=%o i_MEM=%04o xbr[%05o] ce=%o we=%o rd=%04o wd=%04o\n",
            clockno, timestatenames[timestate], FIELD(Z_RK,k_timedelay),
            majstatenames[FIELD(Z_RK,k_majstate)], majstatenames[FIELD(Z_RK,k_nextmajst)],
            FIELD(Z_RF,f_oMEMSTART), FIELD(Z_RA,a_i_STROBE), FIELD(Z_RC,c_i_MEM),
            FIELD(Z_RL,l_xbraddr), FIELD(Z_RL,l_xbrenab), FIELD(Z_RL,l_xbrwena), FIELD(Z_RM,m_xbrrdat), FIELD(Z_RM,m_xbrwdat));
    }
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

static void printshadow (FILE *out)
{
    char *shst = formatshadow (shat);
    fprintf (out, "            shadow: %s\n", shst);
    free (shst);
}

static void fatalerr (char const *fmt, ...)
{
    printf ("\n");
    va_list ap;
    va_start (ap, fmt);
    vfprintf (stderr, fmt, ap);
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
    fprintf (stderr, "clockno=%u ir=%o majstate=%s timestate=%s timedelay=%u\n",
        clockno, ir, majstatenames[majstate], timestatenames[timestate], timedelay);
    uint8_t dfld = (xmemat[2] & XM2_DFLD) / XM2_DFLD0;
    uint8_t ifld = (xmemat[2] & XM2_IFLD) / XM2_IFLD0;
    uint8_t ifaj = (xmemat[2] & XM2_IFLDAFJMP) / XM2_IFLDAFJMP0;
    fprintf (stderr, "  dfld=%o ifld=%o ifaj=%o\n", dfld, ifld, ifaj);
}
