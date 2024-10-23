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

// PDP-8/L teletype interface

module pdp8ltty (
    input CLOCK, RESET,
    input armwpulse,
    input[1:0] armraddr, armwaddr,
    input[31:00] armwdata,
    output[31:00] armrdata,

    output reg[11:00] INPUTBUS,
    output reg AC_CLEAR,
    output INT_RQST,
    output reg IO_SKIP,

    input[11:00] BAC,
    input BIOP1,
    input BIOP2,
    input BIOP4,
    input[11:00] BMB,
    input BUSINIT
);

    reg intenab, kbflag, lastiop, prflag, prfull;
    reg[11:00] kbchar, prchar;

    assign armrdata = 32'h87654321;/** (armraddr == 0) ? 32'h54541001 : // [31:16] = 'TT'; [15:12] = (log2 nreg) - 1; [11:00] = version
                      (armraddr == 1) ? { kbflag, 19'b0, kbchar } :
                      (armraddr == 2) ? { prflag, prfull, 18'b0, prchar } :
                      32'hDEADBEEF;**/

    assign INT_RQST = intenab & (kbflag | prflag);

    always @(posedge CLOCK) begin
        if (RESET | BUSINIT) begin
            intenab <= 0;
            kbflag  <= 0;
            lastiop <= 0;
            prflag  <= 0;
            prfull  <= 0;
        end

        // arm processor doing a write takes precedence over looking for IO pulse
        // ...because armwpulse only lasts one cycle
        else if (armwpulse) begin
            case (armwaddr)

                // arm processor is sending a keyboard char for the PDP-8/L code to read
                // - typically sets kbflag = 1 indicating kbchar has something in it
                1: begin kbflag <= armwdata[31]; kbchar <= armwdata[07:00]; end

                // arm processor is telling PDP-8/L that it has finished printing char
                // - typically sets prflag = 1 meaning it has finished printing char
                //         and sets prfull = 0 indicating prchar is empty
                2: begin prflag <= armwdata[31]; prfull <= armwdata[30]; end
            endcase
        end else begin

            // see if PDP-8/L is outputting one of the IOPs
            // make sure we process only one IOP per IO instruction
            if ((BIOP1 & (BMB[00] == 1)) | (BIOP2 & (BMB[01:00] == 2)) | (BIOP4 & (BMB[02:00] == 4))) begin

                // process the IOP only on the leading edge
                // ...but leave output signals to PDP-8/L in place until IOP negates
                if (~ lastiop) begin
                    case (BMB)
                        12'o6031: begin IO_SKIP <= kbflag; end                                  // skip if kb char ready
                        12'o6032: begin AC_CLEAR <= 1; kbflag <= 0; end                         // clear accum, clear flag
                        12'o6034: begin INPUTBUS <= { 4'b0, kbchar }; end                       // read kb char
                        12'o6035: begin intenab <= BAC[00]; end                                 // enable/disable interrupts
                        12'o6036: begin AC_CLEAR <= 1; INPUTBUS <= kbchar; kbflag <= 0; end     // read kb char, clear flag
                        12'o6041: begin IO_SKIP <= prflag; end                                  // skip if done printing
                        12'o6042: begin prflag <= 0; end                                        // stop saying done printing
                        12'o6044: begin prchar <= BAC; prfull <= 1; end                         // start printing char
                        12'o6045: begin IO_SKIP <= INT_RQST; end                                // skip if requesting interrupt
                        12'o6046: begin prchar <= BAC[07:00]; prflag <= 0; prfull <= 1; end     // start printing char, clear done flag
                    endcase
                end
            end

            // processor no longer sending an IOP, stop sending data over the bus so it doesn't jam other devices
            else begin
                AC_CLEAR <= 0;
                INPUTBUS <= 0;
                IO_SKIP  <= 0;
            end

            // used to detect IOP transition
            lastiop <= BIOP1 | BIOP2 | BIOP4;
        end
    end
endmodule
