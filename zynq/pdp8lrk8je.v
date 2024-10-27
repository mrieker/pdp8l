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

// PDP-8/L RK8JE interface

module pdp8lrk8je (
    input CLOCK, RESET,

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

    localparam ST_DONE = 11;    // transfer complete
    localparam ST_HDIM = 10;    // head in motion
    localparam ST_XFRX = 09;    // transfer capacity exceeded
    localparam ST_SKFL = 08;    // seek fail
    localparam ST_FLNR = 07;    // file not ready
    localparam ST_CBSY = 06;    // controller busy
    localparam ST_TMER = 05;    // timing error
    localparam ST_WLER = 04;    // write lock error
    localparam ST_CRCR = 03;    // crc error
    localparam ST_DRLT = 02;    // data request late
    localparam ST_DSER = 01;    // drive status error
    localparam ST_CYLR = 00;    // cylinder error

    reg[11:00] command, diskaddr, memaddr, status;
    reg stbusy, startio, enable;

    assign armrdata = (armraddr == 0) ? 32'h524B2002 : // [31:16] = 'RK'; [15:12] = (log2 nreg) - 1; [11:00] = version
                      (armraddr == 1) ? { 20'b0, command  } :
                      (armraddr == 2) ? { 20'b0, diskaddr } :
                      (armraddr == 3) ? { 20'b0, memaddr  } :
                      (armraddr == 4) ? { 20'b0, status   } :
                      (armraddr == 5) ? { 29'b0, stbusy, startio, enable } :
                       32'hDEADBEEF;

    wire stskip = status[ST_DONE] | status[ST_XFRX] | status[ST_SKFL] | status[ST_FLNR] | status[ST_TMER] |
                  status[ST_WLER] | status[ST_CRCR] | status[ST_DRLT] | status[ST_DSER] | status[ST_CYLR];

    assign INT_RQST = command[08] & stskip;

    always @(posedge CLOCK) begin
        if (RESET) begin
            command  <= 0;
            diskaddr <= 0;
            enable   <= 0;
            memaddr  <= 0;
            status   <= 0;
            startio  <= 0;
            stbusy   <= 0;
        end

        // arm processor is writing one of our registers
        else if (armwrite) begin
            case (armwaddr)
                1: command  <= armwdata[11:00];
                2: diskaddr <= armwdata[11:00];
                3: memaddr  <= armwdata[11:00];
                4: status   <= armwdata[11:00];
                5: begin enable <= armwdata[00]; startio <= armwdata[01]; stbusy <= armwdata[02]; end
            endcase
        end

        // process the IOP only on the leading edge
        // ...but leave output signals to PDP-8/L in place until given the all clear
        else if (iopstart & enable) begin
            case (ioopcode)

                // DSKP - skip if transfer done or error
                // p 225 v 9-116
                12'o6741: begin
                    IO_SKIP <= stskip;
                end

                // DCLR - function in AC<01:00>
                12'o6742: begin
                    case (cputodev[01:00])

                        // clear status (unless busy doing something)
                        0: begin
                            if (stbusy) begin
                                status[ST_CBSY] <= 1;
                            end else begin
                                status <= 0;
                            end
                        end

                        // clear controller
                        // aborts any i/o in progress
                        1: begin
                            command <= 0;
                            memaddr <= 0;
                            startio <= 1;
                            status  <= 0;
                            stbusy  <= 1;
                        end

                        // reset drive
                        2: begin
                            if (stbusy) begin
                                status[ST_CBSY] <= 1;
                            end else begin
                                // seek cylinder 0
                                command[11:09] <= 3;
                                command[07:00] <= 0;
                                diskaddr <= 0;
                                startio  <= 1;
                                stbusy   <= 1;
                            end
                        end

                        // clear status including busy bit
                        // aborts any i/o in progress
                        3: begin
                            startio <= 1;
                            status  <= 0;
                        end
                    endcase
                end

                // DLAG - load disk address, clear accumulator and start function in command register
                12'o6743: begin
                    if (stbusy) begin
                        status[ST_CBSY] <= 1;
                    end else begin
                        AC_CLEAR <= 1;
                        devtocpu <= 0;
                        diskaddr <= cputodev;
                        startio  <= 1;
                        stbusy   <= 1;
                    end
                end

                // DLCA - load current address register from the AC
                12'o6744: begin
                    if (stbusy) begin
                        status[ST_CBSY] <= 1;
                    end else begin
                        AC_CLEAR <= 1;
                        devtocpu <= 0;
                        memaddr  <= cputodev;
                    end
                end

                // DRST - clear the AC and read conents of status register into the AC
                12'o6745: begin
                    AC_CLEAR <= 1;
                    devtocpu <= status;
                end

                // DLDC - load command register from the AC, clear AC and clear status register
                12'o6746: begin
                    if (stbusy) begin
                        status[ST_CBSY] <= 1;
                    end else begin
                        AC_CLEAR <= 1;
                        command  <= cputodev;
                        devtocpu <= 0;
                        status   <= 0;
                    end
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
endmodule
