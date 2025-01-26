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

// Verify the PDP-8/L operation, memory cycle by cycle
// Requires xmem enlo4k set so it sees all data read from memory

module pdp8lshad (
    input CLOCK, CSTEP, RESET,  // fpga 100MHz clock and reset

    input armwrite,
    input[2:0] armraddr, armwaddr,
    input[31:00] armwdata,
    output[31:00] armrdata,

    input iBEMA,                // B35-T2,p5 B-7,J11-45,,B25,"if low, blocks mem protect switch"
    input i_CA_INCRMNT,         // C35-M2,p15 A-3,J11-30,,C25,?? NOT ignored in PDP-8/L see p2 D-2
    input i_DATA_IN,            // C36-M2,p15 B-2,J11-32,,,
    input[11:00] iINPUTBUS,     // D34-B1,p15 B-8,PIOBUSA,,,gated out to CPU by x_INPUTBUS
    input iMEMINCR,             // C36-T2,p15 B-1,J11-18,,,
    input[11:00] i_MEM,         // B35-D1,p19 C-7,MEMBUSH,,,gated out to CPU by x_MEM
    input i_MEM_P,              // B35-B1,p19 C-8,MEMBUSA,,,gated out to CPU by x_MEM
    input i3CYCLE,              // C35-K2,p15 A-3,J11-38,,C22,
    input iAC_CLEAR,            // D34-P2,p15 C-2,J12-27,,D33,gated out to CPU by x_INPUTBUS
    input iBRK_RQST,            // C36-K2,p15 B-3,J11-36,,,"used on p5 B-2, clocked by TP1"
    input[11:00] iDMAADDR,      // C36-B1,p15 B-8,DMABUSB,,,gated out to CPU by x_DMAADDR
    input[11:00] iDMADATA,      // C35-B1,p15 A-8,DMABUSA,,,gated out to CPU by x_DMADATA
    input i_EA,                 // B34-B1,p18 C-8,J11-53,,B30,high: use CPU core stack for mem cycle; low: block using CPU core stack
    input iEMA,                 // B35-V2,p5 B-7,J11-51,,B29,goes to EA light bulb on front panel
    input iINT_INHIBIT,         // B36-L1,p9 B-3,J12-66,,,
    input iINT_RQST,            // D34-M2,p15 C-3,J12-23,,D32,open collector out to CPU
    input iIO_SKIP,             // D34-K2,p15 C-3,J12-28,,D30,gated out to CPU by x_INPUTBUS
    input i_MEMDONE,            // B34-V2,p4 C-7,J11-55,,B32,gated out to CPU by x_MEM
    input i_STROBE,             // B34-S2,p4 C-6,J11-59,,B35,gated out to CPU by x_MEM

    input[11:00] oBAC,          // D36-B1,p15 D-8,PIOBUSA,,,gated onto PIOBUS by r_BAC
    input oBIOP1,               // D36-K2,p15 D-3,J12-34,,,
    input oBIOP2,               // D36-M2,p15 D-3,J12-32,,,
    input oBIOP4,               // D36-P2,p15 D-2,J12-26,,,
    input[11:00] oBMB,          // D35-B1,p15 D-8,PIOBUSH,,,gated from CPU onto PIOBUS by r_BMB
    input oBTP2,                // B34-T2,p4 C-5,J11-61,,B36,
    input oBTP3,                // B36-H1,p4 C-4,J12-73,,,
    input oBTS_1,               // D36-T2,p15 D-1,J12-25,,,
    input oBTS_3,               // D36-S2,p15 D-2,J12-22,,,
    input o_BWC_OVERFLOW,       // C35-P2,p15 A-2,J11-16,,C33,
    input o_B_BREAK,            // C36-P2,p15 B-2,J11-26,,,
    input oE_SET_F_SET,         // B36-D2,p22 C-3,J12-72,,,
    input oJMP_JMS,             // B36-E2,p22 C-3,J11-63,,,
    input oLINE_LOW,            // B36-V2,p18 B-7,J12-43,,,
    input[11:00] oMA,           // B34-D1,p22 D-8,MEMBUSH,,,gated from CPU onto MEMBUS by r_MA
    input oMEMSTART,            // B34-P2,p4 D-8,J11-57,,B33,
    input o_ADDR_ACCEPT,        // C36-S2,p15 S-2,J11-22,,,
    input o_BF_ENABLE,          // B36-E1,p22 C-6,J12-69,,,
    input oBUSINIT,             // C36-V2,p15 B-1,J11-9,,,active high bus init
    input oB_RUN,               // D34-S2,p15 C-1,J12-29,,D36,run flipflop on p4 B-2
    input o_DF_ENABLE,          // B36-B1,p22 C-7,J12-65,,,
    input o_KEY_CLEAR,          // B36-J1,p22 C-5,J12-68,,,
    input o_KEY_DF,             // B36-S1,p22 C-4,J12-44,,,
    input o_KEY_IF,             // B36-P1,p22 C-4,J12-46,,,
    input o_KEY_LOAD,           // B36-H2,p22 C-3,J11-85,,,
    input o_LOAD_SF,            // B36-M1,p22 C-5,J12-52,,,
    input o_SP_CYC_NEXT,        // B36-D1,p22 C-6,J12-67,,,

    output reg[3:0] majstate, nextmajst, timedelay, timestate,
    output error
);

    localparam MS_UNKN  =  0;       // major state unknown
    localparam MS_FETCH =  1;       // memory cycle is fetching instruction
    localparam MS_DEFER =  2;       // memory cycle is reading pointer
    localparam MS_EXEC  =  3;       // memory cycle is for executing instruction
    localparam MS_WRDCT =  4;       // memory cycle is for incrementing dma word count
    localparam MS_CURAD =  5;       // memory cycle is for reading dma address
    localparam MS_BREAK =  6;       // memory cycle is for dma data word transfer
    localparam MS_INTAK =  7;       // memory cycle is for interrupt acknowledge
    localparam MS_START =  8;       // memory cycle initiated by START key

    localparam TS_U =  0;       // time state unknown
    localparam TS_1 =  1;       // read memory (EA sampled at posedge TS1)
                                // - TP1: sample BRK_RQST; inc PC (FETCH); clear ADDR_ACCEPT
    localparam TS_2 =  2;       // compute update to memory
                                // - TP2: load IR (FETCH); load MB; load WC_OVERFLOW
    localparam TS_3 =  3;       // write memory, update registers
                                // - TP3: load L,AC (AND,TAD,DCA,OPR); load PC (jumps, skips); sample INT_RQST
    localparam TS_4 =  4;       // determine next state
                                // - TP4: set IR=4 (INTAK next); load F,D,E,WC,CA,B; load MA; load ADDR_ACCEPT (BREAK)

                                // we do io pulses as part of TS_4
                                // - AC,PC updated at end of each pulse

                                // STROBE is exactly TP1
                                // MEMDONE can happen any time after TP1 but typically 800nS+
                                // - see MEM_IDLE ff, TP4 will wait for it
                                // - WC_OVERFLOW cleared every MEMDONE

    reg acknown, eaknown, irknown, lnknown, maknown, mbknown, pcknown;
    reg[11:00] acum, eadr, ireg, madr, mbuf, pctr;
    reg[15:00] err;
    reg clearit, link;

    assign error = (err != 0);

    assign armrdata = (armraddr == 0) ? 32'h5348200B : // [31:16] = 'SH'; [15:12] = (log2 nreg) - 1; [11:00] = version
                      (armraddr == 1) ? { acknown, eaknown, irknown, lnknown, maknown, mbknown, pcknown, 8'b0, clearit, err } :
                      (armraddr == 2) ? { majstate, timestate, ireg, pctr } :
                      (armraddr == 3) ? { timedelay, 4'b0, mbuf, madr } :
                      (armraddr == 4) ? { 7'b0, link, acum, eadr } :
                      32'hDEADBEEF;

    // calculate accumulator and link for group 1 instruction
    reg[11:00] g1acum, g1acumraw;
    reg g1link, g1linkraw;
    reg g1acknraw, g1lnknraw;
    reg g1acknown, g1lnknown;

    always @(*) begin

        // handle cla, cll, cma, cll, iac bits
        { g1linkraw, g1acumraw } = { (ireg[06] ? 1'b0 : link) ^ ireg[04], (ireg[07] ? 12'b0 : acum) ^ (ireg[05] ? 12'o7777 : 12'o0000) } + { 12'b0, ireg[00] };
        g1acknraw = acknown | ireg[07];     // cla makes acum known
        g1lnknraw = lnknown | ireg[06];     // cll makes link known

        // handle rotate bits
        case (ireg[03:01])

            // rol 1
            2: begin
                { g1link, g1acum } = { g1acumraw, g1linkraw };
                g1acknown = g1acknraw & g1lnknraw;
                g1lnknown = g1acknraw;
            end

            // rol 2
            3: begin
                { g1link, g1acum } = { g1acumraw[10:00], g1linkraw, g1acumraw[11] };
                g1acknown = g1acknraw & g1lnknraw;
                g1lnknown = g1acknraw;
            end

            // ror 1
            4: begin
                { g1link, g1acum } = { g1acumraw[00], g1linkraw, g1acumraw[11:01] };
                g1acknown = g1acknraw & g1lnknraw;
                g1lnknown = g1acknraw;
            end

            // ror 2
            5: begin
                { g1link, g1acum } = { g1acumraw[01:00], g1linkraw, g1acumraw[11:02] };
                g1acknown = g1acknraw & g1lnknraw;
                g1lnknown = g1acknraw;
            end

            // all else, nop
            default: begin
                { g1link, g1acum } = { g1linkraw, g1acumraw };
                g1acknown = g1acknraw;
                g1lnknown = g1lnknraw;
            end
        endcase
    end

    // calculate accumulator and program counter for group 2 instruction

    // - maybe increment PC for skip
    wire g2pcknown = pcknown & (~ ireg[06] | acknown) & (~ ireg[05] | acknown) & (~ ireg[04] | lnknown);
    wire[11:00] g2pctr = pctr + 1 + (
                  ((ireg[06] & acum[11]) |    // SMA
                   (ireg[05] & (acum == 0)) | // SZA
                   (ireg[04] & link)) ^       // SNL
                    ireg[03]                  // reverse
        );

    // - maybe clear accumulator
    //   clearing AC makes it known, but reading SR makes it unknown
    wire g2acknown = (acknown | ireg[07]) & ~ ireg[02];
    wire[11:00] g2acum = ireg[07] ? 0 : acum;

    // misc state

    reg didio, dmabrk, intack, wcca, wcov, wcovkwn;
    reg last_dmabrk, last_intack, last_wcca, lastbiop, last_jmpjms, lastts1, lastts3;

    reg[7:0] breaksr, intacksr, wccasr;
    wire[3:0] nextfetch = breaksr[7] ? MS_BREAK :
            wccasr[7] ? (wcca ? MS_CURAD : MS_WRDCT) :
            intacksr[7] ? MS_INTAK : MS_FETCH;

    wire[11:00] autoinc = ~ i_MEM + ((oMA[11:03] == 1) ? 1 : 0);
    wire[11:00] brkdata = i_DATA_IN ? ~ i_MEM : iDMADATA;
    wire[11:00] curaddr = ~ i_MEM + (i_CA_INCRMNT ? 0 : 1);

    wire[3:0] timedelayinc = (timedelay == 15) ? 15 : timedelay + 1;

    // main processing loop

    always @(posedge CLOCK) begin

        if (RESET) begin
            clearit <= 1;
        end

        // arm processor is writing one of the registers
        else if (armwrite) begin
            case (armwaddr)
                1: begin
                    err <= armwdata[15:00];
                    clearit <= armwdata[16];
                end
            endcase
        end

        else if (CSTEP) begin

            // pulse from START swtich during MFTP0
            if (oBUSINIT | clearit) begin
                acknown     <= oBUSINIT;
                acum        <= 0;
                breaksr     <= 0;
                clearit     <= 0;
                didio       <= 0;
                dmabrk      <= 0;
                eadr        <= 0;
                eaknown     <= 0;
                err         <= 0;
                intack      <= 0;
                intacksr    <= 0;
                ireg        <= 0;
                irknown     <= 0;
                last_dmabrk <= 0;
                last_intack <= 0;
                last_jmpjms <= 0;
                last_wcca   <= 0;
                lastbiop    <= 0;
                lastts1     <= 0;
                lastts3     <= 0;
                link        <= 0;
                lnknown     <= oBUSINIT;
                madr        <= 0;
                maknown     <= 0;
                majstate    <= oBUSINIT ? MS_START : MS_UNKN;
                mbknown     <= 0;
                mbuf        <= 0;
                nextmajst   <= oBUSINIT ? MS_FETCH : MS_UNKN;
                pcknown     <= 0;
                pctr        <= 0;
                timedelay   <= 0;
                timestate   <= TS_U;
                wcca        <= 0;
                wccasr      <= 0;
            end

            else if (~ error) begin

                // TS1 and TS3 should always have at least 1 cycle gap (more like dozens)
                if ((oBTS_1 | lastts1) & (oBTS_3 | lastts3)) begin
                    err[00] <= 1;
                end

                else case (timestate)

                    // just starting, sync up with TS1/TS3 edges
                    TS_U: begin
                        timedelay <= 0;
                             if (~ lastts1 & oBTS_1) timestate <= TS_1;
                        else if (lastts1 & ~ oBTS_1) timestate <= TS_2;
                        else if (~ lastts3 & oBTS_3) timestate <= TS_3;
                        else if (lastts3 & ~ oBTS_3) timestate <= TS_4;
                    end

                    // majstate = MS_START: just pressed START key
                    //            else: previous cycle major state
                    // nextmajst = what we expect this cycle major state to be
                    // acum,link,pctr = should match what PDP/sim has
                    // madr,mbuf = garbazhe
                    // eadr = depends on previous major state
                    //        MS_FETCH: operand address not yet deferred
                    //        MS_DEFER: operand address with increment if any
                    //        MS_CURAD: transfer address
                    //        else: garbazhe

                    // beginning of new major state
                    TS_1: begin
                        if (i_EA) err[13] <= 1;     // can't see core stack read data
                        if (oBTS_1) begin
                            timedelay <= timedelayinc;
                        end else begin
                            didio     <= 0;
                            dmabrk    <= ~ o_B_BREAK;
                            timedelay <= 0;
                            timestate <= TS_2;
                        end
                    end

                    // oMA = memory address for this cycle
                    // i_MEM = value read from that location

                    TS_2: begin
                        if (oBTS_1) err[02] <= 1;
                        if (~ oBTS_3) begin
                            timedelay <= timedelayinc;
                        end else begin
                            timedelay <= 0;
                            timestate <= TS_3;
                        end
                    end

                    // oBMB = value to be written back to memory

                    TS_3: begin
                        if (oBTS_3) begin

                            // MB gets loaded at end of TS2 with value to be written to memory
                            // IR gets loaded at end of TS2 if this is a FETCH
                            //  80nS: capture/compute major state
                            //  90nS: verify major state, compute IR (if FETCH)
                            // 100nS: compute what MA, MB should be
                            // 110nS: verify address, writeback

                            case (timedelay)

                                // try to compute current state
                                8: begin

                                    // PDP directly says this is BREAK cycle
                                    if (dmabrk) majstate <= MS_BREAK;

                                    // PDP directly says this is DEFER cycle
                                    // (only if AND,TAD,ISZ,DCA)
                                    else if (~ o_DF_ENABLE) majstate <= MS_DEFER;

                                    // PDP said at end of last cycle that INTACK was next
                                    else if (intack) majstate <= MS_INTAK;

                                    // PDP said at end of last cycle that WC or CA was next
                                    //  if previous cycle was WC, this one is CA
                                    //  otherwise, this one is WC
                                    else if (wcca) majstate <= last_wcca ? MS_CURAD : MS_WRDCT;

                                    // if last cycle did io, or was BRK or INTACK, this one is FETCH
                                    else if (didio | last_dmabrk | last_intack) majstate <= MS_FETCH;

                                    // JMP_JMS changes only when IR is loaded, so this one is FETCH
                                    else if (last_jmpjms ^ oJMP_JMS) majstate <= MS_FETCH;

                                    // determine state based on previous state and IR
                                    else case (majstate)

                                        // last cycle was FETCH - this cycle state based on IR we got last cycle
                                        MS_FETCH: begin
                                            if (~ irknown) majstate <= nextmajst;
                                            else if (ireg[11:09] < 5) majstate <= ireg[08] ? MS_DEFER : MS_EXEC;
                                            else if (ireg[11:08] == 11) majstate <= MS_DEFER;
                                        end

                                        // last cycle was DEFER - this cycle state based on IR we got two cycles ago
                                        MS_DEFER: begin
                                            if (~ irknown) majstate <= nextmajst;
                                            else majstate <= (ireg[11:09] == 5) ? MS_FETCH : MS_EXEC;
                                        end

                                        // last cycle was EXEC - next cycle is FETCH in the absense of DMA or INTACK above
                                        MS_EXEC:  majstate <= MS_FETCH;

                                        // last cycle unknown, maybe it figured it out
                                        MS_UNKN:  majstate <= nextmajst;

                                        // first cycle after START switch, this is a FETCH cycle
                                        MS_START: majstate <= MS_FETCH;

                                        // everything else is an error (we should not lose track of state)
                                        default: begin
                                            err[03] <= 1;
                                            majstate <= nextmajst;
                                        end
                                    endcase
                                end

                                // verify major state
                                // also if FETCH, IR was loaded from MEM back at TP2, and compute EA and capture PC
                                // also convert MS_INTAK to MS_EXEC
                                9: begin
                                    if ((majstate != MS_UNKN) & (nextmajst != MS_UNKN) & (majstate != nextmajst)) err[04] <= 1;
                                    case (majstate)
                                        MS_FETCH: begin
                                            eaknown     <= 1;
                                            eadr[11:07] <= i_MEM[07] ? 5'b0 : oMA[11:07];
                                            eadr[06:00] <= ~ i_MEM[06:00];
                                            irknown     <= 1;
                                            ireg        <= ~ i_MEM;
                                            if (~ pcknown) begin
                                                pcknown <= 1;
                                                pctr <= oMA;
                                            end
                                        end
                                        MS_INTAK: begin
                                            eaknown  <= 1;
                                            eadr     <= 0;
                                            irknown  <= 1;
                                            ireg     <= 12'o4000;
                                            majstate <= MS_EXEC;
                                        end
                                    endcase

                                    // next step will try to compute what MA, MB should be
                                    maknown <= 0;
                                    mbknown <= 0;
                                    wcovkwn <= 0;
                                    wcov <= 0;
                                end

                                // compute what address and writeback should be
                                10: begin
                                    case (majstate)

                                        // address is program counter
                                        // writeback is same as what was read
                                        MS_FETCH: begin
                                            maknown <= pcknown;
                                            mbknown <= 1;
                                            madr <= pctr;
                                            mbuf <= ~ i_MEM;
                                            wcovkwn <= 1;
                                        end

                                        // address is effective address
                                        // writeback is same as what was read maybe + 1
                                        MS_DEFER: begin
                                            maknown <= eaknown;
                                            eaknown <= 1;
                                            mbknown <= 1;
                                            madr <= eadr;
                                            eadr <= autoinc;
                                            mbuf <= autoinc;
                                            wcovkwn <= 1;
                                        end

                                        // address is effective address
                                        // writeback depends on the instruction
                                        MS_EXEC: begin
                                            maknown <= eaknown;
                                            madr <= eadr;
                                            if (irknown) case (ireg[11:09])
                                                0, 1: begin
                                                    mbknown <= 1;
                                                    mbuf <= ~ i_MEM;        // AND, TAD write back what was read as is
                                                end
                                                2: begin
                                                    mbknown <= 1;
                                                    mbuf <= ~ i_MEM + 1;    // ISZ writes back what was read + 1
                                                end
                                                3: begin
                                                    mbknown <= acknown;     // DCA writes back accumulator
                                                    mbuf <= acum;
                                                end
                                                4: begin
                                                    mbknown <= pcknown;     // JMS writes back program counter
                                                    mbuf <= pctr;
                                                end
                                                default: err[05] <= 1;      // shouldn't get EXEC for anything else
                                            endcase
                                            wcovkwn <= 1;
                                        end

                                        // address is DMA address
                                        // writeback is what was read + 1
                                        MS_WRDCT: begin
                                            if (~ i3CYCLE) err[10] <= 1;
                                            maknown <= 1;
                                            mbknown <= 1;
                                            wcovkwn <= 1;
                                            madr <= iDMAADDR;
                                            mbuf <= ~ i_MEM + 1;
                                            wcov <= (i_MEM == 0);
                                        end

                                        // address is DMA address + 1
                                        // writeback is what was read maybe + 1
                                        MS_CURAD: begin
                                            if (~ i3CYCLE) err[11] <= 1;
                                            maknown <= 1;
                                            mbknown <= 1;
                                            madr <= iDMAADDR + 1;
                                            eadr <= curaddr;
                                            mbuf <= curaddr;
                                            wcovkwn <= 1;
                                        end

                                        // address is from CA or DMA address
                                        // writeback is what was read maybe + 1
                                        MS_BREAK: begin
                                            maknown <= 1;
                                            mbknown <= 1;
                                            wcovkwn <= 1;
                                            madr <= i3CYCLE ? eadr : iDMAADDR;
                                            mbuf <= brkdata + (iMEMINCR ? 1 : 0);
                                            wcov <= iMEMINCR & (brkdata == 12'o7777);
                                        end
                                    endcase
                                end

                                // verify address and writeback coming from PDP/sim
                                11: begin
                                    if (maknown & (madr != oMA))  err[01] <= 1;
                                    if (mbknown & (mbuf != oBMB)) err[06] <= 1;
                                    if (wcovkwn & (wcov == o_BWC_OVERFLOW)) err[12] <= 1;
                                end
                            endcase

                            timedelay <= timedelayinc;
                        end else begin
                            timedelay <= 0;
                            timestate <= TS_4;

                            // end of TS3, update what AC, LINK and PC should be at this point
                            case (majstate)

                                // FETCH:
                                //  direct JMP: sets PC = effective address
                                //   (indirect JMP sets PC in DEFER cycle)
                                //  IOT: updates to AC, PC are done as part of TS4
                                //  OPR: update AC, LINK, PC from opcode
                                MS_FETCH: begin
                                    if (irknown) case (ireg[11:09])
                                        5: begin
                                            if (~ ireg[08]) begin
                                                pcknown <= eaknown;
                                                pctr <= eadr;
                                            end
                                        end
                                        7: begin
                                            if (~ ireg[08]) begin
                                                acknown <= g1acknown;
                                                lnknown <= g1lnknown;
                                                acum    <= g1acum;
                                                link    <= g1link;
                                                pctr    <= pctr + 1;
                                            end else if (~ ireg[00]) begin
                                                acknown <= g2acknown;
                                                pcknown <= g2pcknown;
                                                acum    <= g2acum;
                                                pctr    <= g2pctr;
                                            end else begin
                                                pctr    <= pctr + 1;
                                            end
                                        end
                                        default: pctr <= pctr + 1;
                                    endcase
                                end

                                MS_DEFER: begin
                                    if (irknown && (ireg[11:09] == 5)) begin
                                        pcknown <= 1;
                                        pctr <= mbuf;
                                    end
                                end

                                // EXEC:
                                //  AND,TAD,DCA: update according to opcode
                                //  ISZ: maybe increment PC
                                //  JMS: PC = EA + 1
                                MS_EXEC: begin
                                    if (irknown) case (ireg[11:09])
                                        0: begin
                                            acum <= acum & ~ i_MEM;
                                        end
                                        1: begin
                                            { link, acum } <= { link, acum } + { 1'b0, ~ i_MEM };
                                        end
                                        2: begin
                                            if (oBMB == 0) pctr <= pctr + 1;
                                        end
                                        3: begin
                                            acknown <= 1;
                                            acum <= 0;
                                        end
                                        4: begin
                                            pcknown <= eaknown;
                                            pctr <= eadr + 1;
                                        end
                                    endcase
                                end
                            endcase
                        end
                    end

                    // majstate = this cycle major state

                    TS_4: begin
                        if (oBTS_3) err[07] <= 1;
                        if (~ oBTS_1) begin

                            // AC gets loaded at end of TS3
                            // at 80nS into TP4, we should be able to capture
                            if (timedelay == 8) begin
                                if (~ acknown) begin
                                    acum <= oBAC;
                                    acknown <= 1;
                                end
                            end

                            // maybe not really TS4 but we are doing an io instruction
                            // it means this is a fetch cycle
                            // update AC at end of each io pulse and maybe inc PC
                            if (oBIOP1 | oBIOP2 | oBIOP4) begin
                                if ((i_MEM[11:09] != 1) | (i_MEM[02] & oBIOP4) | (i_MEM[01] & oBIOP2) | (i_MEM[00] & oBIOP1)) begin
                                    err[08] <= 1;
                                end else if (majstate == MS_UNKN) begin
                                    majstate <= MS_FETCH;
                                    irknown  <= 1;
                                    ireg     <= ~ i_MEM;
                                end

                                didio    <= 1;
                                lastbiop  <= 1;
                            end else if (lastbiop) begin
                                if (iAC_CLEAR) acknown <= 1;
                                acum <= (iAC_CLEAR ? 0 : acum) | iINPUTBUS;
                                if (iIO_SKIP) pctr <= pctr + 1;
                                lastbiop <= 0;
                            end

                            breaksr   <= { breaksr[6:0], ~ o_BF_ENABLE };
                            intacksr  <= { intacksr[6:0], ~ o_LOAD_SF };
                            wccasr    <= { wccasr[6:0], ~ o_SP_CYC_NEXT };
                            nextmajst <= MS_UNKN;

                            // RUN FF gets clocked by TP3 so assume it is valid by 150nS into TS4
                            // if clear (halted), assume state is unknown cuz we don't know what user is doing
                            if (timedelay != timedelayinc) begin
                                timedelay <= timedelayinc;
                            end else if (~ oB_RUN) begin
                                timestate <= TS_U;
                                majstate  <= MS_UNKN;
                                nextmajst <= MS_UNKN;
                            end
                        end else begin

                            // definitely ended TS4, compare AC now that all io pulses are done
                            if (acknown & (oBAC != acum)) err[09] <= 1;

                            last_dmabrk <= dmabrk;          // this was a BREAK cycle
                            last_intack <= intack;          // this was an interrupt acknowledge cycle
                            last_jmpjms <= oJMP_JMS;        // IR contains a JMP or JMS instruction
                            last_wcca   <= wcca;            // this was a WC or CA cycle

                            intack <= intacksr[7];          // next cycle is interrupt acknowledge
                            wcca   <= wccasr[7];            // next cycle is WC or CA

                            // try to come up with what next major state should be
                            case (majstate)
                                MS_FETCH: if (irknown) case (ireg[11:09])
                                    0,1,2,3,4: nextmajst <= ireg[08] ? MS_DEFER : MS_EXEC;
                                    5:         nextmajst <= ireg[08] ? MS_DEFER : nextfetch;
                                    6,7:       nextmajst <= nextfetch;
                                endcase
                                MS_DEFER: if (irknown) nextmajst <= (ireg[11:09] == 5) ? nextfetch : MS_EXEC;
                                MS_EXEC:  nextmajst <= nextfetch;
                                MS_WRDCT: nextmajst <= MS_CURAD;
                                MS_CURAD: nextmajst <= MS_BREAK;
                                MS_BREAK: nextmajst <= nextfetch;
                            endcase

                            timedelay <= 0;
                            timestate <= TS_1;
                        end
                    end
                endcase
            end

            lastts1 <= oBTS_1;
            lastts3 <= oBTS_3;
        end
    end

endmodule
