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
// If xmxm enlo4K is clear, this circuit will access the PDP-8/L 4K core memory
// Can also access upper 28K (of the 32K block memory) via dma for testing

// PDP-8/L Users Handbook Chap 5 p38/39 discuss the DMA cycle
//  address, datain, threecycle, breakrequest accepted all at same time, but we delay breakrequest 20nS
//  read data available at time of strobe
//  write data must be available before TS3 begins
//  addr_accept asserted at TP4 time and lasts 350..450nS

// arm registers:
//  [0] = ident='CM',sizecode=1,version
//  [1] = <enable> 0 <rddone> <busy> <12-bit-data> <write> <15-bit-addr>
//    (rw) enable = perform memory cycle
//    (ro) rddone = read data is available (sets while busy still set)
//    (ro) busy = busy performing memory cycle (do not modify register)
//    (rw) data = write: data to be written to memory via dma cycle
//                 read: data read from memory (write=0) or written to memory (write=1)
//    (rw) write = write given data to memory
//    (rw) addr = address of memory word to access

module pdp8lcmem (
    input CLOCK, RESET,

    input armwrite,
    input[1:0] armraddr, armwaddr,
    input[31:00] armwdata,
    output[31:00] armrdata,

    output reg brkrqst,
    output brkwrite,
    output[2:0] brkema,
    output[11:00] brkaddr,
    output[11:00] brkwdat,
    input[11:00] brkrdat,
    input brkts3,
    input _brkdone
);

    reg ctlwrite, ctlrdone;
    reg[14:00] ctladdr;
    reg[11:00] ctldata;
    reg[2:0] busyonarm;
    reg ctlenab;
    wire ctlbusy = busyonarm != 0;

    assign armrdata = (armraddr == 0) ? 32'h434D1006 :  // [31:16] = 'CM'; [15:12] = (log2 nreg) - 1; [11:00] = version
                      (armraddr == 1) ? { ctlenab, 1'b0, ctlrdone, ctlbusy, ctldata, ctlwrite, ctladdr } :
                      (armraddr == 2) ? { 27'b0, brkts3, _brkdone, busyonarm } :
                      32'hDEADBEEF;

    assign brkema   = ctladdr[14:12];
    assign brkaddr  = ctladdr[11:00];
    assign brkwrite = ctlwrite;
    assign brkwdat  = ctldata;

    always @(posedge CLOCK) begin
        if (RESET) begin
            brkrqst   <= 0;
            busyonarm <= 0;
            ctlenab   <= 0;
        end

        // arm processor is writing one of the registers
        else if (armwrite) begin
            case (armwaddr)
                1: begin
                    if (~ ctlbusy) begin
                        ctladdr   <= armwdata[14:00];
                        ctlwrite  <= armwdata[15];
                        ctldata   <= armwdata[27:16];
                        ctlrdone  <= 0;
                        ctlenab   <= armwdata[31];
                        busyonarm <= { 2'b00, armwdata[31] };
                    end
                end
            endcase
        end

        // maybe we have dma cycle to process
        else case (busyonarm)

            // delay asserting brkrqst for 20nS after outputting other control lines
            1: begin
                busyonarm <= 2;
            end
            2: begin
                brkrqst   <= 1;
                busyonarm <= 3;
            end

            // when TS3 for break state starts, we can drop brkrqst
            // if this is a read request, MB contains the data
            3: begin
                if (brkts3) begin
                    brkrqst <= 0;
                    if (~ ctlwrite) begin
                        ctldata  <= brkrdat;
                        ctlrdone <= 1;
                    end
                    busyonarm <= 4;
                end
            end

            // wait for ADDR_ACCEPTED asserted (occurs at TP4)
            4: begin
                if (~ _brkdone) begin
                    busyonarm <= 5;
                end
            end

            // wait for ADDR_ACCEPTED negated before accepting another request from arm
            5: begin
                if (_brkdone) begin
                    busyonarm <= 0;
                end
            end
        endcase
    end
endmodule
