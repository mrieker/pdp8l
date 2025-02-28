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

// Provide arm processor access to PDP-8/L core memory via dma requests
// If xmem enlo4K is set, this circuit will access the low 4K of the 32K block memory
// If xmem enlo4K is clear, this circuit will access the PDP-8/L 4K core memory
// Can also access upper 28K (of the 32K block memory) via dma for testing

// PDP-8/L Users Handbook Chap 5 p38/39 discuss the DMA cycle
//  address, datain, threecycle, breakrequest accepted all at same time, but we delay breakrequest 30nS
//  read data available at time of strobe (end of TS1)
//  write data must be available before TS3 begins
//  addr_accept asserted at TP4 time and lasts 350..450nS

// arm registers:
//  [0] = ident='CM',sizecode=1,version
//  [1] = <enable> <wcovf> <rddone> <busy> <12-bit-data> <write> <15-bit-addr>
//    (rw) enable = perform memory cycle
//    (ro) wcovf  = wordcount overflow for 3-cycle
//    (ro) rddone = read data is available (sets while busy still set)
//    (ro) busy   = busy performing memory cycle (do not modify register)
//    (rw) data   = write: data to be written to memory via dma cycle
//                  read: data read from memory
//    (rw) write  = write given data to memory
//    (rw) addr   = address of memory word to access
//  [3] = 32-bit test-and-set cell
//        always accepts writing anything when currently zero
//        otherwise writing its current value back clears it to zero

