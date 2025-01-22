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
//  [1] = <enable> <enlo4k> 00000000000000000000000000000 os8zap
//    enable = 0 : ignore io instructions, ie, be a dumb 4K system
//             1 : handle io instructions
//    enlo4k = 0 : PDP-8/L core stack will be used for low 4K addresses
//             1 : the low 4K of the 32K block memory will be used for low 4K addresses
//                 ...regardless of the enable bit setting
//    os8zap = 0 : nothing special
//             1 : os8 likes to do:
//                  ISZ x
//                  JMP .-1
//              ...as a delay during printing
//              zap the JMP .-1 to be:
//                  NOP ; x <= 0
//              must have enlo4k set for it to work in low 4K

module pdp8lxmem (
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

    input memstart,                 // pulse from cpu to start memory cycle (150..500nS)
    input[11:00] memaddr,
    input[11:00] memwdat,
    output reg[11:00] memrdat,
    output reg _mrdone,             // pulse to cpu saying data is available
    output reg _mwdone,             // pulse to cpu saying data has been written
    input[2:0] brkfld,              // field for dma
    output[2:0] field,              // current field being used

    input _bf_enab, _df_enab, exefet, _intack, jmpjms, ts1, ts3, tp3, _zf_enab,
    output _ea, _intinh,
    input[9:0] meminprog,
    output reg r_MA, x_MEM, hizmembus,

    input ldaddrsw,                 // load address switch
    input[2:0] ldaddfld, ldadifld,  // ifld, dfld for load address switch

    output reg[14:00] xbraddr,      // external block ram address bus
    output[11:00] xbrwdat,          // ... write data bus
    input[11:00] xbrrdat,           // ... read data bus
    output reg xbrenab,             // ... chip enable
    output reg xbrwena              // ... write enable

    ,output xmstate
);

    reg ctlenab, ctllo4k, ctlwrite, intinhibeduntiljump;
    reg[2:0] lastintack;
    reg[2:0] dfld, ifld, ifldafterjump, saveddfld, savedifld;
    reg[7:0] memdelay, numcycles;
    reg[7:0] addrlatchwid, readstrobedel, readstrobewid, writeenabdel, writeenabwid, writedonewid;

    reg mwhold, mwstep, mrhold, mrstep, os8zap;
    reg[11:00] os8iszrdata;
    reg[14:00] os8iszxaddr;
    reg[1:0] os8step;
    reg[6:0] os8iszopcad;
    reg[5:0] xmstate;

    // the _xx_enab inputs are valid in the cycle before the one they are needed in
    // ...and they go away right around the start of TS1 where they are needed
    // so we save them at the end of previous TS3 so they are valid at start of TS1 through end of TS3 that they are needed for
    reg buf_bf_enab, buf_df_enab, buf_zf_enab, lastts3;

    assign field =
                ~ buf_bf_enab ? brkfld :    // break field if cpu says so
                ~ buf_df_enab ? dfld   :    // data field if cpu says so
                ~ buf_zf_enab ? 0      :    // WC and CA cycles always use field 0
                                ifld;       // by default, use instruction field

    assign armrdata = (armraddr == 0) ? 32'h584D2028 :  // [31:16] = 'XM'; [15:12] = (log2 nreg) - 1; [11:00] = version
                      (armraddr == 1) ? { ctlenab, ctllo4k, 25'b0, mrhold, mrstep, mwhold, mwstep, os8zap } :
                      (armraddr == 2) ? { _mrdone, _mwdone, field, 4'b0, dfld, ifld, ifldafterjump, saveddfld, savedifld, 2'b0, xmstate } :
                      (armraddr == 3) ? { numcycles, lastintack, buf_bf_enab, buf_df_enab, buf_zf_enab, 16'b0, os8step } :
                      (armraddr == 4) ? { writeenabdel, readstrobewid, readstrobedel, addrlatchwid } :
                      (armraddr == 5) ? { 16'b0, writedonewid, writeenabwid } :
                      32'hDEADBEEF;

    assign _ea = ~ (ctllo4k | (field != 0));
    assign _intinh = ~ intinhibeduntiljump;

    wire[6:0] os8jmpopcad = os8iszopcad + 1;

    assign xbrwdat = memwdat;               // data being written to external memory (from PDP/sim's MB register)

    // main processing loop
    always @(posedge CLOCK) begin
        if (BINIT) begin
            if (RESET) begin
                // these get cleared on power up
                // they remain as is when start switch is pressed
                buf_bf_enab   <= 1;
                buf_df_enab   <= 1;
                buf_zf_enab   <= 1;
                ctlenab       <= 1;
                ctllo4k       <= 0;
                dfld          <= 0;
                hizmembus     <= 1;
                ifld          <= 0;
                ifldafterjump <= 0;
                lastts3       <= 0;
                mrhold        <= 0;
                mrstep        <= 0;
                mwhold        <= 0;
                mwstep        <= 0;
                os8step       <= 0;
                os8zap        <= 0;
                r_MA          <= 1;
                x_MEM         <= 1;
                xbrenab       <= 0;
                xbrwena       <= 0;
                xmstate       <= 0;
                _mrdone       <= 1;
                _mwdone       <= 1;

                addrlatchwid  <=  8;                    // default extmem timing
                readstrobedel <= 42;
                readstrobewid <= 10;
                writeenabdel  <= 85;
                writeenabwid  <=  5;
                writedonewid  <= 10;
            end

            // these get cleared on power up or start switch
            intinhibeduntiljump <= 0;
            lastintack <= 0;
            numcycles  <= 0;
            saveddfld  <= 0;
            savedifld  <= 0;
        end

        // arm processor is writing one of the registers
        // armwrite lasts only 1 fpga clock cycle
        else if (armwrite) begin
            case (armwaddr)

                // arm processor is wanting to write the registers
                1: begin
                    ctlenab  <= armwdata[31];           // save overall enabled flag
                    ctllo4k  <= armwdata[30];           // save low 4K enabled flag
                    mrhold   <= armwdata[04];
                    mrstep   <= armwdata[03];
                    mwhold   <= armwdata[02];
                    mwstep   <= armwdata[01];
                    os8zap   <= armwdata[00];           // save os8zap enabled flag
                end

                4: begin
                    addrlatchwid  <= armwdata[07:00];   // time at beg of cycle to latch address
                    readstrobedel <= armwdata[15:08];   // time after that to beg of strobe pulse
                    readstrobewid <= armwdata[23:16];   // width of strobe pulse
                    writeenabdel  <= armwdata[31:24];   // time after strobe before writing ram
                end
                5: begin
                    writeenabwid  <= armwdata[07:00];   // width of write pulse
                    writedonewid  <= armwdata[15:08];   // width of memdone pulse
                end
            endcase
        end else if (CSTEP) begin
            numcycles <= numcycles + 1;

            //////////////////////////////////////////////
            //  manage IF and DF and related registers  //
            //////////////////////////////////////////////

            // capture the _xx_enable signals at the end of TS3
            // they hold the values needed for the next memory cycle
            // clocking at the end of TS3 holds them valid throughout the current cycle
            // ...and gives the next cycle plenty of time to set up
            if (lastts3 & ~ ts3) begin
                buf_bf_enab <= _bf_enab;
                buf_df_enab <= _df_enab;
                buf_zf_enab <= _zf_enab;
            end
            lastts3 <= ts3;

            // maybe we have load address switch to process
            // theoretically can't happen when the processor is running
            // ...so it won't interfere with io instruction processing below
            if (ldaddrsw) begin
                dfld <= ldaddfld;
                ifld <= ldadifld;
                ifldafterjump <= ldadifld;
            end

            // maybe there is an io instruction to process
            // see mc8l memory extension, p6
            // note: if _intack is asserted, the _intack code below that saves ifld,dfld,etc has already executed at tp3 time
            //       so anything here must take that into account, using the saved values instead of current values
            else if (iopstart & (ioopcode[11:06] == 6'o62)) begin
                case (ioopcode[02:00])
                    0,1,2,3: begin

                        // check for CDFn instruction
                        if (ioopcode[00]) begin
                            // won't be followed by intack, just set the field
                            dfld <= ioopcode[05:03];
                        end

                        // check for CIFn instruction
                        // won't be followed by intack cuz it blocks interrupts
                        if (ioopcode[01]) begin
                            ifldafterjump <= ioopcode[05:03];
                            intinhibeduntiljump <= 1;
                        end
                    end
                    4: begin
                        case (ioopcode[05:03])
                            1: begin    // 6214 RDF
                                devtocpu[05:03] <= dfld;
                            end
                            2: begin    // 6224 RIF
                                devtocpu[05:03] <= ifld;
                            end
                            3: begin    // 6234 RIB
                                devtocpu[05:03] <= savedifld;
                                devtocpu[02:00] <= saveddfld;
                            end

                            4: begin    // 6244 RMF
                                dfld          <= saveddfld;
                                ifldafterjump <= savedifld;
                            end
                        endcase
                    end
                endcase
            end

            // somewhere in middle of instruction to be followed by jms 0 for an interrupt
            //  _intack = next cycle will be the exec of the jms 0 for calling interrupt service routine
            //            asserted only during TS4 (p22 C-5; p9)
            // the interrupt service routine always starts in field 0
            if (~ _intack) begin
                if (lastintack == 6) begin  // have solid 60nS of _intack in TS4 cycle
                    saveddfld <= dfld;      // save data & instruction fields
                    savedifld <= jmpjms ? ifldafterjump : ifld;
                    dfld <= 0;              // interrupt routine starts in field 0
                    ifld <= 0;
                    ifldafterjump <= 0;
                end
                if (lastintack != 7) lastintack <= lastintack + 1;
            end

            // somewhere in middle of jmp/jms, don't inhibit interrupts any more
            //  tp3    = blip near end of writing memory (our external block ram, sim's localcore array, or PDP-8/L core stack)
            //  jmpjms = JMP or JMS opcode currently in instruction register
            //  exefet = EXEC or FETCH state is next
            //
            // cases:
            //  direct JMP:
            //                      <- here with jmpjms = 0
            //   jmp fetch:  old IF
            //                      <- here with jmpjms = 1, exefet = 1
            //   next fetch: new IF
            //
            //  indirect JMP:
            //                      <- here with jmpjms = 0
            //   jmp fetch:  old IF
            //                      <- here with jmpjms = 1, exefet = 0
            //   jmp defer:  old IF
            //                      <- here with jmpjms = 1, exefet = 1
            //   next fetch: new IF
            //
            //  direct JMS:
            //                      <- here with jmpjms = 0
            //   jms fetch:  old IF
            //                      <- here with jmpjms = 1, exefet = 1
            //   jms exec:   new IF
            //                      <- here with jmpjms = 1, exefet = 1
            //   next fetch: new IF
            //
            //  indirect JMS:
            //                      <- here with jmpjms = 0
            //   jms fetch:  old IF
            //                      <- here with jmpjms = 1, exefet = 0
            //   jms defer:  old IF
            //                      <- here with jmpjms = 1, exefet = 1
            //   jms exec:   new IF
            //                      <- here with jmpjms = 1, exefet = 1
            //   next fetch: new IF
            //
            else if (tp3 & jmpjms & exefet) begin
                intinhibeduntiljump <= 0;
                ifld <= ifldafterjump;
            end

            // processor no longer sending an IOP, stop sending data over the bus so it doesn't jam other devices
            else if (iopstop) begin
                devtocpu <= 0;
            end

            // process each intack cycle only once
            // ie, don't process another one until this one is finished
            if (_intack) lastintack <= 0;

            ////////////////////////////////////
            //  external 32KW memory control  //
            ////////////////////////////////////

            // wait for MEMSTART=1 and _EA=0 meaning PDP/sim wants to use external memory
            // follows timing from PDP-8/L maint vol 1, p 4-14
            //    0.. 250nS : latch 15-bit memory address into XBRADDR
            //  250..1600nS : read from FPGA memory, gate onto FPGAs MEMBUS for rest of cycle
            //  500.. 600nS : send STROBE pulse to PDP/sim saying read data is valid
            // 1000..1500nS : write data coming from PDP/sim MB register to FPGA memory
            // 1500..1600nS : send MEMDONE pule to PDP/sim saying write is complete

            case (xmstate)

                // wait for PDP/sim to start a memory cycle for external memory
                0: begin
                    x_MEM <= 1;

                    // check for PDP/sim starting a memory cycle
                    if (memstart) begin
                        hizmembus <= 1;                     // hi-Z the FPGAs MEMBUS pins so we can read PDPs MA
                        memdelay  <= 0;                     // reset memory delay line

                        // starting an internal memory cycle (internal to PDP, ie, its 4KW core stack)
                        if (_ea) xmstate <= 10;             // wait for internal memory cycle to complete

                        // starting an external memory cycle (external to PDP, ie, our 32KW memory)
                        else xmstate <= 1;                  // start processing external memory cycle
                    end

                    // if it has been a while since any MEMSTART pulse, assume the PDP is stopped
                    // clock in the PDPs MA register so it shows up on console
                    else case (meminprog)
                        7: hizmembus <= 1;                  // hi-Z the FPGAs MEMBUS pins so we can read PDPs MA
                        6: r_MA <= 0;                       // gate PDPs MA onto FPGAs MEMBUS
                                                            // zynq.v clocks it into its oMA
                        0: r_MA <= 1;                       // should be soaked in by now
                    endcase
                end

                // doing external memory cycle:
                //   give us 80nS to latch MA contents from FPGA's MEMBUS (though it really comes in around 30-40nS)
                //   this is analogous to waiting for the start of the core memory READ pulse in the real PDP
                1: begin
                    if (memdelay != addrlatchwid) begin
                        memdelay <= memdelay + 1;
                        r_MA     <= 0;
                        xbraddr  <= { field, memaddr };
                    end else begin
                        memdelay <= 0;
                        r_MA     <= 1;
                        xbrenab  <= 1;
                        xmstate  <= 2;
                    end
                end

                // wait another 420nS then send out STROBE pulse to PDP/sim saying data is valid
                2: begin
                    memrdat   <= xbrrdat;   // save latest value read from extmem ram
                    x_MEM     <= 0;         // gate memrdat out to PDP via FPGAs MEMBUS
                    hizmembus <= 0;
                    if (memdelay != readstrobedel) begin
                        memdelay <= memdelay + 1;
                    end else begin
                        memdelay <= 0;
                        mrstep   <= 0;      // tell stepper we are about to stop
                        xmstate  <= 3;      // ...then start sending strobe pulse
                    end
                end

                // end the STROBE pulse after 100nS
                // then we're 600nS into the cycle
                3: begin
                    if (~ mrhold | mrstep) begin
                        if (memdelay != readstrobewid) begin
                            memdelay <= memdelay + 1;
                            _mrdone  <= 0;
                        end else begin
                            memdelay <= 0;
                            _mrdone  <= 1;
                            xmstate  <= 44;
                        end
                    end
                end

                // write to FPGA memory after another 850nS
                // then we're 1450nS into the cycle
                // supposedly the PDP has come up with data by then
                44: begin
                    if (memdelay != writeenabdel) begin
                        memdelay <= memdelay + 1;
                    end else begin
                        memdelay <= 0;
                        xbrwena  <= 1;
                        xmstate  <= 55;
                    end
                end

                // write the FPGA memory for 50nS
                // then we're 1500nS into the cycle
                55: begin
                    if (memdelay != writeenabwid) begin
                        memdelay <= memdelay + 1;
                    end else if (~ armwrite) begin
                        memdelay <= 0;
                        mwstep   <= 0;              // tell stepper (eg z8lmctrace.cc) cycle is complete
                        xbrenab  <= 0;              // stop writing to FPGA 32KW memory
                        xbrwena  <= 0;
                        xmstate  <= 6;
                    end
                end

                // output 100nS write complete pulse to processor
                // then we're 1600nS into the cycle
                // but wait for stepper if we are doing stepping
                6: begin
                    if (~ mwhold | mwstep) begin
                        if (memdelay != writedonewid) begin
                            _mwdone  <= 0;
                            memdelay <= memdelay + 1;
                        end else begin
                            _mwdone  <= 1;
                            xmstate  <= 0;
                        end
                    end
                end

                // start of memory cycle using core memory
                // grab a copy of the MA so it shows up on console
                // FPGA MEMBUS pins are already in hi-Z
                10: begin
                    if (memdelay != 5) begin
                        r_MA <= 0;      // gate from PDP's MA bus onto FPGA MEMBUS for 50nS
                        memdelay <= memdelay + 1;
                    end else begin
                        r_MA <= 1;      // 50nS is up, stop gating MA onto FGPA MEMBUS
                        xmstate <= 11;
                    end
                end

                // send out zeroes to FPGA MEMBUS to open-drain the transistors
                // so they don't interfere with PDP reading its own core memory
                11: begin
                    hizmembus <= 0;
                    if (~ memstart) begin
                        xmstate <= 0;
                    end
                end
            endcase
        end
    end
endmodule
