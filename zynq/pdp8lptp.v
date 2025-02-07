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

// PDP-8/L paper tape punch interface
// small computer handbook 1972 p 7-41..43 (261..263)

// arm registers:
//  [0] = ident='PP',sizecode=0,version
//  [1] = <wrflag> <enable> <wrbusy> 00000000000000000 <wrchar>

module pdp8lptp (
    input CLOCK, CSTEP, RESET, BINIT,

    input armwrite,
    input armraddr, armwaddr,
    input[31:00] armwdata,
    output[31:00] armrdata,

    input iopstart,
    input iopstop,
    input[11:00] ioopcode,
    input[11:00] cputodev,

    output reg IO_SKIP,
    output INT_RQST
);

    reg enable, wrbusy, wrflag;
    reg[11:00] wrchar;

    assign armrdata = ~ armraddr ? 32'h50500001 : // [31:16] = 'PP'; [15:12] = (log2 nreg) - 1; [11:00] = version
                      { wrflag, enable, wrbusy, 17'b0, wrchar };

    assign INT_RQST = wrflag;

    always @(posedge CLOCK) begin
        if (BINIT) begin
            if (RESET) begin
                enable <= 0;
            end
            wrflag <= 0;
        end

        // arm processor is writing one of the registers
        else if (armwrite) begin
            case (armwaddr)
                1: begin
                    wrflag <= armwdata[31];
                    enable <= armwdata[30];
                    wrbusy <= armwdata[29];
                end
            endcase
        end else if (CSTEP) begin

            // process the IOP only on the leading edge
            // ...but leave output signals to PDP-8/L in place until given the all clear
            if (iopstart & enable & (ioopcode[11:03] == 9'o602)) begin
                if (ioopcode[00]) IO_SKIP <= wrflag;
                if (ioopcode[01]) wrflag  <= 0;
                if (ioopcode[02]) begin wrchar <= cputodev; wrbusy <= 1; end
            end

            // processor no longer sending an IOP, stop sending data over the bus so it doesn't jam other devices
            else if (iopstop) begin
                IO_SKIP <= 0;
            end
        end
    end
endmodule
