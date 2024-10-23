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

// PDP-8/L-like processor, using the signals on the B,C,D 34,35,36 connectors plus front panel

module pdp8l (
    input CLOCK, RESET,         // fpga 100MHz clock and reset
    input iBEMA,                // B35-T2,p5 B-7,J11-45,,B25,"if low, blocks mem protect switch"
    input iCA_INCREMENT,        // C35-M2,p15 A-3,J11-30,,C25,?? NOT ignored in PDP-8/L see p2 D-2
    input iDATA_IN,             // C36-M2,p15 B-2,J11-32,,,
    input[11:00] iINPUTBUS,     // D34-B1,p15 B-8,PIOBUSA,,,gated out to CPU by x_INPUTBUS
    input iMEMINCR,             // C36-T2,p15 B-1,J11-18,,,
    input[11:00] iMEM,          // B35-D1,p19 C-7,MEMBUSH,,,gated out to CPU by x_MEM
    input iMEM_P,               // B35-B1,p19 C-8,MEMBUSA,,,gated out to CPU by x_MEM
    input iTHREECYCLE,          // C35-K2,p15 A-3,J11-38,,C22,
    input i_AC_CLEAR,           // D34-P2,p15 C-2,J12-27,,D33,gated out to CPU by x_INPUTBUS
    input i_BRK_RQST,           // C36-K2,p15 B-3,J11-36,,,"used on p5 B-2, clocked by TP1"
    input[11:00] i_DMAADDR,     // C36-B1,p15 B-8,DMABUSB,,,gated out to CPU by x_DMAADDR
    input[11:00] i_DMADATA,     // C35-B1,p15 A-8,DMABUSA,,,gated out to CPU by x_DMADATA
    input i_EA,                 // B34-B1,p18 C-8,J11-53,,B30,high: use CPU core stack for mem cycle; low: block using CPU core stack
    input i_EMA,                // B35-V2,p5 B-7,J11-51,,B29,goes to EA light bulb on front panel
    input i_INT_INHIBIT,        // B36-L1,p9 B-3,J12-66,,,
    input i_INT_RQST,           // D34-M2,p15 C-3,J12-23,,D32,open collector out to CPU
    input i_IO_SKIP,            // D34-K2,p15 C-3,J12-28,,D30,gated out to CPU by x_INPUTBUS
    input i_MEMDONE,            // B34-V2,p4 C-7,J11-55,,B32,gated out to CPU by x_MEM
    input i_STROBE,             // B34-S2,p4 C-6,J11-59,,B35,gated out to CPU by x_MEM
    output[11:00] oBAC,         // D36-B1,p15 D-8,PIOBUSA,,,gated onto PIOBUS by r_BAC
    output oBIOP1,              // D36-K2,p15 D-3,J12-34,,,
    output oBIOP2,              // D36-M2,p15 D-3,J12-32,,,
    output oBIOP4,              // D36-P2,p15 D-2,J12-26,,,
    output[11:00] oBMB,         // D35-B1,p15 D-8,PIOBUSH,,,gated from CPU onto PIOBUS by r_BMB
    output oBTP2,               // B34-T2,p4 C-5,J11-61,,B36,
    output oBTP3,               // B36-H1,p4 C-4,J12-73,,,
    output oBTS_1,              // D36-T2,p15 D-1,J12-25,,,
    output oBTS_3,              // D36-S2,p15 D-2,J12-22,,,
    output oBUSINIT,            // D36-V2,p15 D-1,,,,redundant bus init
    output reg oBWC_OVERFLOW,   // C35-P2,p15 A-2,J11-16,,C33,
    output oB_BREAK,            // C36-P2,p15 B-2,J11-26,,,
    output oE_SET_F_SET,        // B36-D2,p22 C-3,J12-72,,,
    output oJMP_JMS,            // B36-E2,p22 C-3,J11-63,,,
    output oLINE_LOW,           // B36-V2,p18 B-7,J12-43,,,?? op amp output maybe needs clipping diodes
    output[11:00] oMA,          // B34-D1,p22 D-8,MEMBUSH,,,gated from CPU onto MEMBUS by r_MA
    output reg oMEMSTART,       // B34-P2,p4 D-8,J11-57,,B33,
    output reg o_ADDR_ACCEPT,   // C36-S2,p15 S-2,J11-22,,,
    output o_BF_ENABLE,         // B36-E1,p22 C-6,J12-69,,,
    output o_BUSINIT,           // C36-V2,p15 B-1,J11-9,,,?? active low bus init
    output o_B_RUN,             // D34-S2,p15 C-1,J12-29,,D36,run flipflop on p4 B-2
    output o_DF_ENABLE,         // B36-B1,p22 C-7,J12-65,,,
    output o_KEY_CLEAR,         // B36-J1,p22 C-5,J12-68,,,
    output o_KEY_DF,            // B36-S1,p22 C-4,J12-44,,,
    output o_KEY_IF,            // B36-P1,p22 C-4,J12-46,,,
    output o_KEY_LOAD,          // B36-H2,p22 C-3,J11-85,,,
    output reg o_LOAD_SF,       // B36-M1,p22 C-5,J12-52,,,
    output o_SP_CYC_NEXT,       // B36-D1,p22 C-6,J12-67,,,

    // front panel
    output[11:00] lbAC,
    output lbBRK,
    output lbCA,
    output lbDEF,
    output lbEA,
    output lbEXE,
    output lbFET,
    output lbION,
    output[2:0] lbIR,
    output lbLINK,
    output[11:00] lbMA,
    output[11:00] lbMB,
    output lbRUN,
    output lbWC,
    input swCONT,
    input swDEP,
    input swDFLD,
    input swEXAM,
    input swIFLD,
    input swLDAD,
    input swMPRT,
    input swSTEP,     // KEYSS+IREF (p4 B-6)  clears the RUN FF at any TP3 (p4 B-2)
    input swSTOP,
    input[11:00] swSR,
    input swSTART
    
    // debug
    ,output reg[ 2:0] majstate
    ,output reg[10:0] timedelay
    ,output reg[ 4:0] timestate
    ,output reg[ 9:0] cyclectr
);

    localparam MS_START = 0;
    localparam MS_FETCH = 1;
    localparam MS_DEFER = 2;
    localparam MS_EXEC  = 3;
    localparam MS_WC    = 4;
    localparam MS_CA    = 5;
    localparam MS_BRK   = 6;
    localparam MS_DEPOS = 7;

    // general processor state
    reg hidestep, intdelayed, intenabled, ldad;
    reg lastswCONT, lastswDEP, lastswEXAM, lastswLDAD, lastswSTART;
    reg irusedf, link, runff;
    wire tp1, tp2, tp3, tp4, ts1, ts2, ts3, ts4;
    reg[2:0] ir;
    reg[11:00] acum, ma, mb, pctr;
    reg[23:00] debounce;

    // various outputs that can be derived with a few gates or passthrough
    assign oBAC          = acum;
    assign oBIOP1        = (timestate == TS_DOIOP1) & mb[00];
    assign oBIOP2        = (timestate == TS_DOIOP2) & mb[01];
    assign oBIOP4        = (timestate == TS_DOIOP4) & mb[02];
    assign oBMB          = mb;
    assign oBTP2         = tp2;
    assign oBTP3         = tp3;
    assign oBTS_1        = ts1;
    assign oBTS_3        = ts3;
    assign oBUSINIT      = RESET | (~ runff & swSTART);             // power-on reset or start switch and mftp0 (p4 A-3)
    assign oJMP_JMS      = (ir[2:1] == 2);                          // JMP or JMS is loaded in IR (p22 C-2, p5 C-6)
    assign oLINE_LOW     = RESET;
    assign oMA           = ma;
    assign oB_BREAK      = (majstate == MS_BRK);                    //?? not sure if active high or low??
    assign o_BF_ENABLE   = ~ (majstate == MS_BRK);                  // next memory cycle is MS_BRK, use break frame (dma extended address bits) for next mem access (p5 D-2)
    assign o_BUSINIT     = ~ oBUSINIT;
    assign o_B_RUN       = ~ runff;
    assign o_DF_ENABLE   = ~ ((majstate == MS_EXEC) & irusedf);     // next memory cycle uses DF (p22 C-7)
    assign o_KEY_DF      = ~ swDFLD;
    assign o_KEY_IF      = ~ swIFLD;
    assign o_KEY_LOAD    = ldad & tp1;                              // load address cycle (p33 C-2, not exact match)
    assign o_SP_CYC_NEXT = ~ ((majstate == MS_WC) | (majstate == MS_CA));  // next mem cycle is MS_WC or MS_CA, use field 0 (p22 C-6, p5 C-3, also p5 B-2)
    assign oE_SET_F_SET  = 0;                                       // B36-D2,p22 C-3,J12-72,,,
    assign o_KEY_CLEAR   = 1;                                       // B36-J1,p22 C-5,J12-68,,,

    assign lbAC   = acum;
    assign lbBRK  = (majstate == MS_BRK);
    assign lbCA   = (majstate == MS_CA);
    assign lbDEF  = (majstate == MS_DEFER);
    assign lbEA   = ~ i_EA;
    assign lbEXE  = (majstate == MS_EXEC);
    assign lbFET  = (majstate == MS_FETCH);
    assign lbION  = intenabled;
    assign lbIR   = ir;
    assign lbLINK = link;
    assign lbMA   = ma;
    assign lbMB   = mb;
    assign lbRUN  = runff;
    assign lbWC   = (majstate == MS_WC);

    // local memory (presumably inferred block memory)
    reg[11:00] localcore[4095:0000];

    // calculate effective address for memory reference instructions
    wire[11:00] effaddr = { (mb[07] ? ma[11:07] : 5'b0), mb[06:00] };

    // calculate accumulator and link for tad instruction
    wire[11:00] tadacum;
    wire tadlink;
    assign { tadlink, tadacum } = { link, acum } + { 1'b0, mb };

    // calculate accumulator and link for group 1 instruction
    reg[11:00] g1acum, g1acumraw;
    reg g1link, g1linkraw;

    always @(*) begin
        // handle cla, cll, cma, cll, iac bits
        { g1linkraw, g1acumraw } = { (mb[06] ? 1'b0 : link) ^ mb[04], (mb[07] ? 12'b0 : acum) ^ (mb[05] ? 12'o7777 : 12'o0000) } + { 12'b0, mb[00] };
        // handle rotate bits
        case (mb[03:01])
            1: { g1link, g1acum } = { g1linkraw, g1acumraw[05:00], g1acumraw[11:06] };  // bsw
            2: { g1link, g1acum } = { g1acumraw, g1linkraw };                           // rol 1
            3: { g1link, g1acum } = { g1acumraw[10:00], g1linkraw, g1acumraw[11] };     // rol 2
            4: { g1link, g1acum } = { g1acumraw[00], g1linkraw, g1acumraw[11:01] };     // ror 1
            5: { g1link, g1acum } = { g1acumraw[01:00], g1linkraw, g1acumraw[11:02] };  // ror 2
            default: { g1link, g1acum } = { g1linkraw, g1acumraw };                     // all else, nop
        endcase
    end
    
    // calculate skip condition for group 2 instruction
    wire g2skip = ((mb[06] & acum[11]) |    // SMA
                   (mb[05] & (acum == 0)) | // SZA
                   (mb[04] & link)) ^       // SNL
                    mb[03];                 // reverse

    // calculate accumulator for group 2 instruction
    wire[11:00] g2acum = (mb[07] ? 0 : acum) | (mb[02] ? swSR : 0);

    // this memory cycle will do io
    wire withio = (majstate == MS_FETCH) & (mb[11:09] == 6);

    // get incoming ac bits from io bus
    wire[11:00] ioac = (i_AC_CLEAR ? acum : 0) | iINPUTBUS;

    // main processing loop
    // set tp1 near end of ts1, then this circuit will transition to ts2 then clear tp1
    // likewise for ts2, 3, 4, but do not enter ts4 is 'withio' is set

    localparam TS_IDLE    =  0;     // figure out what to do next, does console switch processing if not running
    localparam TS_TS1BODY =  1;     // tell memory to start reading location addressed by MA
    localparam TS_TP1BEG  =  2;
    localparam TS_TP1END  =  3;
    localparam TS_TS2BODY =  4;     // get contents of memory into MB and modify according to majstate S_...
    localparam TS_TP2BEG  =  5;
    localparam TS_TP2END  =  6;
    localparam TS_TS3BODY =  7;     // write contents of MB back to memory
    localparam TS_TP3BEG  =  8;
    localparam TS_TP3END  =  9;
    localparam TS_BEGIOP1 = 10;
    localparam TS_DOIOP1  = 11;     // maybe output IOP1
    localparam TS_BEGIOP2 = 12;
    localparam TS_DOIOP2  = 13;     // maybe output IOP2
    localparam TS_BEGIOP4 = 14;
    localparam TS_DOIOP4  = 15;     // maybe output IOP4
    localparam TS_TS4BODY = 16;     // finish up instruction (modify ac, link, pc, etc)
    localparam TS_TP4BEG  = 17;
    localparam TS_TP4END  = 18;

    assign tp1 = (timestate == TS_TP1BEG) | (timestate == TS_TP1END);
    assign tp2 = (timestate == TS_TP2BEG) | (timestate == TS_TP2END);
    assign tp3 = (timestate == TS_TP3BEG) | (timestate == TS_TP3END);
    assign tp4 = (timestate == TS_TP4BEG) | (timestate == TS_TP4END);

    assign ts1 =                            (timestate == TS_TS1BODY) | (timestate == TS_TP1BEG);
    assign ts2 = (timestate == TS_TP1END) | (timestate == TS_TS2BODY) | (timestate == TS_TP2BEG);
    assign ts3 = (timestate == TS_TP2END) | (timestate == TS_TS3BODY) | (timestate == TS_TP3BEG);
    assign ts4 = ((timestate == TS_TP3END) & ~ withio) | (timestate == TS_TS4BODY) | (timestate == TS_TP4BEG);

    always @(posedge CLOCK) begin
        if (RESET) begin
            timedelay <= 0;
            timestate <= TS_IDLE;

            debounce    <= 0;
            hidestep    <= 0;
            intdelayed  <= 0;
            intenabled  <= 0;
            ldad        <= 0;
            runff       <= 0;
            majstate    <= MS_START;
            lastswCONT  <= 0;
            lastswDEP   <= 0;
            lastswEXAM  <= 0;
            lastswLDAD  <= 0;
            lastswSTART <= 0;

            oBWC_OVERFLOW <= 0;
            o_ADDR_ACCEPT <= 1;
            o_LOAD_SF     <= 1;

            cyclectr      <= 0;
        end else begin
            cyclectr      <= cyclectr + 1;

            case (timestate)

                ///////////////////////////////////////////
                //  figure out what major state is next  //
                ///////////////////////////////////////////
                // - if not running, stay here until a console button is pressed
                // - if running and in MS_START, check for dma, irq, stop switch, else start fetch memory cycle
                // - if running and not in MS_START, go on to start memory cycle

                TS_IDLE: begin
                    timedelay <= 0;

                    // if not running, process console switches
                    if (~ runff) begin

                        // load address switch
                        if (debounce[23] & lastswLDAD & ~ swLDAD) begin
                            ldad      <= 1;
                            ma        <= swSR;
                            pctr      <= swSR;
                            majstate  <= MS_START;
                            timestate <= TS_TS1BODY;
                        end

                        // examine switch
                        else if (debounce[23] & lastswEXAM & ~ swEXAM) begin
                            ldad      <= 0;
                            ma        <= pctr;
                            pctr      <= pctr + 1;
                            majstate  <= MS_START;
                            timestate <= TS_TS1BODY;
                        end

                        // deposit switch
                        else if (debounce[23] & lastswDEP & ~ swDEP) begin
                            ldad      <= 0;
                            ma        <= pctr;
                            pctr      <= pctr + 1;
                            majstate  <= MS_DEPOS;
                            timestate <= TS_TS1BODY;
                        end

                        // continue switch
                        else if (debounce[23] & lastswCONT & ~ swCONT) begin
                            ldad      <= 0;
                            hidestep  <= 1;
                            runff     <= 1;
                            // stay in whatever majstate we are in now
                            // stay in TS_IDLE so next clock will execute MS_START code below if we are in MS_START
                        end

                        // start switch
                        else if (debounce[23] & lastswSTART & ~ swSTART) begin
                            acum       <= 0;
                            hidestep   <= 0;
                            intdelayed <= 0;
                            intenabled <= 0;
                            ldad       <= 0;
                            link       <= 0;
                            runff      <= 1;
                            majstate   <= MS_START;
                            // stay in TS_IDLE so next clock will execute MS_START code below
                        end
                    end

                    // running, figure out what type of memory cycle to run
                    // if majstate MS_START, check for DMA request, STOP switch, interrupt request, else start a fetch
                    else if (majstate == MS_START) begin

                        // if there is a dma request pending, start doing the dma
                        if (~ i_BRK_RQST) begin
                            ma        <= ~ i_DMAADDR;
                            majstate  <= iTHREECYCLE ? MS_WC : MS_BRK;
                            timestate <= TS_TS1BODY;
                        end

                        // maybe the stop switch is being pressed or step switch is on
                        else if (swSTOP | swSTEP & ~ hidestep) begin
                            ma    <= pctr;
                            runff <= 0;
                        end

                        // if interrupt request pending, disable interrupts, and do a JMS 0 to field 0
                        else if (~ i_INT_RQST & i_INT_INHIBIT & intenabled) begin
                            intdelayed <= 0;
                            intenabled <= 0;
                            ir         <= 0;
                            ma         <= 0;
                            o_LOAD_SF  <= 0;
                            majstate   <= MS_EXEC;
                            timestate  <= TS_TS1BODY;
                        end

                        // none of the above, start a fetch going
                        else begin
                            intenabled <= intdelayed;
                            ma         <= pctr;
                            pctr       <= pctr + 1;
                            majstate   <= MS_FETCH;
                            timestate  <= TS_TS1BODY;
                        end
                    end

                    // for all other majstates, start memory cycle for that majstate
                    else timestate <= TS_TS1BODY;
                end

                ////////////////////////////////////////////////
                //  read memory from address i_EA,MA into MB  //
                ////////////////////////////////////////////////

                // memory is being read at address in MA, either internal or external memory
                TS_TS1BODY: begin
                    oMEMSTART <= 1;
                    if (i_EA) begin
                        if (timedelay != 65) timedelay <= timedelay + 1;
                        else begin timedelay <= 0; timestate <= TS_TP1BEG; end
                    end else begin
                        if (! i_STROBE) timestate <= TS_TP1BEG;
                    end
                end

                // clock read data from internal or external memory into MB
                TS_TP1BEG: begin
                    oMEMSTART <= 0;
                    if (timedelay != 2) timedelay <= timedelay + 1;
                    else begin
                        mb <= i_EA ? localcore[ma] : iMEM;
                        timedelay <= 0;
                        timestate <= TS_TP1END;
                    end
                end

                TS_TP1END: begin
                    if (timedelay != 5) timedelay <= timedelay + 1;
                    else begin timedelay <= 0; timestate <= TS_TS2BODY; end
                end

                //////////////////////////////////////////
                //  modify MB according to major state  //
                //////////////////////////////////////////

                // allow 210nS for the data in MB to be modified by the instruction if appropriate
                TS_TS2BODY: begin
                    if (timedelay != 21) timedelay <= timedelay + 1;
                    else begin
                        case (majstate)
                            MS_DEFER: if (ma[11:03] == 1) mb <= mb + 1;
                            MS_EXEC: case (ir)
                                2: mb <= mb + 1;  // isz
                                3: mb <= acum;    // dca
                                4: mb <= pctr;    // jms
                            endcase
                            MS_WC: begin
                                mb <= mb + 1;
                                // middle of wc with count just read, detect count overflow
                                oBWC_OVERFLOW <= (mb == 12'o7777);
                                o_ADDR_ACCEPT <= 0;
                            end
                            MS_CA:  mb <= mb + iCA_INCREMENT;
                            MS_BRK: begin
                                mb <= (iDATA_IN ? ~ i_DMADATA : mb) + { 11'b0, iMEMINCR };
                                o_ADDR_ACCEPT <= 0;
                            end
                            MS_DEPOS: mb <= swSR;
                        endcase
                        timedelay <= 0;
                        timestate <= TS_TP2BEG;
                    end
                end

                TS_TP2BEG: begin
                    if (timedelay != 2) timedelay <= timedelay + 1;
                    else timestate <= TS_TP2END;
                end

                TS_TP2END: begin
                    if (timedelay != 5) timedelay <= timedelay + 1;
                    else begin timedelay <= 0; timestate <= TS_TS3BODY; end
                end

                ////////////////////////////////////////
                //  write MB back to memory location  //
                ////////////////////////////////////////

                // write modification or original data in MB back to memory
                // - for localcore, we wait 280nS then clock in the value
                // - for external memory, it indicates it has clocked it in by asserting i_MEMDONE
                TS_TS3BODY: begin
                    if (i_EA) begin
                        if (timedelay != 28) timedelay <= timedelay + 1;
                        else begin
                            localcore[ma] <= mb;
                            timedelay     <= 0;
                            timestate     <= TS_TP3BEG;
                        end
                    end else begin
                        if (! i_MEMDONE) timestate <= TS_TP3BEG;
                    end
                end

                TS_TP3BEG: begin
                    if (timedelay != 2) timedelay <= timedelay + 1;
                    else timestate <= TS_TP3END;
                end

                TS_TP3END: begin
                    if (timedelay != 5) timedelay <= timedelay + 1;
                    else begin
                        timedelay <= 0;
                        timestate <= withio ? TS_BEGIOP1 : TS_TS4BODY;
                    end
                end

                //////////////////////////////////////////////////
                //  if io instruciton, output the three pulses  //
                //////////////////////////////////////////////////

                // delay between end of memory cycle and start of first io pulse
                TS_BEGIOP1: begin
                    if (timedelay != 33) timedelay <= timedelay + 1;
                    else timestate <= TS_DOIOP1;
                end

                // output first io pulse for 580nS
                TS_DOIOP1: begin
                    if (timedelay != 33+58) timedelay <= timedelay + 1;
                    else begin
                        acum <= ioac;
                        if (~ i_IO_SKIP) pctr <= pctr + 1;
                        timestate <= TS_BEGIOP2;
                    end
                end

                // delay 280nS between first and second io pulse
                TS_BEGIOP2: begin
                    if (timedelay != 33+58+28) timedelay <= timedelay + 1;
                    else timestate <= TS_DOIOP2;
                end

                // output second io pulse for 580nS
                TS_DOIOP2: begin
                    if (timedelay != 33+58+28+58) timedelay <= timedelay + 1;
                    else begin
                        acum <= ioac;
                        if (~ i_IO_SKIP) pctr <= pctr + 1;
                        timestate <= TS_BEGIOP4;
                    end
                end

                // delay 280nS between second and third io pulse
                TS_BEGIOP4: begin
                    if (timedelay != 33+58+28+58+28) timedelay <= timedelay + 1;
                    else timestate <= TS_DOIOP4;
                end

                // output third io pulse for 500nS
                TS_DOIOP4: begin
                    if (timedelay != 33+58+28+58+28+50) timedelay <= timedelay + 1;
                    else begin
                        acum <= ioac;
                        if (~ i_IO_SKIP) pctr <= pctr + 1;
                        timedelay <= 8;             // TS4 is 400nS
                        timestate <= TS_TS4BODY;
                    end
                end

                ///////////////////////////////
                //  finish the memory cycle  //
                ///////////////////////////////
                // - do any final processing for the state such as update acum, link, pctr
                // - determine next major state based on previous state

                // finish up this cycle (400nS if did io, otherwise 480nS)
                TS_TS4BODY: begin
                    if (timedelay != 48) timedelay <= timedelay + 1;
                    else case (majstate)

                        // end of FETCH cycle
                        MS_FETCH: begin
                            case (ir)

                                // add, tad, isz, dca, jms
                                0, 1, 2, 3, 4: begin
                                    ma       <= effaddr;
                                    majstate <= mb[08] ? MS_DEFER : MS_EXEC;
                                end

                                // jmp
                                5: begin
                                    if (mb[08]) begin
                                        ma       <= effaddr;
                                        majstate <= MS_DEFER;
                                    end else begin
                                        pctr     <= effaddr;
                                        majstate <= MS_START;
                                    end
                                end

                                // iot - i/o cycle was already performed by memory control so start another fetch
                                6: majstate <= MS_START;

                                // opr - perform operation and start another fetch
                                7: begin
                                    if (~ mb[08]) begin
                                        acum  <= g1acum;
                                        link  <= g1link;
                                    end else if (~ mb[00]) begin
                                        if (g2skip) pctr <= pctr + 1;
                                        acum  <= g2acum;
                                        runff <= ~ mb[01];
                                    end
                                    majstate <= MS_START;
                                end
                            endcase
                        end

                        // end of DEFER cycle
                        MS_DEFER: begin
                            if (ir == 5) begin
                                pctr     <= ma;
                                majstate <= MS_START;
                            end else begin
                                majstate <= MS_EXEC;
                            end
                        end

                        // end of EXEC cycle
                        MS_EXEC: begin
                            case (ir)
                                0: acum <= acum & mb;                           // and
                                1: begin link <= tadlink; acum <= tadacum; end  // tad
                                2: if (mb == 0) pctr <= pctr + 1;               // isz
                                3: acum <= 0;                                   // dca
                                4: pctr <= ma + 1;                              // jms
                            endcase
                            o_LOAD_SF <= 1;
                            majstate  <= MS_START;
                        end

                        // end of wordcount
                        MS_WC: begin
                            ma       <= ma + 1;
                            majstate <= MS_CA;
                        end

                        // end of currentaddress
                        MS_CA: begin
                            ma       <= mb;     // set up value as address for dma transfer
                            majstate <= MS_BRK;
                        end

                        // end of break
                        MS_BRK: begin
                            o_ADDR_ACCEPT <= 1;
                            majstate      <= MS_START;
                        end
                    endcase
                    timedelay <= 0;
                    timestate <= TS_TP4BEG;
                end

                TS_TP4BEG: begin
                    if (timedelay != 2) timedelay <= timedelay + 1;
                    else timestate <= TS_TP4END;
                end

                TS_TP4END: begin
                    if (timedelay != 5) timedelay <= timedelay + 1;
                    else timestate <= TS_IDLE;
                end
            endcase
        end

        // debounce front-panel momentary switches
        if (~ (swCONT | swDEP | swEXAM | swLDAD | swSTART)) debounce <= 0;
        else if (~ debounce[23]) debounce <= debounce + 24'h400000;  // ~84mS

        // save these for transition testing
        lastswCONT  <= swCONT;
        lastswDEP   <= swDEP;
        lastswEXAM  <= swEXAM;
        lastswLDAD  <= swLDAD;
        lastswSTART <= swSTART;
    end
endmodule
