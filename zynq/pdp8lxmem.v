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
    input[1:0] armraddr, armwaddr,
    input[31:00] armwdata,
    output[31:00] armrdata,

    input iopstart,
    input iopstop,
    input[11:00] ioopcode,
    input[11:00] cputodev,

    output reg[11:00] devtocpu,

    input memstart,                 // pulse from cpu to start memory cycle (150..500nS)
    input memwrite,                 // pulse from cpu to start memory write
    input[11:00] memaddr,
    input[11:00] memwdat,
    output reg[11:00] memrdat,
    output reg _mrdone,             // pulse to cpu saying data is available
    output reg _mwdone,             // pulse to cpu saying data has been written
    input[2:0] brkfld,              // field for dma

    input _bf_enab, _df_enab, exefet, _intack, jmpjms, tp3, _zf_enab,
    output _ea, _intinh,

    input ldaddrsw,                 // load address switch
    input[2:0] ldaddfld, ldadifld,  // ifld, dfld for load address switch

    output reg[14:00] xbraddr,      // external block ram address bus
    output reg[11:00] xbrwdat,      // ... write data bus
    input[11:00] xbrrdat,           // ... read data bus
    output reg xbrenab,             // ... chip enable
    output reg xbrwena              // ... write enable
);

    reg ctlenab, ctllo4k, ctlwrite, intinhibeduntiljump, lastintack;
    reg[7:0] memdelay;
    reg[2:0] dfld, ifld, ifldafterjump, oldsaveddfld, oldsavedifld, saveddfld, savedifld;
    reg[7:0] numcycles;

    reg os8zap;
    reg[11:00] os8iszrdata;
    reg[14:00] os8iszxaddr;
    reg[1:0] os8step;
    reg[6:0] os8iszopcad;

    wire[2:0] field = ~ _zf_enab ? 0 :                  // WC and CA cycles always use field 0
                        ~ _df_enab ? dfld :             // data field if cpu says so
                        ~ _bf_enab ? brkfld :           // break field if cpu says so
                        ifld;

    assign armrdata = (armraddr == 0) ? 32'h584D1016 :  // [31:16] = 'XM'; [15:12] = (log2 nreg) - 1; [11:00] = version
                      (armraddr == 1) ? { ctlenab, ctllo4k, 29'b0, os8zap } :
                      (armraddr == 2) ? { _mrdone, _mwdone, field, 4'b0, dfld, ifld, ifldafterjump, saveddfld, savedifld, memdelay } :
                      { numcycles, lastintack, 21'b0, os8step };

    assign _ea = ~ (ctllo4k | (field != 0));
    assign _intinh = ~ intinhibeduntiljump;

    wire[6:0] os8jmpopcad = os8iszopcad + 1;

    always @(posedge CLOCK) begin
        if (BINIT) begin
            if (RESET) begin
                // these get cleared on power up
                // they remain as is when start switch is pressed
                ctlenab       <= 1;
                ctllo4k       <= 0;
                dfld          <= 0;
                ifld          <= 0;
                ifldafterjump <= 0;
                memdelay      <= 0;
                _mrdone       <= 1;
                _mwdone       <= 1;
                os8step       <= 0;
                os8zap        <= 0;
                xbrenab       <= 0;
                xbrwena       <= 0;
            end
            // these get cleared on power up or start switch
            intinhibeduntiljump <= 0;
            lastintack   <= 0;
            numcycles    <= 0;
            oldsaveddfld <= 0;
            oldsavedifld <= 0;
            saveddfld    <= 0;
            savedifld    <= 0;
        end

        // arm processor is writing one of the registers
        // armwrite lasts only 1 fpga clock cycle
        else if (armwrite) begin
            case (armwaddr)

                // arm processor is wanting to write the registers
                1: begin
                    ctlenab  <= armwdata[31];           // save overall enabled flag
                    ctllo4k  <= armwdata[30];           // save low 4K enabled flag
                    os8zap   <= armwdata[00];           // save os8zap enabled flag
                end
            endcase
        end else if (CSTEP) begin
            numcycles <= numcycles + 1;

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
                            if (_intack) begin
                                dfld <= ioopcode[05:03];
                            end
                            // will be followed by intack,
                            // the dfld register was already saved in saveddfld and dfld was set to 0
                            // so we must save the new dfld in saveddfld and leave dfld set to 0
                            else begin
                                saveddfld <= ioopcode[05:03];
                            end
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
                                devtocpu[05:03] <= _intack ? dfld : saveddfld;
                            end
                            2: begin    // 6224 RIF
                                devtocpu[05:03] <= _intack ? ifld : savedifld;
                            end
                            3: begin    // 6234 RIB
                                devtocpu[05:03] <= _intack ? savedifld : oldsavedifld;
                                devtocpu[02:00] <= _intack ? saveddfld : oldsaveddfld;
                            end

                            4: begin    // 6244 RMF

                                // if no interrupt, do just what book says
                                if (_intack) begin
                                    dfld          <= saveddfld;
                                    ifldafterjump <= savedifld;
                                end

                                // RMF should never be followed by interrupt but if it is...
                                //  here's what a programmer should expect:
                                //   DFLD=2, SAVEDDFLD=3, IFLD=7, SAVEDIFLD=4
                                //     RMF      sets DFLD=3, IFLDAFTERJUMP=4
                                //   DFLD=3, SAVEDDFLD=3, IFLD=7, SAVEDIFLD=4, IFLDAFTERJUMP=4
                                //     INTACK   sets SAVEDDFLD=3, SAVEDIFLD=7, DFLD=IFLD=IFLDJAFTERJUMP=0
                                //   DFLD=0, SAVEDDFLD=3, IFLD=0, SAVEDIFLD=7, IFLDAFTERJUMP=0

                                //  the intack code below has done (back at TP3 time):
                                //   OLDSAVEDDFLD=3
                                //   OLDSAVEDIFLD=4
                                //   SAVEDDFLD=2
                                //   SAVEDIFLD=7
                                //   DFLD=0
                                //   IFLD=0
                                //   IFLDAFTERJUMP=0
                                //  and now we are at IOP4 time to fix things up:
                                //   SAVEDDFLD=3
                                else begin
                                    saveddfld <= oldsaveddfld;
                                end
                            end
                        endcase
                    end
                endcase
            end

            // maybe pdp is requesting a memory cycle on extended memory
            // includes the lower 4K if our enlo4k bit is set cuz that forces _EA=0
            else if (memstart & ~ _ea & (memdelay == 0)) begin
                memdelay <= memdelay + 1;
            end

            // somewhere in middle of instruction to be followed by jms 0 for an interrupt
            //  tp3     = blip near end of writing memory (our external block ram, sim's localcore array, or PDP-8/L core stack)
            //  _intack = next cycle will be the exec of the jms 0 for calling interrupt service routine
            // the interrupt service routine always starts in field 0
            // tp3 is timing used by mc8l board
            else if (tp3 & ~ _intack & ~ lastintack) begin
                lastintack <= 1;        // only once per low-to-high transition
                oldsaveddfld <= saveddfld;
                oldsavedifld <= savedifld;
                saveddfld  <= dfld;     // save data & instruction fields
                savedifld  <= jmpjms ? ifldafterjump : ifld;
                dfld <= 0;              // interrupt routine starts in field 0
                ifld <= 0;
                ifldafterjump <= 0;
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

            // see if pdp is requesting a memory cycle
            case (memdelay)
                0: begin end

                // 150nS, start reading block memory
                15: begin
                    xbraddr  <= { field, memaddr };
                    xbrenab  <= 1;
                    xbrwena  <= 0;
                    memdelay <= memdelay + 1;
                end

                // 200nS, send read data to cpu
                20: begin
                    if (os8zap & exefet) case (os8step)

                        // see if we just fetched an ISZ direct page, not last word on page (so there is room for following JMP on page)
                        // if so, save ISZ opcode address, save ISZ operand address
                        // in any case, send opcode on to processor
                        0: begin
                            if ((xbrrdat[11:07] == 5'b01001) & (xbraddr[06:00] != 7'b1111111)) begin
                                os8iszopcad <= xbraddr[06:00];
                                os8iszxaddr <= { xbraddr[14:07], xbrrdat[06:00] };
                                os8step <= 1;
                            end
                            memrdat <= xbrrdat;
                        end

                        // see if we just read the operand for the ISZ
                        // if so, save the operand
                        // if not, last cycle wasn't really a fetch of an ISZ, abandon zap
                        // in any case, send operand on to processor
                        1: begin
                            if (xbraddr == os8iszxaddr) begin
                                os8iszrdata <= xbrrdat;
                                os8step <= 2;
                            end
                            else os8step <= 0;
                            memrdat <= xbrrdat;
                        end

                        // see if we just fetched a JMP .-1 following the ISZ
                        // if so, send NOP to processor instead of JMP .-1
                        // if not, send opcode to processor and abandon zap
                        2: begin
                            if ((xbraddr == { os8iszxaddr[14:07], os8jmpopcad }) &
                                    (xbrrdat == { 5'b10101, os8iszopcad })) begin
                                memrdat <= 12'o7000;
                                os8step <= 3;
                            end
                            else begin
                                memrdat <= xbrrdat;
                                os8step <= 0;
                            end
                        end
                    endcase
                    else begin
                        os8step <= 0;
                        memrdat <= xbrrdat;
                    end
                    xbrenab  <= 0;
                    memdelay <= memdelay + 1;
                end

                // 500nS, send strobe pulse for 100nS telling processor read data is valid
                50: begin
                    _mrdone  <= 0;
                    memdelay <= memdelay + 1;
                end

                // 600nS, wait for processor to send memwrite pulse (TS3)
                60: begin
                    _mrdone  <= 1;
                    if (memwrite) memdelay <= memdelay + 1;
                end

                // 700nS, clock in write data, start writing to memory
                70: begin

                    // see if doing ISZ/JMP .-1 zap
                    case (os8step)

                        // see if writing ISZ operand is read value + 1
                        // if so, only EXEC state of ISZ increments memory so we know we are in EXEC following FETCH of an ISZ direct
                        // if not, abandon zap
                        // in any case, send operand on to processor as is
                        2: begin
                            if (memwdat != os8iszrdata + 1) os8step <= 0;
                            xbrwdat <= memwdat;
                        end

                        // see if writing back JMP .-1 that was zapped to a NOP (should always be the case)
                        // if so, zap the writeback to write 0 to the ISZ operand and zap is complete
                        // if not, leave the writeback as is and abandon zap and disable future zapping as error indication
                        3: begin
                            if (memwdat == 12'o7000) begin
                                xbraddr <= os8iszxaddr;
                                xbrwdat <= 0;
                            end
                            else begin
                                xbrwdat <= memwdat;
                                os8zap  <= 0;
                            end
                            os8step <= 0;
                        end

                        // either not zapping or writing back FETCH of the ISZ opcode, write what processor wants as is
                        default: xbrwdat <= memwdat;
                    endcase
                    xbrenab  <= 1;
                    xbrwena  <= 1;
                    memdelay <= memdelay + 1;
                end

                // 750nS, stop writing, turn on memdone pulse for 100nS
                75: begin
                    xbrenab  <= 0;
                    xbrwena  <= 0;
                    memdelay <= memdelay + 1;
                    _mwdone  <= 0;
                end

                // 850nS, all done, shut off memdone pulse
                85: begin
                    memdelay <= 0;
                    _mwdone  <= 1;
                end

                // in between somewhere, just increment counter
                default: begin
                    memdelay <= memdelay + 1;
                end
            endcase

            // process each intack cycle only once
            // ie, don't process another one until this one is finished
            if (_intack) lastintack <= 0;
        end
    end
endmodule
