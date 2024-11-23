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

// PDP-8/L VC-8 interface

module pdp8lvc8 (
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
    output INT_RQST,

    output reg[14:00] vidaddra, vidaddrb,
    output reg[21:00] viddataa,
    output reg videnaba, vidwrena, videnabb,
    input[21:00] viddatab
);

    localparam EF_M_DN = 12'o4000;      // done executing last command
    localparam EF_M_ST = 12'o0020;      // storage mode
    localparam EF_M_IE = 12'o0001;      // interrupt enable
    localparam EF_M_MASK = 12'o77;      // loadable bits

    localparam EF_V_DN = 11;
    localparam EF_V_CO =  2;            // 0=green; 1=red
    localparam EF_V_IE =  0;

    reg[21:00] vidramreaddata;
    reg[14:00] vidramreadaddr;
    reg[1:0] vidramreadbusy, vidramwritebsy;
    reg vidramempty;

    reg[11:00] eflags;
    reg[14:00] insert, remove;
    reg[9:0] xcobuf, ycobuf;
    reg[1:0] intens;
    reg typee;

    assign armrdata = (armraddr == 0) ? 32'h56432004 : // [31:16] = 'VC'; [15:12] = (log2 nreg) - 1; [11:00] = version
                      (armraddr == 1) ? { 1'b0, remove, 1'b0, insert } :
                      (armraddr == 2) ? { typee, 1'b0, intens, eflags, 16'b0 } :
                      (armraddr == 3) ? { 17'b0, vidramreadaddr } :
                      (armraddr == 4) ? { vidramreadbusy, vidramempty, 7'b0, vidramreaddata } :
                       32'hDEADBEEF;

    wire[9:0] newixcobuf = (ioopcode[00] ? 0 : xcobuf) | (ioopcode[01] ? cputodev[09:00] : 0);
    wire[9:0] newiycobuf = (ioopcode[00] ? 0 : ycobuf) | (ioopcode[01] ? cputodev[09:00] : 0);
    wire[14:00] incdinsert = insert + 1;
    wire[14:00] incdremove = remove + 1;

    assign INT_RQST = eflags[EF_V_DN] & eflags[EF_V_IE];

    always @(posedge CLOCK) begin
        if (BINIT) begin
            if (RESET) begin
                typee  <= 0;
                eflags <= EF_M_ST;
                intens <= 3;
                vidramreadbusy <= 0;
                vidramwritebsy <= 0;
            end else begin
                eflags <= typee ? 0 : EF_M_ST;
                intens <= typee ? 0 : 3;
            end
            insert <= 0;
            remove <= 0;
        end

        // arm processor is writing one of our registers
        else if (armwrite) begin
            case (armwaddr)
                1: begin
                    insert <= armwdata[14:00];
                    remove <= armwdata[30:16];
                end
                2: begin
                    typee  <= armwdata[31];
                    intens <= armwdata[29:28];
                    eflags <= armwdata[27:16];
                end
                3: if (vidramreadbusy == 0) begin           // make sure not already busy doing a read
                    if (armwdata[31]) begin                 // see if 'remove from ring' style
                        vidramreadaddr <= remove;           // if so, get address in ram to remove
                        if (remove == insert) begin         //  check for ring empty
                            vidramempty <= 1;               //   empty, set empty flag
                        end else begin
                            vidramempty    <= 0;            //   not empty, clear empty flag
                            vidramreadbusy <= 1;            //    start reading video ram
                            vidaddrb       <= remove;       //    at the remove location
                            videnabb       <= 1;
                            remove <= incdremove;           //    increment pointer for next time
                        end
                    end else begin
                        vidramreadbusy <= 1;                // ramdom read, start reading video ram
                        vidaddrb       <= armwdata[14:00];  //  at the given location
                        videnabb       <= 1;
                        vidramreadaddr <= armwdata[14:00];
                    end
                end
            endcase
        end

        // process the IOP only on the leading edge
        // ...but leave output signals to PDP-8/L in place until given the all clear
        else if (CSTEP) begin
            if (iopstart) begin
                if (typee) begin

                    // VC-8/E style

                    case (ioopcode)

                        // we can't do DILC - logic clear - cuz opcode 6050 ends in 0
                        // it resets eflags = 0

                        // DICD - clear done flag
                        12'o6051: begin
                            eflags[EF_V_DN] <= 0;
                        end

                        // DISD - skip on done
                        12'o6052: begin
                            IO_SKIP <= eflags[EF_V_DN];
                        end

                        // DILX - load x
                        12'o6053: begin
                            xcobuf <= cputodev[9:0] ^ 10'o1000;
                            eflags[EF_V_DN] <= 1;
                        end

                        // DILY - load y
                        12'o6054: begin
                            ycobuf <= cputodev[9:0] ^ 10'o1000;
                            eflags[EF_V_DN] <= 1;
                        end

                        // DIXY - intensify at (x,y)
                        12'o6055: begin
                            vidaddra <= insert;
                            viddataa <= { intens, ycobuf, xcobuf };
                            videnaba <= 1;
                            vidwrena <= 1;
                            vidramwritebsy <= 1;
                        end

                        // DILE - load enable/status register
                        12'o6056: begin
                            eflags[05:00] <= cputodev[05:00];
                            intens <= cputodev[EF_V_CO] ? 3 : 0;    // 3=RED; 0=GREEN
                        end

                        // DIRE - read enable/status register
                        12'o6057: begin
                            devtocpu <= eflags;
                            AC_CLEAR <= 1;
                        end
                    endcase
                end else begin

                    // VC-8/I style

                    if (ioopcode[11:03] == 9'o605) begin
                        xcobuf <= newixcobuf;
                        if (ioopcode[02]) begin
                            vidaddra <= insert;
                            viddataa <= { intens, ycobuf, newixcobuf };
                            videnaba <= 1;
                            vidwrena <= 1;
                            vidramwritebsy <= 1;
                        end
                    end

                    if (ioopcode[11:03] == 9'o606) begin
                        ycobuf <= newiycobuf;
                        if (ioopcode[02]) begin
                            vidaddra <= insert;
                            viddataa <= { intens, newiycobuf, xcobuf };
                            videnaba <= 1;
                            vidwrena <= 1;
                            vidramwritebsy <= 1;
                        end
                    end

                    if (ioopcode[11:03] == 9'o607) begin
                        if (ioopcode[02]) intens <= ioopcode[1:0];
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

        // 2 cycle latency writing ram
        if (~ CSTEP | ~ iopstart) case (vidramwritebsy)
            1: begin vidramwritebsy <= 2; insert <= incdinsert; if (incdinsert == remove) remove <= incdremove; end
            2: begin videnaba <= 0; vidwrena <= 0; vidramwritebsy <= 0; end
        endcase

        // 2 cycle latency reading ram
        case (vidramreadbusy)
            1: vidramreadbusy <= 2;
            2: vidramreadbusy <= 3;
            3: begin videnabb <= 0; vidramreadbusy <= 0; vidramreaddata <= viddatab; end
        endcase
    end
endmodule
