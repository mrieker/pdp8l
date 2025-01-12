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

// PDP-8/L front panel interface

module pdp8lfpi2c (
    input CLOCK, RESET,

    input armwrite,
    input[2:0] armraddr, armwaddr,
    input[31:00] armwdata,
    output[31:00] armrdata,

    output i2cclk,
    output i2cdao,
    input  i2cdai,

    output[13:00] i2ccount
);

    reg clear, manclk, mandao, manual, stepon;
    reg[31:00] commandlo;
    wire autclk, autdao;
    wire[63:00] comand, status;

    assign armrdata = (armraddr == 0) ? 32'h46502008 : // [31:16] = 'FP'; [15:12] = (log2 nreg) - 1; [11:00] = version
                      (armraddr == 1) ? { comand[31:00] } :
                      (armraddr == 2) ? { comand[63:32] } :
                      (armraddr == 3) ? { status[31:00] } :
                      (armraddr == 4) ? { status[63:32] } :
                      (armraddr == 5) ? { i2cclk, i2cdao, i2cdai, 11'b0, i2ccount, manual, clear, stepon, 1'b0 } :
                       32'hDEADBEEF;

    assign i2cclk = manual ? manclk : autclk;
    assign i2cdao = manual ? mandao : autdao;

    i2cmaster fpi2cintf (
        .CLOCK (CLOCK),
        .RESET (RESET | clear),
        .CSTEP (~ stepon | (armwrite & (armwaddr == 5) & armwdata[0])),
        .wrcmd (armwrite & (armwaddr == 2)),    // trigger when writing hi-order command
        .command ({ armwdata, commandlo }),
        .comand (comand),
        .status (status),
        .sclo (autclk),
        .sdao (autdao),
        .sdai (i2cdai),
        .count (i2ccount));

    always @(posedge CLOCK) begin
        if (RESET) begin
            clear  <= 0;
            manual <= 0;
            stepon <= 0;
        end else if (armwrite) begin
            case (armwaddr)
                1: commandlo <= armwdata;       // save lo-order command word
                5: begin
                    stepon <= armwdata[01];     // 0=clock i2cmaster normally; 1=clock i2cmaster with armwdata[00]
                    clear  <= armwdata[02];     // reset i2cmaster module
                    manual <= armwdata[03];     // 0=use i2cmaster module for cli2ck,i2cdao; 1=use manclk,mandao bits
                    manclk <= armwdata[31];     // if manual mode, use this for i2cclk
                    mandao <= armwdata[30];     // if manual mode, use this for i2cdao
                end
            endcase
        end
    end
endmodule