module pdp8lcmem (
    input CLOCK, CSTEP, RESET,

    input armwrite,
    input[1:0] armraddr, armwaddr,
    input[31:00] armwdata,
    output[31:00] armrdata,

    output reg brkrqst,
    output brkwrite,
    output reg brk3cycl,
    output reg brkcainc,
    output[2:0] brkema,
    output[11:00] brkaddr,
    output[11:00] brkwdat,
    input[11:00] brkrdat,
    input brkcycle,
    input brkts1,
    input brkts3,
    input brkwcovf,

    output reg xmemblock,
    input  xmemidle,

    output reg[14:00] xbraddr,
    output reg[11:00] xbrwdat,
    output reg xbrenab, xbrwena,
    input[11:00] xbrrdat

    ,output reg nobrkopt
    ,output reg[4:0] busyonarm
);

    reg ctlwrite, ctldone;
    reg[14:00] ctladdr;
    reg[11:00] ctldata;
    reg[3:0] ts3count;
    reg[31:00] armlock;
    reg ctlwcovf, last1wcovf, last2wcovf;
    wire ctlbusy = busyonarm != 0;

    assign armrdata = (armraddr == 0) ? 32'h434D1018 :  // [31:16] = 'CM'; [15:12] = (log2 nreg) - 1; [11:00] = version
                      (armraddr == 1) ? { 1'b0, ctlwcovf, ctldone, ctlbusy, ctldata, ctlwrite, ctladdr } :
                      (armraddr == 2) ? { brk3cycl, brkcainc, nobrkopt, 21'b0, brkcycle, xmemblock, xmemidle, busyonarm } :
                      armlock;

    wire[11:00] xbrrdatplus1 = xbrrdat + 1;
    wire[4:0] busyoninc = busyonarm + 1;

    assign brkema   = ctladdr[14:12];
    assign brkwrite = ctlwrite;

    assign brkaddr  = ctladdr[11:00];   // sent to processor during TS4 and 1
                                        // processor captures it at end of TS4
    assign brkwdat  = ctldata;          // sent to processor during TS2 and 3
                                        // processor captures it at end of TS2

    always @(posedge CLOCK) begin
        if (RESET) begin
            armlock   <= 0;
            brkrqst   <= 0;
            busyonarm <= 0;
            nobrkopt  <= 0;
            xbraddr   <= 0;
            xbrwdat   <= 0;
            xbrenab   <= 0;
            xbrwena   <= 0;
            xmemblock <= 0;
        end

        // arm processor is writing one of the registers
        else if (armwrite) begin
            case (armwaddr)
                1: begin
                    if (~ ctlbusy) begin
                        ctladdr   <= armwdata[14:00];
                        ctlwrite  <= armwdata[15];
                        ctldata   <= armwdata[27:16];
                        ctldone   <= 0;
                        ctlwcovf  <= 0;
                        busyonarm <= { 3'b000, armwdata[31] };
                        xmemblock <= armwdata[31] & nobrkopt;
                    end
                end

                2: begin
                    if (~ ctlbusy) begin
                        brk3cycl <= armwdata[31];
                        brkcainc <= armwdata[30];
                        nobrkopt <= armwdata[29];
                    end
                end

                // test-and-set cell for arm processor use
                //  anything can be written when it is currently zero
                //  otherwise writing the same number back clears it to zero
                //  all other writes are ignored
                3: begin
                    if (armlock == 0) begin
                        armlock <= armwdata;
                    end else if (armlock == armwdata) begin
                        armlock <= 0;
                    end
                end
            endcase
        end

        // maybe we have dma cycle to process
        else if (CSTEP) begin

            if (nobrkopt) begin

                // the PDP does not have the BREAK option installed
                // so do the access directly in the extended memory
                // xmem enlo4k is assumed to be set so low 4K accesses will work

                case (busyonarm)
                     0: begin end

                    // wait for xmem to go idle
                     1: begin
                        if (xmemidle) busyonarm <= brk3cycl ? 2 : 18;
                    end

                    // read the wordcount value
                     2: begin
                        busyonarm <= busyoninc;
                        xbraddr   <= { 3'b0, ctladdr[11:00] };
                        xbrenab   <= 1;
                        xbrwena   <= 0;
                    end

                    // writeback an incremented value and detect overflow
                     6: begin
                        busyonarm <= busyoninc;
                        xbrwdat   <= xbrrdatplus1;
                        xbrwena   <= 1;
                        ctlwcovf  <= xbrrdat == 12'o7777;
                    end

                     9: begin
                        busyonarm <= busyoninc;
                        xbrwena   <= 0;
                    end

                    // read current address
                    10: begin
                        busyonarm <= busyoninc;
                        xbraddr[11:00] <= ctladdr[11:00] + 1;
                        xbrwdat   <= 0;
                    end

                    // update address
                    // maybe writeback incremented address
                    14: begin
                        if (brkcainc) begin
                            busyonarm <= busyoninc;
                            ctladdr[11:00] <= xbrrdatplus1;
                            xbrwdat   <= xbrrdatplus1;
                            xbrwena   <= 1;
                        end else begin
                            busyonarm <= 18;
                            ctladdr[11:00] <= xbrrdat;
                        end
                    end

                    17: begin
                        busyonarm <= busyoninc;
                        xbrwena   <= 0;
                    end

                    // perform dma transfer (read or write)
                    18: begin
                        busyonarm <= busyoninc;
                        xbraddr   <= ctladdr;
                        xbrenab   <= 1;
                        xbrwdat   <= ctldata;
                        xbrwena   <= ctlwrite;
                    end

                    21: begin
                        busyonarm <= busyoninc;
                        xbrwena   <= 0;
                    end

                    // all done
                    22: begin
                        busyonarm <= 0;
                        ctldone   <= 1;
                        ctldata   <= xbrrdat;
                        xbraddr   <= 0;
                        xbrenab   <= 0;
                        xbrwdat   <= 0;
                        xbrwena   <= 0;
                        xmemblock <= 0;
                    end

                    default: busyonarm <= busyoninc;
                endcase
            end else begin

                case (busyonarm)
                    0: begin end

                    // wait for TS3 to send brkrqst
                    // (we might already be in middle of a TS3)
                    // sending it during TS1 gets us glitchy B_BREAK
                    // this way we know it will be a while before TS1 starts
                    1: begin
                        if (brkts3) begin
                            brkrqst   <= 1;
                            busyonarm <= 2;
                        end
                    end

                    // wait for TS1 with B_BREAK indicating this cycle is for us
                    // B_BREAK is the output of the BREAK state flipflop, so is this actual cycle
                    // if this is a write request, flag it done because ctlwcovf is valid now
                    // last2wcovf has wc overflow from 2 cycles ago, ie, WC
                    2: begin
                        if (brkcycle & brkts1) begin
                            brkrqst   <= 0;
                            busyonarm <= 3;
                            ctldone   <= ctlwrite;
                            ctlwcovf  <= last2wcovf;
                        end
                    end

                    // wait for TS3 to start (whilst in BREAK state)
                    3: begin
                        if (brkts3) begin
                            busyonarm <= 4;
                        end
                    end

                    // wait for TS3 negated
                    // read data becomes available somewhere in middle of TS3
                    // ...and remains until beginning of next TS3
                    // so clock read data at end of TS3
                    4: begin
                        if (~ brkts3) begin
                            if (~ brkwrite) begin
                                ctldata <= brkrdat;
                                ctldone <= 1;
                            end
                            busyonarm <= 0;
                        end
                    end
                endcase

                // BWC_OVERFLOW is triggered by TP2, ie, near beginning of TS3
                // it is cleared by MEM_DONE, ie, near end of TS3 (see vol 2, p9 D-5)
                // it is set only during WC TP2 if count increments to zero
                // also during BRK TP2 if MEMINCR and increments to zero
                // we sample it 150nS into TS3 (TS3 is over 200nS wide)
                // set up a shift register chain:
                //   last2wcovf <= last1wcovf <= brkwcovf
                if (~ brkts3) ts3count <= 0;
                else if (ts3count != 15) ts3count <= ts3count + 1;
                if (ts3count == 14) begin
                    last2wcovf <= last1wcovf;
                    last1wcovf <= brkwcovf;
                end
            end
        end
    end
endmodule
