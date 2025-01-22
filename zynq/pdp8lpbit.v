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
//  [1] = <width> <mask> <code>

module pdp8lpbit (
    input CLOCK, CSTEP, RESET,

    input armwrite,
    input armraddr, armwaddr,
    input[31:00] armwdata,
    output[31:00] armrdata,

    input iopstart,
    input[11:00] ioopcode,

    output reg pulse
);

    reg [8:0] mask, code;
    reg [14:00] count, width;

    assign armrdata = ~ armraddr ? 32'h50420001 : // [31:16] = 'PB'; [15:12] = (log2 nreg) - 1; [11:00] = version
                      { width, mask, code };

    always @(posedge CLOCK) begin
        if (RESET) begin
            width <= 599;       // 6.00uS pulse width
            mask  <= 9'o777;    // opcode 6002
            code  <= 9'o002;
            count <= 0;
            pulse <= 0;
        end

        // arm processor is writing the register
        else if (armwrite) begin
            if (armwaddr) begin
                width <= armwdata[31:18];
                mask  <= armwdata[17:09];
                code  <= armwdata[08:00];
                count <= 0;
                pulse <= 0;
            end
        end else if (CSTEP) begin
            if (iopstart & (ioopcode[11:09] == 3'o6) & ((ioopcode[08:00] & mask) == code)) begin
                count <= width;
                pulse <= 1;
            end else if (count != 0) begin
                count <= count - 1;
            end else begin
                pulse <= 0;
            end
        end
    end
endmodule
