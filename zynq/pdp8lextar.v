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

// PDP-8/L extended arithmetic

// arm registers:
//  [0] = ident='EA',sizecode=1,version
//  [1] = 0000000000000 gtflag modeb stepcount multquot

module pdp8lextar (
    input CLOCK, CSTEP, RESET, BINIT,

    input armwrite,
    input[1:0] armraddr, armwaddr,
    input[31:00] armwdata,
    output[31:00] armrdata,

    input iopstart,
    input iopstop,
    input[11:00] ioopcode,
    input[11:00] cputodev,

    output reg[11:00] devtocpu,
    output reg ioskip,
    output reg acclear
);

    reg gtflag, modeb;
    reg[11:00] multquot;
    reg[4:0] stepcount;

    assign armrdata = (armraddr == 0) ? 32'h45411004 :  // [31:16] = 'EA'; [15:12] = (log2 nreg) - 1; [11:00] = version
                      (armraddr == 1) ? { 13'b0, gtflag, modeb, stepcount, multquot } :
                      32'hDEADBEEF;

    always @(posedge CLOCK) begin
        if (BINIT) begin
            if (RESET) begin
                // these get cleared on power up
                // they remain as is when start switch is pressed
            end
            // these get cleared on power up or start switch
            stepcount <= 0;
            multquot  <= 0;
            gtflag    <= 0;
            modeb     <= 0;
        end

        // arm processor is writing one of the registers
        // armwrite lasts only 1 fpga clock cycle
        else if (armwrite) begin
        end else if (CSTEP) begin

            if (iopstart & (ioopcode[11:08] == 4'b1111) & ioopcode[00]) begin

                // 0200 CLA - clear accumulator
                acclear <= ioopcode[07];

                // 7501: p137 / v3-11 - or multiplier quotient into accumulator
                devtocpu <= ioopcode[06] ? multquot : 0;

                // 7421: p137 / v3-11 - load moltiplier quotient from accumulator
                if (ioopcode[04]) multquot <= cputodev;

            end else if (iopstop) begin
                devtocpu <= 0;
                ioskip   <= 0;
                acclear  <= 0;
            end
        end
    end
endmodule
