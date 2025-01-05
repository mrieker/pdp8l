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

// PDP-8/L paper tape reader interface

// arm registers:
//  [0] = ident='PR',sizecode=0,version
//  [1] = <rdflag> <enable> 000000000000000000 <rdchar>

module pdp8lptr (
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

    reg enable, intenab, rdflag, rdstep;
    reg[11:00] rdchar;

    assign armrdata = ~ armraddr ? 32'h50520001 : // [31:16] = 'PR'; [15:12] = (log2 nreg) - 1; [11:00] = version
                      { rdflag, enable, rdstep, 17'b0, rdchar };

    assign INT_RQST = intenab & rdflag;

    always @(posedge CLOCK) begin
        if (BINIT) begin
            if (RESET) begin
                enable <= 0;
            end
            intenab <= 1;
            rdflag  <= 0;
            rdstep  <= 0;
        end

        // arm processor is writing one of the registers
        else if (armwrite) begin
            case (armwaddr)

                // arm processor is sending a reader char for the PDP-8/L code to read
                // - typically sets rdflag = 1 indicating rdchar has something in it
                1: begin
                    rdflag <= armwdata[31];
                    enable <= armwdata[30];
                    rdstep <= armwdata[29];
                    rdchar <= armwdata[11:00];
                end
            endcase
        end else if (CSTEP) begin

            // process the IOP only on the leading edge
            // ...but leave output signals to PDP-8/L in place until given the all clear
            if (iopstart & enable) begin
                case (ioopcode)
                    12'o6011: begin IO_SKIP <= rdflag; end                              // skip if reader char ready
                    12'o6012: begin devtocpu <= rdchar; rdflag <= 0; end                // read char, clear flag
                    12'o6014: begin rdflag <= 0; rdstep <= 1; end                       // clear flag, step reader
                    12'o6016: begin devtocpu <= rdchar; rdflag <= 0; rdstep <= 1; end   // read char, clear flag, step reader
                endcase
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
