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

// PDP-8/L TC08 interface

module pdp8ltc08 (
    input CLOCK, CSTEP, RESET, BINIT,

    input armwrite,
    input armraddr, armwaddr,
    input[31:00] armwdata,
    output[31:00] armrdata,

    input iopstart,
    input iopstop,
    input[11:00] ioopcode,
    input[11:00] cputodev,

    output reg[11:00] devtocpu,
    output reg AC_CLEAR,
    output reg IO_SKIP,
    output INT_RQST
);

    reg[11:00] status_a, status_b;
    reg enable, iopend;

    assign armrdata = (armraddr == 0) ? 32'h54430002 : // [31:16] = 'TC'; [15:12] = (log2 nreg) - 1; [11:00] = version
                      { enable, 3'b0, status_b, iopend, 3'b0, status_a };

    assign INT_RQST = (status_b[11] | status_b[00]) & status_a[02]; // request interrupt if error or success

    wire[11:00] new_status_a = (ioopcode[01] ? 12'o0000 : status_a) ^ (cputodev & 12'o7774);

    always @(posedge CLOCK) begin
        if (BINIT) begin
            if (RESET) begin
                enable <= 0;
            end
            iopend   <= 0;
            status_a <= 0;
            status_b <= 0;
        end

        // arm processor is writing one of our registers
        else if (armwrite) begin
            case (armwaddr)
                1: begin
                    enable   <= armwdata[31];
                    status_b <= armwdata[27:16];
                    iopend   <= armwdata[15];
                    status_a <= armwdata[11:00];
                end
            endcase
        end

        // process the IOP only on the leading edge
        // ...but leave output signals to PDP-8/L in place until given the all clear
        else if (CSTEP) begin
            if (iopstart & enable) begin

                // 676x opcodes
                if (ioopcode[11:03] == 9'o676) begin
                    if (ioopcode[02]) begin
                        status_a <= new_status_a;                   // maybe clear status_a, maybe start IO
                        if (~ cputodev[00]) status_b[00] <= 0;      // clear dectape flag
                        if (~ cputodev[01]) status_b[11:08] <= 0;   // clear error bits
                        if (new_status_a[07]) iopend <= 1;          // wake arm up to process request if GO bit set
                        AC_CLEAR <= 1;                              // clear accumulator
                    end else if (ioopcode[01]) begin
                        status_a <= 0;                              // just clearing status_a, not starting IO
                    end
                    if (ioopcode[00]) begin
                        devtocpu <= status_a;                       // read status_a
                    end
                end

                // 677x opcodes
                if (ioopcode[11:03] == 9'o677) begin
                    if (ioopcode[02]) begin
                        status_b[05:03] <= cputodev[05:03];         // load DMA extended address bits of status register B
                        AC_CLEAR <= 1;                              // clear accumulator
                    end
                    if (ioopcode[01]) begin
                        devtocpu <= status_b;                       // read status register B
                    end
                    if (ioopcode[00]) begin
                        IO_SKIP <= status_b[11] | status_b[00];     // skip if error or success
                    end
                end
            end

            // processor no longer sending an IOP, stop sending data over the bus so it doesn't jam other devices
            else if (iopstop) begin
                AC_CLEAR <= 0;
                devtocpu <= 0;
                IO_SKIP  <= 0;
            end
        end
    end
endmodule
