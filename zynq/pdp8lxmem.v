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

// PDP-8/L extended memory

// arm registers:
//  [0] = ident='XM',sizecode=1,version
//  [1] = <enable> <enlo4K> 0 <busy> <12-bit-data> <write> <15-bit-addr>
//    enable = 0 : ignore io instructions, ie, be a dumb 4K system
//             1 : handle io instructions
//    enlo4K = 0 : PDP-8/L core stack will be used for low 4K addresses
//             1 : the low 4K of the 32K block memory will be used for low 4K addresses
//                 ...regardless of the enable bit setting

module pdp8lxmem (
    input CLOCK, RESET,

    input armwrite,
    input[1:0] armraddr, armwaddr,
    input[31:00] armwdata,
    output[31:00] armrdata,

    input iopstart,
    input iopstop,
    input[11:00] ioopcode,
    input[11:00] cputodev,

    output reg[11:00] devtocpu,

    input memstart,                 // pulse from cpu to start memory cycle (150..500nS)
    input[11:00] memaddr,
    input[11:00] memwdat,
    output reg[11:00] memrdat,
    output reg _mrdone,             // pulse to cpu saying data is available
    output reg _mwdone,             // pulse to cpu saying data has been written
    input[2:0] brkfld,              // field for dma

    input _bf_enab, _df_enab, exefet, _intack, jmpjms, ts3, _zf_enab,
    output _ea, _intinh,

    output reg[14:00] xbraddr,
    output reg[11:00] xbrwdat,
    input[11:00] xbrrdat,
    output reg xbrenab,
    output reg xbrwena
);

    reg busyonpdp, ctlenab, ctllo4K, ctlwrite, intdisableduntiljump;
    reg[14:00] ctladdr, xaddr;
    reg[11:00] ctldata;
    reg[7:0] memdelay;
    reg[2:0] busyonarm, dfld, ifld, ifldafterjump, saveddfld, savedifld;
    reg[31:00] writecounts;
    wire ctlbusy = busyonarm != 0;

    assign armrdata = (armraddr == 0) ? 32'h584D1003 :  // [31:16] = 'XM'; [15:12] = (log2 nreg) - 1; [11:00] = version
                      (armraddr == 1) ? { ctlenab, ctllo4K, 1'b0, ctlbusy, ctldata, ctlwrite, ctladdr } :
                      (armraddr == 2) ? { 1'b0, busyonarm, busyonpdp, dfld, 1'b0, ifld, 1'b0, ifldafterjump, 1'b0, saveddfld, 1'b0, savedifld, memdelay } :
                      writecounts; //// 32'hDEADBEEF;

    wire[2:0] field = ~ _zf_enab ? 0 :                  // WC and CA cycles always use field 0
                        ~ _df_enab ? dfld :             // data field if cpu says so
                        ~ _bf_enab ? brkfld :           // break field if cpu says so
                        (jmpjms & exefet) ? ifldafterjump :
                        ifld;
    assign _ea = ~ (ctllo4K | (field != 0));
    assign _intinh = ~ intdisableduntiljump;

    always @(posedge CLOCK) begin
        if (RESET) begin
            busyonarm     <= 0;
            busyonpdp     <= 0;
            ctlenab       <= 0;
            ctllo4K       <= 0;
            dfld          <= 0;
            ifld          <= 0;
            ifldafterjump <= 0;
            intdisableduntiljump <= 0;
            memdelay      <= 0;
            _mrdone       <= 1;
            _mwdone       <= 1;
            saveddfld     <= 0;
            savedifld     <= 0;
            _mrdone       <= 1;
            xbrenab       <= 0;
            xbrwena       <= 0;
            writecounts   <= 0;
        end

        // arm processor is writing one of the registers
        else if (armwrite) begin
            case (armwaddr)

                // arm processor is wanting to access the memory
                1: begin
                    if (busyonarm == 0) begin               // make sure we own ctl... regs
                        ctlenab  <= armwdata[31];           // save overall enabled flag
                        ctllo4K  <= armwdata[30];           // save low 4K enabled flag
                        ctlwrite <= armwdata[15];           // see if this is a write access
                        ctladdr  <= armwdata[14:00];        // save address to be accessed
                        if (armwdata[15]) begin
                            ctldata <= armwdata[27:16];
                        end
                        busyonarm <= 1;                     // pass ownership of ctl... to code below
                        writecounts <= writecounts + 32'h101;
                    end else begin
                        writecounts <= writecounts + 32'h100;
                    end
                end
            endcase
        end else begin

            // maybe we have an arm request to process
            if ((busyonarm != 0) & (busyonpdp == 0)) begin
                case (busyonarm)
                    1: begin
                        xbraddr <= ctladdr;
                        xbrenab <= 1;
                        xbrwena <= ctlwrite;
                        xbrwdat <= ctldata;
                        busyonarm <= busyonarm + 1;
                    end
                    6: begin
                        if (~ ctlwrite) begin
                            ctldata <= xbrrdat;
                        end
                        xbrenab <= 0;
                        xbrwena <= 0;
                        busyonarm <= 0;
                    end
                    default: begin
                        busyonarm <= busyonarm + 1;
                    end
                endcase
            end

            // process the IOP only on the leading edge
            // ...but leave output signals to PDP-8/L in place until given the all clear
            if (ctlenab & iopstart) begin
                if (ioopcode[11:06] == 6'o62) begin
                    if (ioopcode[00]) dfld <= ioopcode[5:3];
                    if (ioopcode[01]) begin
                        ifldafterjump <= ioopcode[5:3];
                        intdisableduntiljump <= 1;
                    end
                    if (ioopcode[02]) begin
                        case (ioopcode[05:03])
                            1: begin    // 6214
                                devtocpu[05:03] <= dfld;
                            end
                            2: begin    // 6224
                                devtocpu[05:03] <= ifld;
                            end
                            3: begin    // 6234
                                devtocpu[05:03] <= savedifld;
                                devtocpu[02:00] <= saveddfld;
                            end
                            4: begin    // 6244
                                dfld <= saveddfld;
                                ifldafterjump <= savedifld;
                            end
                        endcase
                    end
                end
            end

            // maybe pdp is requesting a memory cycle on what it thinks is extended memory
            else if (memstart & ~ _ea & (memdelay == 0)) begin
                xaddr <= { field, memaddr };
                if (jmpjms & exefet) begin
                    ifld <= ifldafterjump;
                    intdisableduntiljump <= 0;
                end
                memdelay <= memdelay + 1;
            end

            // processor no longer sending an IOP, stop sending data over the bus so it doesn't jam other devices
            else if (iopstop) begin
                devtocpu <= 0;
            end

            // see if pdp is requesting a memory cycle
            if (memdelay != 0) begin

                // 150nS, start reading block memory
                if (memdelay == 15) begin
                    if (busyonarm == 0) begin
                        busyonpdp <= 1;
                        xbraddr  <= xaddr;
                        xbrenab  <= 1;
                        xbrwena  <= 0;
                        memdelay <= memdelay + 1;
                    end
                end

                // 200nS, send read data to cpu
                else if (memdelay == 20) begin
                    busyonpdp <= 0;
                    memrdat  <= xbrrdat;
                    xbrenab  <= 0;
                    memdelay <= memdelay + 1;
                end

                // 600nS, send strobe pulse 100nS wide
                else if (memdelay == 60) begin
                    _mrdone <= 0;
                    memdelay <= memdelay + 1;
                end
                else if (memdelay == 70) begin
                    _mrdone <= 1;
                    memdelay <= memdelay + 1;
                end

                // 950nS, clock in write data, send memdone pulse 100nS wide
                if (memdelay == 95) begin
                    if (busyonarm == 0) begin
                        busyonpdp <= 1;
                        xbraddr   <= xaddr;
                        xbrwdat   <= memwdat;
                        xbrenab   <= 1;
                        xbrwena   <= 1;
                        memdelay  <= memdelay + 1;
                        _mwdone   <= 0;
                    end
                end

                else if (memdelay == 100) begin
                    busyonpdp <= 0;
                    xbrenab   <= 0;
                    xbrwena   <= 0;
                    memdelay  <= memdelay + 1;
                end

                // 1.05uS, all done, shut off memdone pulse
                else if (memdelay == 105) begin
                    memdelay <= 0;
                    _mwdone  <= 1;
                end

                // in between somewhere, just increment counter
                else begin
                    memdelay <= memdelay + 1;
                end
            end
        end
    end
endmodule
