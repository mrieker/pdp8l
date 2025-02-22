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

// PDP-8/L pulse bit generator

// arm registers:
//  [0] = ident='PB',sizecode=0,version
//  [1] = iszac 00000 width count

module pdp8lpbit (
    input CLOCK, CSTEP, RESET,

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

    output reg pulse
);

    reg iszac;
    reg [12:00] count, width;

    assign armrdata = ~ armraddr ? 32'h50420003 : // [31:16] = 'PB'; [15:12] = (log2 nreg) - 1; [11:00] = version
                      { iszac, 5'b0, width, count };

    always @(posedge CLOCK) begin
        if (RESET) begin
            iszac <= 0;         // don't do ISZ AC
            width <= 599;       // 6.00uS pulse width
            count <= 0;
            pulse <= 0;
        end

        // arm processor is writing the register
        else if (armwrite) begin
            if (armwaddr) begin
                iszac <= armwdata[31];
                width <= armwdata[25:13];
                count <= 0;
                pulse <= 0;
            end
        end else if (CSTEP) begin
            if (iopstart) begin

                // any I/O instruction, start pulse going
                count <= width;
                pulse <= 1;

                // maybe do increment accumulator and skip if zero
                if (iszac & (ioopcode == 12'o6004)) begin
                    AC_CLEAR <= 1;
                    { IO_SKIP, devtocpu } <= { 1'b0, cputodev } + 13'o00001;
                end
            end else begin
                if (iopstop) begin
                    devtocpu <= 0;
                    AC_CLEAR <= 0;
                    IO_SKIP  <= 0;
                end
                if (count != 0) begin
                    count <= count - 1;
                end else begin
                    pulse <= 0;
                end
            end
        end
    end
endmodule
