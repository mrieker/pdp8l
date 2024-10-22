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
    input CLOCK, RESET,
    input iBEMA,          // B35-T2,p5 B-7,J11-45,,B25,"if low, blocks mem protect switch"
    input iCA_INCREMENT,  // C35-M2,p15 A-3,J11-30,,C25,?? NOT ignored in PDP-8/L see p2 D-2
    input iDATA_IN,       // C36-M2,p15 B-2,J11-32,,,
    input[11:00] iINPUTBUS,      // D34-B1,p15 B-8,PIOBUSA,,,gated out to CPU by x_INPUTBUS
    input iMEMINCR,       // C36-T2,p15 B-1,J11-18,,,
    input iMEM,           // B35-D1,p19 C-7,MEMBUSH,,,gated out to CPU by x_MEM
    input iMEM_P,         // B35-B1,p19 C-8,MEMBUSA,,,gated out to CPU by x_MEM
    input iTHREECYCLE,    // C35-K2,p15 A-3,J11-38,,C22,
    input i_AC_CLEAR,     // D34-P2,p15 C-2,J12-27,,D33,gated out to CPU by x_INPUTBUS
    input i_BRK_RQST,     // C36-K2,p15 B-3,J11-36,,,"used on p5 B-2, clocked by TP1"
    input[11:00] i_DMAADDR,      // C36-B1,p15 B-8,DMABUSB,,,gated out to CPU by x_DMAADDR
    input[11:00] i_DMADATA,      // C35-B1,p15 A-8,DMABUSA,,,gated out to CPU by x_DMADATA
    input i_EA,           // B34-B1,p18 C-8,J11-53,,B30,high: use CPU core stack for mem cycle; low: block using CPU core stack
    input i_EMA,          // B35-V2,p5 B-7,J11-51,,B29,goes to EA light bulb on front panel
    input i_INT_INHIBIT,  // B36-L1,p9 B-3,J12-66,,,
    input i_INT_RQST,     // D34-M2,p15 C-3,J12-23,,D32,open collector out to CPU
    input i_IO_SKIP,      // D34-K2,p15 C-3,J12-28,,D30,gated out to CPU by x_INPUTBUS
    input i_MEMDONE,      // B34-V2,p4 C-7,J11-55,,B32,gated out to CPU by x_MEM
    input i_STROBE,       // B34-S2,p4 C-6,J11-59,,B35,gated out to CPU by x_MEM
    output[11:00] oBAC,           // D36-B1,p15 D-8,PIOBUSA,,,gated onto PIOBUS by r_BAC
    output oBIOP1,         // D36-K2,p15 D-3,J12-34,,,
    output oBIOP2,         // D36-M2,p15 D-3,J12-32,,,
    output oBIOP4,         // D36-P2,p15 D-2,J12-26,,,
    output[11:00] oBMB,           // D35-B1,p15 D-8,PIOBUSH,,,gated from CPU onto PIOBUS by r_BMB
    output oBTP2,          // B34-T2,p4 C-5,J11-61,,B36,
    output oBTP3,          // B36-H1,p4 C-4,J12-73,,,
    output oBTS_1,         // D36-T2,p15 D-1,J12-25,,,
    output oBTS_3,         // D36-S2,p15 D-2,J12-22,,,
    output oBUSINIT,       // D36-V2,p15 D-1,,,,redundant bus init
    output reg oBWC_OVERFLOW,  // C35-P2,p15 A-2,J11-16,,C33,
    output oB_BREAK,       // C36-P2,p15 B-2,J11-26,,,
    output oE_SET_F_SET,   // B36-D2,p22 C-3,J12-72,,,
    output oJMP_JMS,       // B36-E2,p22 C-3,J11-63,,,
    output oLINE_LOW,      // B36-V2,p18 B-7,J12-43,,,?? op amp output maybe needs clipping diodes
    output[11:00] oMA,            // B34-D1,p22 D-8,MEMBUSH,,,gated from CPU onto MEMBUS by r_MA
    output reg oMEMSTART,      // B34-P2,p4 D-8,J11-57,,B33,
    output reg o_ADDR_ACCEPT,  // C36-S2,p15 S-2,J11-22,,,
    output o_BF_ENABLE,    // B36-E1,p22 C-6,J12-69,,,
    output o_BUSINIT,      // C36-V2,p15 B-1,J11-9,,,?? active low bus init
    output o_B_RUN,        // D34-S2,p15 C-1,J12-29,,D36,run flipflop on p4 B-2
    output o_DF_ENABLE,    // B36-B1,p22 C-7,J12-65,,,
    output o_KEY_CLEAR,    // B36-J1,p22 C-5,J12-68,,,
    output o_KEY_DF,       // B36-S1,p22 C-4,J12-44,,,
    output o_KEY_IF,       // B36-P1,p22 C-4,J12-46,,,
    output o_KEY_LOAD,     // B36-H2,p22 C-3,J11-85,,,
    output reg o_LOAD_SF,      // B36-M1,p22 C-5,J12-52,,,
    output o_SP_CYC_NEXT,  // B36-D1,p22 C-6,J12-67,,,

    // front panel
    output[11:00] lbAC,
    output lbBRK,
    output lbCA,
    output lbDEF,
    output lbEA,
    output lbEXE,
    output lbFET,
    output lbION,
    output lbIR,
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
    ,output reg[2:0] state
    ,output reg[1:0] memodify
    ,output reg[2:0] memstate
    ,output reg[2:0] timedelay
    ,output reg[3:0] timestate
    ,output reg[9:0] cyclectr
);

    localparam S_START = 0;
    localparam S_FETCH = 1;
    localparam S_DEFER = 2;
    localparam S_EXEC  = 3;
    localparam S_WC    = 4;
    localparam S_CA    = 5;
    localparam S_BRK   = 6;
    localparam S_INTAK = 7;

    // general processor state
    reg hidestep, intdelayed, intenabled, iop1, iop2, iop4, lastts2, lastts4, ldad;
    reg lastswCONT, lastswDEP, lastswEXAM, lastswLDAD, lastswSTART;
    reg irusedf, link, runff;
    wire tp1, tp2, tp3, tp4, ts1, ts2, ts3, ts4;
    reg[2:0] ir;
    reg[11:00] acum, ma, mb, pctr;
    reg[23:00] debounce;

    // memory controller state
    localparam MS_IDLE    = 0;
    localparam MS_START   = 1;
    localparam MS_LOCAL   = 2;
    localparam MS_EXTERN  = 3;
    localparam MS_STARTIO = 4;
    localparam MS_DOIO    = 5;
    localparam MS_FINISH  = 6;
    localparam MM_NONE    = 0;
    localparam MM_DEPOS   = 1;
    localparam MM_STATE   = 2;
    reg[11:00] localcore[4095:0000];
    reg[8:0] memdelay;

    // various outputs that can be derived with a few gates or simple passthrough
    assign oBAC          = acum;
    assign oBIOP1        = iop1;
    assign oBIOP2        = iop2;
    assign oBIOP4        = iop4;
    assign oBMB          = mb;
    assign oBTP2         = tp2;
    assign oBTP3         = tp3;
    assign oBTS_1        = ts1;
    assign oBTS_3        = ts3;
    assign oBUSINIT      = RESET | (~ runff & swSTART);             // power-on reset or start switch and mftp0 (p4 A-3)
    assign oJMP_JMS      = (ir[2:1] == 2);                          // JMP or JMS is loaded in IR (p22 C-2, p5 C-6)
    assign oLINE_LOW     = RESET;
    assign oMA           = ma;
    assign oB_BREAK      = (state == S_BRK);                        //?? not sure if active high or low??
    assign o_BF_ENABLE   = ~ (state == S_BRK);                      // next memory cycle is S_BRK, use break frame (dma extended address bits) for next mem access (p5 D-2)
    assign o_BUSINIT     = ~ oBUSINIT;
    assign o_B_RUN       = ~ runff;
    assign o_DF_ENABLE   = ~ ((state == S_EXEC) & irusedf);         // next memory cycle uses DF (p22 C-7)
    assign o_KEY_DF      = ~ swDFLD;
    assign o_KEY_IF      = ~ swIFLD;
    assign o_KEY_LOAD    = ldad & tp1;                              // load address cycle (p33 C-2, not exact match)
    assign o_SP_CYC_NEXT = ~ ((state == S_WC) | (state == S_CA));   // next mem cycle is S_WC or S_CA, use field 0 (p22 C-6, p5 C-3, also p5 B-2)
    assign oE_SET_F_SET  = 0;                                       // B36-D2,p22 C-3,J12-72,,,
    assign o_KEY_CLEAR   = 1;                                       // B36-J1,p22 C-5,J12-68,,,

    assign lbAC   = acum;
    assign lbBRK  = (state == S_BRK);
    assign lbCA   = (state == S_CA);
    assign lbDEF  = (state == S_DEFER);
    assign lbEA   = ~ i_EA;
    assign lbEXE  = (state == S_EXEC);
    assign lbFET  = (state == S_FETCH);
    assign lbION  = intenabled;
    assign lbIR   = ir;
    assign lbLINK = link;
    assign lbMA   = ma;
    assign lbMB   = mb;
    assign lbRUN  = runff;
    assign lbWC   = (state == S_WC);

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

    always @(posedge CLOCK) begin
        if (RESET) begin
            debounce <= 0;
            hidestep <= 0;
            /***intdelayed <= 0;***/
            /***intenabled <= 0;***/
            lastts2  <= 0;
            lastts4  <= 0;
            ldad     <= 0;
            runff    <= 0;
            state    <= S_START;
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

            // if not running, process console switches
            if (~ runff) begin
                if (memstate == MS_IDLE) begin

                    // load address switch
                    if (debounce[23] & lastswLDAD & ~ swLDAD) begin
                        ldad  <= 1;
                        ma <= swSR;
                        /***pctr <= swSR;***/
                        memodify <= MM_NONE;
                        /***memstate <= MS_START;***/
                    end

                    // examine switch
                    else if (debounce[23] & lastswEXAM & ~ swEXAM) begin
                        ldad <= 0;
                        ma <= pctr;
                        /***pctr <= pctr + 1;***/
                        memodify <= MM_NONE;
                        /***memstate <= MS_START;***/
                    end

                    // deposit switch
                    else if (debounce[23] & lastswDEP & ~ swDEP) begin
                        ldad <= 0;
                        ma <= pctr;
                        /***pctr <= pctr + 1;***/
                        memodify <= MM_DEPOS;
                        /***memstate <= MS_START;***/
                    end

                    // continue switch
                    else if (debounce[23] & lastswCONT & ~ swCONT) begin
                        ldad     <= 0;
                        hidestep <= 1;
                        runff    <= 1;
                    end

                    // start switch
                    else if (debounce[23] & lastswSTART & ~ swSTART) begin
                        /***acum  <= 0;***/
                        hidestep   <= 0;
                        /***intdelayed <= 0;***/
                        /***intenabled <= 0;***/
                        ldad  <= 0;
                        link  <= 0;
                        runff <= 1;
                        state <= S_START;
                    end
                end
            end

            /////////////
            //  START  //
            /////////////

            // this state runs for one fpga clock cycle between end of last instruction and the next instruction
            // ... unless processor is halted in which case it runs until cont or start switch is pressed

            else if (state == S_START) begin

                // if there is a dma request pending, start doing the dma
                if (~ i_BRK_RQST) begin
                    ma <= ~ i_DMAADDR;
                    memodify <= MM_STATE;
                    /***memstate <= MS_START;***/
                    state <= iTHREECYCLE ? S_WC : S_BRK;
                end

                // maybe the stop switch is being pressed or step switch is on
                else if (swSTOP | swSTEP & ~ hidestep) begin
                    ma <= pctr;
                    runff <= 0;
                end

                // if interrupt request pending, start that going
                else if (~ i_INT_RQST & i_INT_INHIBIT & intenabled) begin
                    /***intdelayed <= 0;***/
                    /***intenabled <= 0;***/
                    ma         <= 0;
                    o_LOAD_SF  <= 0;
                    state      <= S_INTAK;
                end

                // none of the above, start a fetch going
                else begin
                    /***intenabled <= intdelayed;***/
                    ma         <= pctr;
                    memodify   <= MM_STATE;
                    /***memstate   <= MS_START;***/
                    /***pctr       <= pctr + 1;***/
                    state      <= S_FETCH;
                end
            end

            // artificial state made up to let LOAD_SF stay asserted for a while before starting memory cycle
            // this way the memory cycle that saves the old pc will write it to field 0
            // the real PDP-8/L does this in the previous instruction's ts4 (p9 C-3, p22 C-5)
            else if (state == S_INTAK) begin
                if (ma[3:0] == 15) begin
                    ir <= 4;                // set up JMS 0 instruction
                    irusedf <= 0;
                    ma <= 0;
                    memodify  <= MM_STATE;
                    /***memstate  <= MS_START;***/  // start a memory cycle
                    o_LOAD_SF <= 1;         // release the LOAD_SF line, should be in field 0 now
                    state     <= S_EXEC;    // do the execute portion of the JMS 0
                end else begin
                    ma <= ma + 1;           // let LOAD_SF linger a bit
                end
            end

            /////////////
            //  FETCH  //
            /////////////

            // just read opcode for fetch, save upper bits in ir
            else if (~ lastts2 & ts2 & (state == S_FETCH)) begin
                ir <= mb[11:09];
                irusedf <= ~ mb[11] & mb[08];
            end

            // end of fetch
            //  for and, tad, isz, dca, jms, set up ma and do defer or exec next
            //  for direct jmp, update the pc and start another fetch next
            //  for indirect jmp, set up ma and do defer next
            //  for iot, the io cycles were already done, so start another fetch next
            //  for opr, update accumulator and/or pc and start another fetch next
            else if (lastts4 & ~ ts4 & (state == S_FETCH)) begin
                case (ir)

                    // add, tad, isz, dca, jms
                    0, 1, 2, 3, 4: begin
                        ma <= effaddr;
                        memodify <= MM_STATE;
                        /***memstate <= MS_START;***/
                        state <= mb[08] ? S_DEFER : S_EXEC;
                    end

                    // jmp
                    5: begin
                        if (mb[08]) begin
                            ma <= effaddr;
                            memodify <= MM_STATE;
                            /***memstate <= MS_START;***/
                            state <= S_DEFER;
                        end else begin
                            /***pctr  <= effaddr;***/
                            state <= S_START;
                        end
                    end

                    // iot - i/o cycle was already performed by memory control so start another fetch
                    6: begin
                        state <= S_START;
                    end

                    // opr - perform operation and start another fetch
                    7: begin
                        if (~ mb[08]) begin
                            /***acum  <= g1acum;***/
                            link  <= g1link;
                        end else if (~ mb[00]) begin
                            /***if (g2skip) pctr <= pctr + 1;***/
                            /***acum  <= g2acum;***/
                            runff <= ~ mb[01];
                        end
                        state <= S_START;
                    end
                endcase
            end

            /////////////
            //  DEFER  //
            /////////////

            // end of defer
            else if (lastts4 & ~ ts4 & (state == S_DEFER)) begin
                if (ir == 5) begin
                    /***pctr     <= ma;***/
                    state    <= S_START;
                end else begin
                    memodify <= MM_STATE;
                    /***memstate <= MS_START;***/
                    state    <= S_EXEC;
                end
            end

            ////////////
            //  EXEC  //
            ////////////

            // end of exec
            else if (lastts4 & ~ ts4 & (state == S_EXEC)) begin
                case (ir)
                    /***0: acum <= acum & mb;***/                           // and
                    1: begin link <= tadlink; /***acum <= tadacum;***/ end  // tad
                    /***2: if (mb == 0) pctr <= pctr + 1;***/               // isz
                    /***3: acum <= 0;***/                                   // dca
                    /***4: pctr <= ma + 1;***/                              // jms
                endcase
                o_LOAD_SF <= 1;
                state <= S_START;
            end

            //////////////////
            //  WORD COUNT  //
            //////////////////

            // middle of wc with count just read, detect count overflow
            // mb is incremented via MM_STATE
            else if (~ lastts2 & ts2 & (state == S_WC)) begin
                oBWC_OVERFLOW <= (mb == 12'o7777);
                o_ADDR_ACCEPT <= 0;
            end

            // end of wordcount
            else if (lastts4 & ~ ts4 & (state == S_WC)) begin
                ma <= ma + 1;
                memodify <= MM_STATE;
                /***memstate <= MS_START;***/
                state <= S_CA;
            end

            ///////////////////////
            //  CURRENT ADDRESS  //
            ///////////////////////

            // end of currentaddress
            else if (lastts4 & ~ ts4 & (state == S_CA)) begin
                ma <= mb;               // set up possibly incremented value as address for dma transfer
                memodify <= MM_STATE;
                /***memstate <= MS_START;***/
                state <= S_BRK;
            end

            /////////////
            //  BREAK  //
            /////////////

            // middle of break with data just read,
            // tell controller we have received the address and have read data ready if it wants it
            else if (~ lastts2 & ts2 & (state == S_BRK)) begin
                o_ADDR_ACCEPT <= 0;
            end

            // end of break
            else if (lastts4 & ~ ts4 & (state == S_BRK)) begin
                o_ADDR_ACCEPT <= 1;
                state <= S_START;
            end
        end

        // debounce front-panel momentary switches
        if (~ (swCONT | swDEP | swEXAM | swLDAD | swSTART)) debounce <= 0;
        else if (~ debounce[23]) debounce <= debounce + 24'h400000;  // ~84mS

        // save these for transition testing
        lastts2 <= ts2;
        lastts4 <= ts4;
        lastswCONT  <= swCONT;
        lastswDEP   <= swDEP;
        lastswEXAM  <= swEXAM;
        lastswLDAD  <= swLDAD;
        lastswSTART <= swSTART;
    end

    // perform a memory cycle
    // trigger by setting memstate = MS_STSRT
    // completed when memstate == MS_IDLE
    // uses contents of MA for memory address
    // reads contents of memory into MB then sets ts2 = 1 via tp1
    // value in MB is then modified according to memodify then sets ts3 = 1 via tp2
    // value is written back to memory then memstate is set to MS_IDLE
    // simulate 1.6uS memory cycle by taking 160 clocks to complete
    // also sticks an io cycle in at the end if just fetched an io opcode

    reg[11:00] mbmod;
    always @(*) begin
        case (memodify)
            MM_DEPOS: mbmod = swSR;
            MM_STATE: case (state)
                S_DEFER: mbmod = mb + (ma[11:03] == 1);
                S_EXEC: case (ir)
                    2: mbmod = mb + 1;  // isz
                    3: mbmod = acum;    // dca
                    4: mbmod = pctr;    // jms
                    default: mbmod = mb;
                endcase
                S_WC:    mbmod = mb + 1;
                S_CA:    mbmod = mb + iCA_INCREMENT;
                S_BRK:   mbmod = (iDATA_IN ? ~ i_DMADATA : mb) + { 11'b0, iMEMINCR };
                default: mbmod = mb;
            endcase
            default: mbmod = mb;
        endcase
    end

    wire withio = (state == S_FETCH) & (mb[11:09] == 6);
    wire[11:00] ioac = (i_AC_CLEAR ? acum : 0) | iINPUTBUS;

    always @(posedge CLOCK) begin
        if (RESET) begin
            oMEMSTART <= 0;
            memstate  <= MS_IDLE;
        end

        else case (memstate)
            MS_IDLE: begin
                if (~ runff & debounce[23] & lastswLDAD & ~ swLDAD) memstate <= MS_START;
                if (~ runff & debounce[23] & lastswEXAM & ~ swEXAM) memstate <= MS_START;
                if (~ runff & debounce[23] & lastswDEP  & ~ swDEP)  memstate <= MS_START;
                if (runff & (state == S_START) & ~ i_BRK_RQST) memstate <= MS_START;
                if (runff & (state == S_START) & i_BRK_RQST & ~ (swSTOP | swSTEP & ~ hidestep) & ~ (~ i_INT_RQST & i_INT_INHIBIT & intenabled)) memstate <= MS_START;
                if (runff & (state == S_INTAK) & (ma[3:0] == 15)) memstate <= MS_START;
                if (runff & lastts4 & ~ ts4 & (state == S_FETCH) & (ir == 0)) memstate <= MS_START;
                if (runff & lastts4 & ~ ts4 & (state == S_FETCH) & (ir == 1)) memstate <= MS_START;
                if (runff & lastts4 & ~ ts4 & (state == S_FETCH) & (ir == 2)) memstate <= MS_START;
                if (runff & lastts4 & ~ ts4 & (state == S_FETCH) & (ir == 3)) memstate <= MS_START;
                if (runff & lastts4 & ~ ts4 & (state == S_FETCH) & (ir == 4)) memstate <= MS_START;
                if (runff & lastts4 & ~ ts4 & (state == S_FETCH) & (ir == 5) & mb[08]) memstate <= MS_START;
                if (runff & lastts4 & ~ ts4 & (state == S_DEFER) & (ir != 5)) memstate <= MS_START;
                if (runff & lastts4 & ~ ts4 & (state == S_WC)) memstate <= MS_START;
                if (runff & lastts4 & ~ ts4 & (state == S_CA)) memstate <= MS_START;
            end

            // time to start memory (and maybe io) cycle
            MS_START: begin
                memdelay <= 0;
                memstate <= i_EA ? MS_LOCAL : MS_EXTERN;
            end

            MS_LOCAL: begin
                case (memdelay)
                      0: begin oMEMSTART <= 1; /***ts1 <= 1;***/ end
                     15: oMEMSTART <= 0;
                     61: begin mb <= localcore[ma]; /***tp1 <= 1;***/ end
                     82: begin mb <= mbmod; /***tp2 <= 1;***/ end
                    110: begin localcore[ma] <= mb; /***tp3 <= 1;***/ end
                    112: if (withio) memstate <= MS_STARTIO;
                    158: begin /***tp4 <= 1;***/ memstate <= MS_FINISH; end
                endcase
                memdelay <= memdelay + 1;
            end

            // using external memory
            MS_EXTERN: begin

                // start out by setting MEMSTART and entering ts1
                if (memdelay == 0) begin
                    oMEMSTART <= 1;
                    /***ts1 <= 1;***/
                end

                // MEMSTART lasts 150nS
                else if (memdelay < 15) begin
                    memdelay <= memdelay + 1;
                end
                else if (memdelay == 15) begin
                    oMEMSTART <= 0;
                    memdelay <= memdelay + 1;
                end

                // wait for STROBE from external memory controller indicating read data is ready
                // this transitions us to ts2 via a tp1 pulse
                else if (memdelay == 16) begin
                    if (! i_STROBE) begin
                        /***tp1 <= 1;***/
                        mb <= iMEM;
                        memdelay <= memdelay + 1;
                    end
                end

                else if (memdelay == 17) begin
                    memdelay <= memdelay + 1;
                end

                else if (memdelay == 18) begin
                    if (ts2) begin
                        memdelay <= memdelay + 1;
                    end
                end

                // ts2 lasts 210nS to give mbmod time to work
                else if (memdelay < 37) begin
                    memdelay <= memdelay + 1;
                end
                else if (memdelay == 37) begin
                    mb <= mbmod;
                    /***tp2 <= 1;***/
                    memdelay <= memdelay + 1;
                end

                // external memory controller pulses MEMDONE when it has finished writing MB back to memory
                // this triggers end of ts3
                else if (memdelay == 38) begin
                    if (! i_MEMDONE) begin
                        /***tp3 <= 1;***/
                        memdelay <= memdelay + 1;
                    end
                end

                // at end of ts3, if there is io to do, go do it
                // otherwise prepare to signal end of ts4
                else if (memdelay == 39) begin
                    if (! ts3) begin
                        if (withio) memstate <= MS_STARTIO;
                        memdelay <= memdelay + 1;
                    end
                end

                // at end of ts4, go idle
                // otherwise prepare to signal end of ts4
                else if (memdelay < 87) begin
                    memdelay <= memdelay + 1;
                end
                else if (memdelay == 87) begin
                    /***tp4 <= 1;***/
                    memstate <= MS_FINISH;
                end
            end

            // do io cycles, then do a ts4 at the end, then go idle
            MS_STARTIO: begin
                memdelay <= 0;
                memstate <= MS_DOIO;
            end
            MS_DOIO: begin
                case (memdelay)
                     33: iop1 <= mb[00];
                     91: begin
                        /***if (iop1 & (mb[08:02] == 0)) intdelayed <= 1;***/
                        iop1 <= 0;
                        /***acum <= ioac;***/
                        /***if (~ i_IO_SKIP) pctr <= pctr + 1;***/
                    end
                    119: iop2 <= mb[01];
                    177: begin
                        if (iop2 & (mb[08:02] == 0)) begin /***intdelayed <= 0;***/ /***intenabled <= 0;***/ end
                        iop2 <= 0;
                        /***acum <= ioac;***/
                        /***if (~ i_IO_SKIP) pctr <= pctr + 1;***/
                    end
                    205: iop4 <= mb[02];
                    263: begin
                        iop4 <= 0;
                        /***acum <= ioac;***/
                        /***if (~ i_IO_SKIP) pctr <= pctr + 1;***/
                        /***ts4 <= 1;***/
                    end
                    303: begin /***tp4 <= 1;***/ memstate <= MS_FINISH; end
                endcase
                memdelay <= memdelay + 1;
            end

            // finish up by waiting for ts4 and tp4 to go low
            MS_FINISH: begin
                if (~ tp4 & ~ ts4) begin
                    memstate <= MS_IDLE;
                end
            end
        endcase
    end

    // all accumulator updates to make vivado happy
    always @(posedge CLOCK) begin

        // start switch clears accumulator
        if (~ RESET & ~ runff & (memstate == MS_IDLE) & debounce[23] & ~ swSTART) acum <= 0;

        // group 1 instruction
        if (~ RESET & runff & lastts4 & ~ ts4 & (memstate != MS_DOIO) & (state == S_FETCH) & (ir == 7) & ~ mb[08]) acum <= g1acum;

        // group 2 instruction
        if (~ RESET & runff & lastts4 & ~ ts4 & (memstate != MS_DOIO) & (state == S_FETCH) & (ir == 7) & mb[08] & ~ mb[00]) acum <= g2acum;

        // and instruction
        if (~ RESET & runff & lastts4 & ~ ts4 & (memstate != MS_DOIO) & (state == S_EXEC) & (ir == 0)) acum <= acum & mb;

        // tad instruction
        if (~ RESET & runff & lastts4 & ~ ts4 & (memstate != MS_DOIO) & (state == S_EXEC) & (ir == 1)) acum <= tadacum;

        // dca instruction
        if (~ RESET & runff & lastts4 & ~ ts4 & (memstate != MS_DOIO) & (state == S_EXEC) & (ir == 3)) acum <= 0;

        // io instruction
        if (~ RESET & runff & withio & (memstate == MS_DOIO) & (memdelay ==  91)) acum <= ioac;
        if (~ RESET & runff & withio & (memstate == MS_DOIO) & (memdelay == 177)) acum <= ioac;
        if (~ RESET & runff & withio & (memstate == MS_DOIO) & (memdelay == 263)) acum <= ioac;
    end

    // all updates to program counter to make vivado happy
    always @(posedge CLOCK) begin
             if (~ RESET & ~ runff & (memstate == MS_IDLE) & debounce[23] & lastswLDAD & ~ swLDAD) pctr <= swSR;
        else if (~ RESET & ~ runff & (memstate == MS_IDLE) & debounce[23] & lastswEXAM & ~ swEXAM) pctr <= pctr + 1;
        else if (~ RESET & ~ runff & (memstate == MS_IDLE) & debounce[23] & lastswDEP  & ~ swDEP)  pctr <= pctr + 1;
        if (~ RESET & runff & (state == S_START) & i_BRK_RQST & ~ (swSTOP | swSTEP & ~ hidestep) & ~ (~ i_INT_RQST & i_INT_INHIBIT & intenabled)) pctr <= pctr + 1;
        if (~ RESET & runff & lastts4 & ~ ts4 & (state == S_FETCH) & (ir == 5) & ~ mb[08]) pctr <= effaddr;
        if (~ RESET & runff & lastts4 & ~ ts4 & (state == S_FETCH) & (ir == 7) & mb[08] & g2skip) pctr <= pctr + 1;
        if (~ RESET & runff & lastts4 & ~ ts4 & (state == S_DEFER) & (ir == 5)) pctr <= ma;
        if (~ RESET & runff & lastts4 & ~ ts4 & (state == S_EXEC)  & (ir == 2) && (mb == 0)) pctr <= pctr + 1;
        if (~ RESET & runff & lastts4 & ~ ts4 & (state == S_EXEC)  & (ir == 4)) pctr <= ma + 1;
        if (~ RESET & runff & (state == S_FETCH) & (ir == 6) & (memstate == MS_DOIO) && (memdelay ==  91) & ~ i_IO_SKIP) pctr <= pctr + 1;
        if (~ RESET & runff & (state == S_FETCH) & (ir == 6) & (memstate == MS_DOIO) && (memdelay == 177) & ~ i_IO_SKIP) pctr <= pctr + 1;
        if (~ RESET & runff & (state == S_FETCH) & (ir == 6) & (memstate == MS_DOIO) && (memdelay == 263) & ~ i_IO_SKIP) pctr <= pctr + 1;
    end

    // all updates to intdelayed, intenabled to make vivado happy
    always @(posedge CLOCK) begin
        if (RESET) intdelayed <= 0;
        if (~ RESET & ~ runff & (memstate == MS_IDLE) & debounce[23] & lastswSTART & ~ swSTART) intdelayed <= 0;
        if (~ RESET & runff & (state == S_START) & i_BRK_RQST & ~ (swSTOP | swSTEP & ~ hidestep) & (~ i_INT_RQST & i_INT_INHIBIT & intenabled)) intdelayed <= 0;
        if (~ RESET & runff & (memstate == MS_DOIO) & (memdelay ==  91) & iop1 & (mb[08:02] == 0)) intdelayed <= 1;
        if (~ RESET & runff & (memstate == MS_DOIO) & (memdelay == 177) & iop2 & (mb[08:02] == 0)) intdelayed <= 0;

        if (RESET) intenabled <= 0;
        if (~ RESET & ~ runff & (memstate == MS_IDLE) & debounce[23] & lastswSTART & ~ swSTART) intenabled <= 0;
        if (~ RESET & runff & (state == S_START) & i_BRK_RQST & ~ (swSTOP | swSTEP & ~ hidestep) &   (~ i_INT_RQST & i_INT_INHIBIT & intenabled)) intenabled <= 0;
        if (~ RESET & runff & (state == S_START) & i_BRK_RQST & ~ (swSTOP | swSTEP & ~ hidestep) & ~ (~ i_INT_RQST & i_INT_INHIBIT & intenabled)) intenabled <= intdelayed;
        if (~ RESET & runff & (memstate == MS_DOIO) & (memdelay == 177) & iop2 & (mb[08:02] == 0)) intenabled <= 0;
    end

    // time state management
    // set tp1 near end of ts1, then this circuit will transition to ts2 then clear tp1
    // likewise for ts2, 3, 4, but do not enter ts4 is 'withio' is set

    localparam TS_IDLE    =  0;
    localparam TS_TS1BODY =  1;
    localparam TS_TP1BEG  =  2;
    localparam TS_TP1END  =  3;
    localparam TS_TS2BODY =  4;
    localparam TS_TP2BEG  =  5;
    localparam TS_TP2END  =  6;
    localparam TS_TS3BODY =  7;
    localparam TS_TP3BEG  =  8;
    localparam TS_TP3END  =  9;
    localparam TS_TS4BODY = 10;
    localparam TS_TP4BEG  = 11;
    localparam TS_TP4END  = 12;

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
        end else begin
            case (timestate)
                TS_IDLE: begin
                    if ((memstate == MS_LOCAL)  & (memdelay ==   0)) timestate <= TS_TS1BODY;
                    if ((memstate == MS_EXTERN) & (memdelay ==   0)) timestate <= TS_TS1BODY;
                    if ((memstate == MS_DOIO)   & (memdelay == 263)) timestate <= TS_TS4BODY;
                end

                TS_TS1BODY: begin
                    timedelay <= 0;
                    if ((memstate == MS_LOCAL)  & (memdelay ==  61)) timestate <= TS_TP1BEG;
                    if ((memstate == MS_EXTERN) & (memdelay ==  16)) timestate <= TS_TP1BEG;
                end

                TS_TP1BEG: begin
                    if (timedelay != 2) timedelay <= timedelay + 1;
                    else timestate <= TS_TP1END;
                end

                TS_TP1END: begin
                    if (timedelay != 5) timedelay <= timedelay + 1;
                    else timestate <= TS_TS2BODY;
                end

                TS_TS2BODY: begin
                    timedelay <= 0;
                    if ((memstate == MS_LOCAL)  & (memdelay ==  82)) timestate <= TS_TP2BEG;
                    if ((memstate == MS_EXTERN) & (memdelay ==  37)) timestate <= TS_TP2BEG;
                end

                TS_TP2BEG: begin
                    if (timedelay != 2) timedelay <= timedelay + 1;
                    else timestate <= TS_TP2END;
                end

                TS_TP2END: begin
                    if (timedelay != 5) timedelay <= timedelay + 1;
                    else timestate <= TS_TS3BODY;
                end

                TS_TS3BODY: begin
                    timedelay <= 0;
                    if ((memstate == MS_LOCAL)  & (memdelay == 110)) timestate <= TS_TP3BEG;
                    if ((memstate == MS_EXTERN) & (memdelay ==  38)) timestate <= TS_TP3BEG;
                end

                TS_TP3BEG: begin
                    if (timedelay != 2) timedelay <= timedelay + 1;
                    else timestate <= TS_TP3END;
                end

                TS_TP3END: begin
                    if (timedelay != 5) timedelay <= timedelay + 1;
                    else timestate <= withio ? TS_IDLE : TS_TS4BODY;
                end

                TS_TS4BODY: begin
                    timedelay <= 0;
                    if ((memstate == MS_LOCAL)  & (memdelay == 158)) timestate <= TS_TP4BEG;
                    if ((memstate == MS_EXTERN) & (memdelay ==  87)) timestate <= TS_TP4BEG;
                    if ((memstate == MS_DOIO)   & (memdelay == 303)) timestate <= TS_TP4BEG;
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
    end
endmodule
