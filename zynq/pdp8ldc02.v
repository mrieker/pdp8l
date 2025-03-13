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

// PDP-8/L DC02 terminal multiplexor board, supports up to 6 terminals
//  * incomplete implementation
//    enough to get EDUSYSTEM 20 C BASIC working

// sources:
//  edu20c.ls listing
//  small computer hadbook 1970 p168..172

// arm registers:
//  [0] = ident='DC',sizecode=2,version
//  [1] = some global bits
//  [2] = terminal 0 bits (enabled by MTON[11])
//  [3] = terminal 1 bits (enabled by MTON[10])
//     ...
//  [7] = terminal 5 bits (enabled by MTON[06])

module pdp8ldc02
(
    input CLOCK, CSTEP, RESET, BINIT,

    input armwrite,
    input[2:0] armraddr, armwaddr,
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

    reg enable, intenab;
    reg[5:0] kbflags, prflags, prfulls;
    reg[11:00] kbchars[5:0], mtonreg, prchars[5:0];

    wire[2:0] rn = armraddr - 2;
    wire[2:0] wn = armwaddr - 2;

    assign armrdata = (armraddr == 0) ? 32'h44432002 : // [31:16] = 'DC'; [15:12] = (log2 nreg) - 1; [11:00] = version
                      (armraddr == 1) ? { enable, intenab, 2'b0, mtonreg, 16'b0 } :
                      { kbflags[rn], prflags[rn], prfulls[rn], 5'b0, prchars[rn], kbchars[rn] };

    assign INT_RQST = intenab & (
            (mtonreg[11] & (kbflags[0] | prflags[0])) |
            (mtonreg[10] & (kbflags[1] | prflags[1])) |
            (mtonreg[09] & (kbflags[2] | prflags[2])) |
            (mtonreg[08] & (kbflags[3] | prflags[3])) |
            (mtonreg[07] & (kbflags[4] | prflags[4])) |
            (mtonreg[06] & (kbflags[5] | prflags[5])));

    always @(posedge CLOCK) begin
        if (BINIT) begin
            if (RESET) begin
                enable <= 1;
            end
            intenab <= 0;
            kbflags <= 0;
            mtonreg <= 0;
            prflags <= 0;
            prfulls <= 0;
        end

        // arm processor is writing one of the registers
        else if (armwrite) begin

            if (armwaddr == 1) begin
                enable <= armwdata[31];
            end

            // registers 2..7 => terminals 0..5
            // <24> = kbchars write enable
            // <25> = prchars write enable
            // <26> = prfulls write enable
            // <27> = prflags write enable
            // <28> = kbflags write enable
            else if (armwaddr >= 2) begin
                if (armwdata[24]) kbchars[wn] <= armwdata[11:00];
                if (armwdata[25]) prchars[wn] <= armwdata[23:12];
                if (armwdata[26]) prfulls[wn] <= armwdata[29];
                if (armwdata[27]) prflags[wn] <= armwdata[30];
                if (armwdata[28]) kbflags[wn] <= armwdata[31];
            end
        end else if (CSTEP) begin

            // process the IOP only on the leading edge
            // ...but leave output signals to PDP-8/L in place until given the all clear
            if (iopstart & enable) begin
                case (ioopcode)

                    // MKSF - skip if enabled keyboard ready
                    12'o6111: begin
                        IO_SKIP <=
                            kbflags[0] & mtonreg[11] |
                            kbflags[1] & mtonreg[10] |
                            kbflags[2] & mtonreg[09] |
                            kbflags[3] & mtonreg[08] |
                            kbflags[4] & mtonreg[07] |
                            kbflags[5] & mtonreg[06];
                    end

                    // MKCC - clear enabled keyboard flags
                    12'o6112: begin
                        AC_CLEAR <= 1;
                        if (mtonreg[11]) kbflags[0] <= 0;
                        if (mtonreg[10]) kbflags[1] <= 0;
                        if (mtonreg[09]) kbflags[2] <= 0;
                        if (mtonreg[08]) kbflags[3] <= 0;
                        if (mtonreg[07]) kbflags[4] <= 0;
                        if (mtonreg[06]) kbflags[5] <= 0;
                    end

                    // MTPF - read all printer flags
                    12'o6113: begin
                        devtocpu[11] <= prflags[0];
                        devtocpu[10] <= prflags[1];
                        devtocpu[09] <= prflags[2];
                        devtocpu[08] <= prflags[3];
                        devtocpu[07] <= prflags[4];
                        devtocpu[06] <= prflags[5];
                        devtocpu[05:00] <= 0;
                    end

                    // MKRS - read enabled keyboard characters
                    12'o6114: begin
                        devtocpu <=
                            (mtonreg[11] ? kbchars[0] : 12'o0000) |
                            (mtonreg[10] ? kbchars[1] : 12'o0000) |
                            (mtonreg[09] ? kbchars[2] : 12'o0000) |
                            (mtonreg[08] ? kbchars[3] : 12'o0000) |
                            (mtonreg[07] ? kbchars[4] : 12'o0000) |
                            (mtonreg[06] ? kbchars[5] : 12'o0000);
                    end

                    // MINT - set global interrupt enable
                    12'o6115: begin
                        intenab <= cputodev[00];
                    end

                    // MKRB - read enabled keyboard characters and clear enabled flags
                    12'o6116: begin
                        devtocpu <=
                            (mtonreg[11] ? kbchars[0] : 12'o0000) |
                            (mtonreg[10] ? kbchars[1] : 12'o0000) |
                            (mtonreg[09] ? kbchars[2] : 12'o0000) |
                            (mtonreg[08] ? kbchars[3] : 12'o0000) |
                            (mtonreg[07] ? kbchars[4] : 12'o0000) |
                            (mtonreg[06] ? kbchars[5] : 12'o0000);
                        AC_CLEAR <= 1;
                        if (mtonreg[11]) kbflags[0] <= 0;
                        if (mtonreg[10]) kbflags[1] <= 0;
                        if (mtonreg[09]) kbflags[2] <= 0;
                        if (mtonreg[08]) kbflags[3] <= 0;
                        if (mtonreg[07]) kbflags[4] <= 0;
                        if (mtonreg[06]) kbflags[5] <= 0;
                    end

                    // MTON - set enabled lines
                    //  edu20c uses [11] to enable terminal 0, [10] to enable terminal 1, ..., [04] to enable terminal 7
                    //  it also always sets [03:00] = 4'b0100, comments says 'GROUP 1', but we ignore them
                    12'o6117: begin
                        mtonreg <= cputodev;
                    end

                    // MTSF - skip if enabled printer ready
                    12'o6121: begin
                        IO_SKIP <=
                            prflags[0] & mtonreg[11] |
                            prflags[1] & mtonreg[10] |
                            prflags[2] & mtonreg[09] |
                            prflags[3] & mtonreg[08] |
                            prflags[4] & mtonreg[07] |
                            prflags[5] & mtonreg[06];
                    end

                    // MTCF - clear enabled printer flag
                    12'o6122: begin
                        if (mtonreg[11]) prflags[0] <= 0;
                        if (mtonreg[10]) prflags[1] <= 0;
                        if (mtonreg[09]) prflags[2] <= 0;
                        if (mtonreg[08]) prflags[3] <= 0;
                        if (mtonreg[07]) prflags[4] <= 0;
                        if (mtonreg[06]) prflags[5] <= 0;
                    end

                    // MTKF - read all keyboard flags
                    12'o6123: begin
                        devtocpu[11] <= kbflags[0];
                        devtocpu[10] <= kbflags[1];
                        devtocpu[09] <= kbflags[2];
                        devtocpu[08] <= kbflags[3];
                        devtocpu[07] <= kbflags[4];
                        devtocpu[06] <= kbflags[5];
                        devtocpu[05:00] <= 0;
                    end

                    // MINS - multiple interrupt and skip
                    12'o6125: begin
                        IO_SKIP <= INT_RQST;
                    end

                    // MTLS - clear printer ready and start printing
                    12'o6126: begin
                        if (mtonreg[11]) begin prflags[0] <= 0; prfulls[0] <= 1; prchars[0] <= cputodev; end
                        if (mtonreg[10]) begin prflags[1] <= 0; prfulls[1] <= 1; prchars[1] <= cputodev; end
                        if (mtonreg[09]) begin prflags[2] <= 0; prfulls[2] <= 1; prchars[2] <= cputodev; end
                        if (mtonreg[08]) begin prflags[3] <= 0; prfulls[3] <= 1; prchars[3] <= cputodev; end
                        if (mtonreg[07]) begin prflags[4] <= 0; prfulls[4] <= 1; prchars[4] <= cputodev; end
                        if (mtonreg[06]) begin prflags[5] <= 0; prfulls[5] <= 1; prchars[5] <= cputodev; end
                    end
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
