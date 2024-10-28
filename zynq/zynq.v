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

module synk (input CLOCK, output reg q, input o);
    reg eo, p;
    always @(posedge CLOCK) begin
        if (eo) p <= o;
           else q <= p;
        eo <= ~ eo;
    end
endmodule

// main program for the zynq implementation

module Zynq (
    input  CLOCK,
    input  RESET_N,
    output LEDoutR,     // IO_B34_LN6 R14
    output LEDoutG,     // IO_B34_LP7 Y16
    output LEDoutB,     // IO_B34_LN7 Y17

    inout  bDMABUSA,
    inout  bDMABUSB,
    inout  bDMABUSC,
    inout  bDMABUSD,
    inout  bDMABUSE,
    inout  bDMABUSF,
    inout  bDMABUSH,
    inout  bDMABUSJ,
    inout  bDMABUSK,
    inout  bDMABUSL,
    inout  bDMABUSM,
    inout  bDMABUSN,
    inout  bMEMBUSA,
    inout  bMEMBUSB,
    inout  bMEMBUSC,
    inout  bMEMBUSD,
    inout  bMEMBUSE,
    inout  bMEMBUSF,
    inout  bMEMBUSH,
    inout  bMEMBUSJ,
    inout  bMEMBUSK,
    inout  bMEMBUSL,
    inout  bMEMBUSM,
    inout  bMEMBUSN,
    inout  bPIOBUSA,
    inout  bPIOBUSB,
    inout  bPIOBUSC,
    inout  bPIOBUSD,
    inout  bPIOBUSE,
    inout  bPIOBUSF,
    inout  bPIOBUSH,
    inout  bPIOBUSJ,
    inout  bPIOBUSK,
    inout  bPIOBUSL,
    inout  bPIOBUSM,
    inout  bPIOBUSN,
    output i_AC_CLEAR,
    output i_BRK_RQST,
    output i_EA,
    output i_EMA,
    output i_EXTDMAADD_12,
    output i_INT_INHIBIT,
    output i_INT_RQST,
    output i_IO_SKIP,
    output i_MEMDONE,
    output i_STROBE,
    output iB36V1,
    output iBEMA,
    output iCA_INCREMENT,
    output iD36B2,
    output iDATA_IN,
    output iMEM_07,
    output iMEMINCR,
    output iTHREECYCLE,
    input  o_ADDR_ACCEPT,
    input  o_B_RUN,
    input  o_BF_ENABLE,
    input  o_BUSINIT,
    input  o_DF_ENABLE,
    input  o_KEY_CLEAR,
    input  o_KEY_DF,
    input  o_KEY_IF,
    input  o_KEY_LOAD,
    input  o_LOAD_SF,
    input  o_SP_CYC_NEXT,
    input  oB_BREAK,
    input  oBIOP1,
    input  oBIOP2,
    input  oBIOP4,
    input  oBTP2,
    input  oBTP3,
    input  oBTS_1,
    input  oBTS_3,
    input  oBWC_OVERFLOW,
    input  oC36B2,
    input  oD35B2,
    input  oE_SET_F_SET,
    input  oJMP_JMS,
    input  oLINE_LOW,
    input  oMEMSTART,
    output reg r_BAC,
    output reg r_BMB,
    output reg r_MA,
    output reg x_DMAADDR,
    output reg x_DMADATA,
    output reg x_INPUTBUS,
    output reg x_MEM,

    output[14:00] xbraddr,
    output[11:00] xbrwdat,
    input[11:00] xbrrdat,
    output xbrenab,
    output xbrwena,

    // arm processor memory bus interface (AXI)
    // we are a slave for accessing the control registers (read and write)
    input[11:00]  saxi_ARADDR,
    output reg    saxi_ARREADY,
    input         saxi_ARVALID,
    input[11:00]  saxi_AWADDR,
    output reg    saxi_AWREADY,
    input         saxi_AWVALID,
    input         saxi_BREADY,
    output[1:0]   saxi_BRESP,
    output reg    saxi_BVALID,
    output[31:00] saxi_RDATA,
    input         saxi_RREADY,
    output[1:0]   saxi_RRESP,
    output reg    saxi_RVALID,
    input[31:00]  saxi_WDATA,
    output reg    saxi_WREADY,
    input         saxi_WVALID);

    // [31:16] = '8L'; [15:12] = (log2 len)-1; [11:00] = version
    localparam VERSION = 32'h384C4036;

    reg[11:02] readaddr, writeaddr;
    wire debounced, lastswLDAD;

    // pdp8/l module signals

    //      i... : signals going to hardware PDP-8/L
    //      o... : signals coming from hardware PDP-8/L

    //  sim_i... : signals going to simulated PDP-8/L (pdp8lsim.v)
    //  sim_o... : signals coming from simulated PDP-8/L (pdp8lsim.v)

    //  dev_i... : signals from io devices going to both i... and sim_i...
    //  dev_o... : selected from o... or sim_o... going to io devices

    wire[31:00] regctla, regctlb, regctlc, regctld, regctle, regctlf, regctlg, regctlh, regctli, regctlj, regctlk;

    wire sim_iBEMA;
    wire sim_iCA_INCREMENT;
    wire sim_iDATA_IN;
    wire[11:00] sim_iINPUTBUS;
    wire sim_iMEMINCR;
    wire[11:00] sim_iMEM;
    wire sim_iMEM_P;
    wire sim_iTHREECYCLE;
    wire sim_i_AC_CLEAR;
    wire sim_i_BRK_RQST;
    wire[11:00] sim_i_DMAADDR;
    wire[11:00] sim_i_DMADATA;
    wire sim_i_EA;
    wire sim_i_EMA;
    wire sim_i_INT_INHIBIT;
    wire sim_i_INT_RQST;
    wire sim_i_IO_SKIP;
    wire sim_i_MEMDONE;
    wire sim_i_STROBE;
    wire[11:00] sim_oBAC;
    wire sim_oBIOP1;
    wire sim_oBIOP2;
    wire sim_oBIOP4;
    wire[11:00] sim_oBMB;
    wire sim_oBTP2;
    wire sim_oBTP3;
    wire sim_oBTS_1;
    wire sim_oBTS_3;
    wire sim_oBWC_OVERFLOW;
    wire sim_oB_BREAK;
    wire sim_oE_SET_F_SET;
    wire sim_oJMP_JMS;
    wire sim_oLINE_LOW;
    wire[11:00] sim_oMA;
    wire sim_oMEMSTART;
    wire sim_o_ADDR_ACCEPT;
    wire sim_o_BF_ENABLE;
    wire sim_o_BUSINIT;
    wire sim_o_B_RUN;
    wire sim_o_DF_ENABLE;
    wire sim_o_KEY_CLEAR;
    wire sim_o_KEY_DF;
    wire sim_o_KEY_IF;
    wire sim_o_KEY_LOAD;
    wire sim_o_LOAD_SF;
    wire sim_o_SP_CYC_NEXT;

    wire[11:00] sim_lbAC;
    wire sim_lbBRK;
    wire sim_lbCA;
    wire sim_lbDEF;
    wire sim_lbEA;
    wire sim_lbEXE;
    wire sim_lbFET;
    wire sim_lbION;
    wire[2:0] sim_lbIR;
    wire sim_lbLINK;
    wire[11:00] sim_lbMA;
    wire[11:00] sim_lbMB;
    wire sim_lbRUN;
    wire sim_lbWC;

    wire dev_iBEMA;
    wire dev_iCA_INCREMENT;
    wire dev_iDATA_IN;
    wire[11:00] dev_iINPUTBUS;
    wire dev_iMEMINCR;
    wire[11:00] dev_iMEM;
    wire dev_iMEM_P;
    wire dev_iTHREECYCLE;
    wire dev_i_AC_CLEAR;
    wire dev_i_BRK_RQST;
    wire[11:00] dev_i_DMAADDR;
    wire[11:00] dev_i_DMADATA;
    wire dev_i_EA;
    wire dev_i_EMA;
    wire dev_i_INT_INHIBIT;
    wire dev_i_INT_RQST;
    wire dev_i_IO_SKIP;
    wire dev_i_MEMDONE;
    wire dev_i_STROBE;
    wire[11:00] dev_oBAC;
    wire dev_oBIOP1;
    wire dev_oBIOP2;
    wire dev_oBIOP4;
    wire[11:00] dev_oBMB;
    wire dev_oBTP2;
    wire dev_oBTP3;
    wire dev_oBTS_1;
    wire dev_oBTS_3;
    wire dev_oBWC_OVERFLOW;
    wire dev_oB_BREAK;
    wire dev_oE_SET_F_SET;
    wire dev_oJMP_JMS;
    wire dev_oLINE_LOW;
    wire[11:00] dev_oMA;
    wire dev_oMEMSTART;
    wire dev_o_ADDR_ACCEPT;
    wire dev_o_BF_ENABLE;
    wire dev_o_BUSINIT;
    wire dev_o_B_RUN;
    wire dev_o_DF_ENABLE;
    wire dev_o_KEY_CLEAR;
    wire dev_o_KEY_DF;
    wire dev_o_KEY_IF;
    wire dev_o_KEY_LOAD;
    wire dev_o_LOAD_SF;
    wire dev_o_SP_CYC_NEXT;

    wire[11:00] i_DMAADDR, i_DMADATA ;   // dma address, data going out to real PDP-8/L via our DMABUS
                                         // ...used to access real PDP-8/L 4K core memory
    wire[11:00] iINPUTBUS ;              // io data going out to real PDP-8/L INPUTBUS via our PIOBUS
    wire[11:00] iMEM   ;                 // extended memory data going to real PDP-8/L MEM via our MEMBUS
    wire iMEM_P ;                        // extended memory parity going to real PDP-8/L MEM_P via our MEMBUSA
    wire[11:00] oBAC   ;                 // real PDP-8/L AC contents, valid only during second half of io pulse
    reg[11:00] oBMB    ;                 // real PDP-8/L MB contents, theoretically always valid (latched during io pulse)
    wire[11:00] oMA    ;                 // real PDP-8/L MA contents, used by PDP-8/L to access extended memory

    reg simit;
    wire lastnanostep;
    reg nanocycle, nanostep, softreset;
    wire inuseclock, inusereset ;
    wire ioreset;
    wire iopstart, iopstop;
    wire firstiop1, firstiop2, firstiop4;
    wire acclr, intrq, ioskp ;
    reg[3:0] iopsetcount;               // count fpga cycles where an IOP is on
    reg[2:0] iopclrcount;               // count fpga cycles where no IOP is on

    // synchroniSed input wires
    wire q_ADDR_ACCEPT  ;
    wire q_B_RUN        ;
    wire q_BF_ENABLE    ;
    wire q_BUSINIT      ;
    wire q_DF_ENABLE    ;
    wire q_KEY_CLEAR    ;
    wire q_KEY_DF       ;
    wire q_KEY_IF       ;
    wire q_KEY_LOAD     ;
    wire q_LOAD_SF      ;
    wire q_SP_CYC_NEXT  ;
    wire qB_BREAK       ;
    wire qBIOP1         ;
    wire qBIOP2         ;
    wire qBIOP4         ;
    wire qBTP2          ;
    wire qBTP3          ;
    wire qBTS_1         ;
    wire qBTS_3         ;
    wire qBWC_OVERFLOW  ;
    wire qC36B2         ;
    wire qD35B2         ;
    wire qE_SET_F_SET   ;
    wire qJMP_JMS       ;
    wire qLINE_LOW      ;
    wire qMEMSTART      ;

    // arm interface wires;
    reg arm_iBEMA;
    reg arm_iCA_INCREMENT;
    reg arm_iDATA_IN;
    reg arm_iMEMINCR;
    reg arm_iMEM_P;
    reg arm_iTHREECYCLE;
    reg arm_i_AC_CLEAR;
    reg arm_i_BRK_RQST;
    reg arm_i_EA;
    reg arm_i_EMA;
    reg arm_i_INT_INHIBIT;
    reg arm_i_INT_RQST;
    reg arm_i_IO_SKIP;
    reg arm_i_MEMDONE;
    reg arm_i_STROBE;
    reg arm_swCONT;
    reg arm_swDEP;
    reg arm_swDFLD;
    reg arm_swEXAM;
    reg arm_swIFLD;
    reg arm_swLDAD;
    reg arm_swMPRT;
    reg arm_swSTEP;
    reg arm_swSTOP;
    reg arm_swSTART;
    reg[11:00] arm_iINPUTBUS;
    reg[11:00] arm_iMEM;
    reg[11:00] arm_i_DMAADDR;
    reg[11:00] arm_i_DMADATA;
    reg[11:00] arm_swSR;

    // tty interface wires
    wire[31:00] ttardata ;
    wire[11:00] ttibus ;
    wire ttacclr, ttintrq, ttioskp;
    wire[31:00] tt40ardata ;
    wire[11:00] tt40ibus ;
    wire tt40acclr, tt40intrq, tt40ioskp;

    // disk interface wires
    wire[31:00] rkardata ;
    wire[11:00] rkibus ;
    wire rkacclr, rkintrq, rkioskp;

    // extended memory interface wires
    wire[31:00] xmardata;
    wire[11:00] xmibus;
    wire[11:00] xmmem;
    wire xm_intinh, xm_mrdone, xm_mwdone, xm_ea;

    // bus values that are constants
    assign saxi_BRESP = 0;  // A3.4.4/A10.3 transfer OK
    assign saxi_RRESP = 0;  // A3.4.4/A10.3 transfer OK

    /////////////////////////////////////
    // send register being read to ARM //
    /////////////////////////////////////

    assign saxi_RDATA =
        (readaddr        == 10'b0000000000) ? VERSION    :  // 00000xxxxx00
        (readaddr        == 10'b0000000001) ? regctla    :
        (readaddr        == 10'b0000000010) ? regctlb    :
        (readaddr        == 10'b0000000011) ? regctlc    :
        (readaddr        == 10'b0000000100) ? regctld    :
        (readaddr        == 10'b0000000101) ? regctle    :
        (readaddr        == 10'b0000000110) ? regctlf    :
        (readaddr        == 10'b0000000111) ? regctlg    :
        (readaddr        == 10'b0000001000) ? regctlh    :
        (readaddr        == 10'b0000001001) ? regctli    :
        (readaddr        == 10'b0000001010) ? regctlj    :
        (readaddr        == 10'b0000001011) ? regctlk    :
        (readaddr[11:05] ==  7'b0000100)    ? rkardata   :  // 0000100xxx00
        (readaddr[11:04] ==  8'b00001010)   ? ttardata   :  // 00001010xx00
        (readaddr[11:04] ==  8'b00001011)   ? tt40ardata :  // 00001011xx00
        (readaddr[11:04] ==  8'b00001100)   ? xmardata   :  // 00001100xx00
        32'hDEADBEEF;

    wire armwrite   = saxi_WREADY & saxi_WVALID;    // arm is writing a register (single fpga clock cycle)
    wire rkawrite   = armwrite & writeaddr[11:05] == 7'b0000100;      // 0000100xxx00
    wire ttawrite   = armwrite & writeaddr[11:04] == 8'b00001010;     // 00001010xx00
    wire tt40awrite = armwrite & writeaddr[11:04] == 8'b00001011;     // 00001011xx00
    wire xmawrite   = armwrite & writeaddr[11:04] == 8'b00001100;     // 00001100xx00

    // A3.3.1 Read transaction dependencies
    // A3.3.1 Write transaction dependencies
    //        AXI4 write response dependency
    always @(posedge CLOCK) begin
        if (~ RESET_N) begin
            saxi_ARREADY <= 1;                             // we are ready to accept read address
            saxi_RVALID  <= 0;                             // we are not sending out read data

            saxi_AWREADY <= 1;                             // we are ready to accept write address
            saxi_WREADY  <= 0;                             // we are not ready to accept write data
            saxi_BVALID  <= 0;                             // we are not acknowledging any write

            nanocycle <= 1;                               // by default, software provides clock
            nanostep  <= 0;                               // by default, software clock is low
            softreset <= 1;                               // by default, software reset asserted
        end else begin

            /////////////////////
            //  register read  //
            /////////////////////

            // check for PS sending us a read address
            if (saxi_ARREADY & saxi_ARVALID) begin
                readaddr <= saxi_ARADDR[11:02];             // save address bits we care about
                saxi_ARREADY <= 0;                         // we are no longer accepting a read address
                saxi_RVALID <= 1;                          // we are sending out the corresponding data

            // check for PS acknowledging receipt of data
            end else if (saxi_RVALID & saxi_RREADY) begin
                saxi_ARREADY <= 1;                         // we are ready to accept an address again
                saxi_RVALID <= 0;                          // we are no longer sending out data
            end

            //////////////////////
            //  register write  //
            //////////////////////

            // check for PS sending us write data
            if (saxi_WREADY & saxi_WVALID) begin
                case (writeaddr)                            // write data to register
                     10'b0000000001: begin
                        arm_iBEMA         <= saxi_WDATA[00];
                        arm_iCA_INCREMENT <= saxi_WDATA[01];
                        arm_iDATA_IN      <= saxi_WDATA[02];
                        arm_iMEMINCR      <= saxi_WDATA[03];
                        arm_iMEM_P        <= saxi_WDATA[04];
                        arm_iTHREECYCLE   <= saxi_WDATA[05];
                        arm_i_AC_CLEAR    <= saxi_WDATA[06];
                        arm_i_BRK_RQST    <= saxi_WDATA[07];
                        arm_i_EA          <= saxi_WDATA[08];
                        arm_i_EMA         <= saxi_WDATA[09];
                        arm_i_INT_INHIBIT <= saxi_WDATA[10];
                        arm_i_INT_RQST    <= saxi_WDATA[11];
                        arm_i_IO_SKIP     <= saxi_WDATA[12];
                        arm_i_MEMDONE     <= saxi_WDATA[13];
                        arm_i_STROBE      <= saxi_WDATA[14];
                        simit             <= saxi_WDATA[27];
                        softreset         <= saxi_WDATA[29];
                        nanostep          <= saxi_WDATA[30];
                        nanocycle         <= saxi_WDATA[31];
                    end
                    10'b0000000010: begin
                        arm_swCONT        <= saxi_WDATA[00];
                        arm_swDEP         <= saxi_WDATA[01];
                        arm_swDFLD        <= saxi_WDATA[02];
                        arm_swEXAM        <= saxi_WDATA[03];
                        arm_swIFLD        <= saxi_WDATA[04];
                        arm_swLDAD        <= saxi_WDATA[05];
                        arm_swMPRT        <= saxi_WDATA[06];
                        arm_swSTEP        <= saxi_WDATA[07];
                        arm_swSTOP        <= saxi_WDATA[08];
                        arm_swSTART       <= saxi_WDATA[09];
                    end

                    10'b0000000011: begin
                        arm_iINPUTBUS <= saxi_WDATA[11:00];
                        arm_iMEM      <= saxi_WDATA[27:16];
                    end

                    10'b0000000100: begin
                        arm_i_DMAADDR <= saxi_WDATA[11:00];
                        arm_i_DMADATA <= saxi_WDATA[27:16];
                    end

                    10'b0000000101: begin
                        arm_swSR      <= saxi_WDATA[11:00];
                    end
                endcase
                saxi_AWREADY <= 1;                           // we are ready to accept an address again
                saxi_WREADY  <= 0;                           // we are no longer accepting write data
                saxi_BVALID  <= 1;                           // we have accepted the data

            end else begin
                // check for PS sending us a write address
                if (saxi_AWREADY & saxi_AWVALID) begin
                    writeaddr <= saxi_AWADDR[11:02];        // save address bits we care about
                    saxi_AWREADY <= 0;                       // we are no longer accepting a write address
                    saxi_WREADY  <= 1;                       // we are ready to accept write data
                end

                // check for PS acknowledging write acceptance
                if (saxi_BVALID & saxi_BREADY) begin
                    saxi_BVALID <= 0;
                end
            end
        end
    end

    ////////////////////////////////////////////////
    //  synchronize signals output by the PDP-8L  //
    ////////////////////////////////////////////////

    synk synkaa (CLOCK, q_ADDR_ACCEPT, o_ADDR_ACCEPT);
    synk synkbr (CLOCK, q_B_RUN,       o_B_RUN);
    synk synkbe (CLOCK, q_BF_ENABLE,   o_BF_ENABLE);
    synk synkbi (CLOCK, q_BUSINIT,     o_BUSINIT);
    synk synkde (CLOCK, q_DF_ENABLE,   o_DF_ENABLE);
    synk synkkc (CLOCK, q_KEY_CLEAR,   o_KEY_CLEAR);
    synk synkkd (CLOCK, q_KEY_DF,      o_KEY_DF);
    synk synkki (CLOCK, q_KEY_IF,      o_KEY_IF);
    synk synkkl (CLOCK, q_KEY_LOAD,    o_KEY_LOAD);
    synk synkls (CLOCK, q_LOAD_SF,     o_LOAD_SF);
    synk synksc (CLOCK, q_SP_CYC_NEXT, o_SP_CYC_NEXT);
    synk synkbb (CLOCK, qB_BREAK,      oB_BREAK);
    synk synkp1 (CLOCK, qBIOP1,        oBIOP1);
    synk synkp2 (CLOCK, qBIOP2,        oBIOP2);
    synk synkp4 (CLOCK, qBIOP4,        oBIOP4);
    synk synkt2 (CLOCK, qBTP2,         oBTP2);
    synk synkp3 (CLOCK, qBTP3,         oBTP3);
    synk synkt1 (CLOCK, qBTS_1,        oBTS_1);
    synk synkt3 (CLOCK, qBTS_3,        oBTS_3);
    synk synkbo (CLOCK, qBWC_OVERFLOW, oBWC_OVERFLOW);
    synk synkc3 (CLOCK, qC36B2,        oC36B2);
    synk synkd3 (CLOCK, qD35B2,        oD35B2);
    synk synkef (CLOCK, qE_SET_F_SET,  oE_SET_F_SET);
    synk synkjj (CLOCK, qJMP_JMS,      oJMP_JMS);
    synk synkll (CLOCK, qLINE_LOW,     oLINE_LOW);
    synk synkms (CLOCK, qMEMSTART,     oMEMSTART);

    /////////////////////////////////////////////////////
    //  select between simulated and hardware PDP-8/L  //
    /////////////////////////////////////////////////////

    // signals coming from arm registers : arm_i*
    // signals coming from devices       : dev_i*
    // signals coming from real PDP      : o*
    // signals coming from simulator     : sim_o*

    // signals going to devices   (dev_o*) = simit ? sim_o* : o*
    // signals going to simulator (sim_i*) = simit ? dev_i* : 0
    // signals going to real PDP  (i*)     = simit ? 0 : dev_i*

    /*
                        +------------+
    (pdp     o* )  -->  | D0       Q |  -->  ( dev_o*  devices)
                        |     mux    |
    (sim sim_o* )  -->  | D1       S |<--simit
                        +------------+

    (arm regs arm_i* )  -->
             (devices)  -->  wired-or-busses  -->  dev_i*  -->  gated with  simit  -->  ( sim_i*  sim)
                                                           -->  gated with ~simit  -->  (     i*  pdp)
    */

    // when simulating, send signals from devices on to the simulated PDP-8/L
    // when not simming, send signals from devices on to the hardware PDP-8/L

    assign sim_iBEMA          = simit ? dev_iBEMA          : 0;
    assign sim_iCA_INCREMENT  = simit ? dev_iCA_INCREMENT  : 0;
    assign sim_iDATA_IN       = simit ? dev_iDATA_IN       : 0;
    assign sim_iINPUTBUS      = simit ? dev_iINPUTBUS      : 0;
    assign sim_iMEMINCR       = simit ? dev_iMEMINCR       : 0;
    assign sim_iMEM           = simit ? dev_iMEM           : 0;
    assign sim_iMEM_P         = simit ? dev_iMEM_P         : 0;
    assign sim_iTHREECYCLE    = simit ? dev_iTHREECYCLE    : 0;
    assign sim_i_AC_CLEAR     = simit ? dev_i_AC_CLEAR     : 1;
    assign sim_i_BRK_RQST     = simit ? dev_i_BRK_RQST     : 1;
    assign sim_i_DMAADDR      = simit ? dev_i_DMAADDR      : 12'hFFF;
    assign sim_i_DMADATA      = simit ? dev_i_DMADATA      : 12'hFFF;
    assign sim_i_EA           = simit ? dev_i_EA           : 1;
    assign sim_i_EMA          = simit ? dev_i_EMA          : 1;
    assign sim_i_INT_INHIBIT  = simit ? dev_i_INT_INHIBIT  : 1;
    assign sim_i_INT_RQST     = simit ? dev_i_INT_RQST     : 1;
    assign sim_i_IO_SKIP      = simit ? dev_i_IO_SKIP      : 1;
    assign sim_i_MEMDONE      = simit ? dev_i_MEMDONE      : 1;
    assign sim_i_STROBE       = simit ? dev_i_STROBE       : 1;

    assign     iBEMA          = simit ? 0       : dev_iBEMA;
    assign     iCA_INCREMENT  = simit ? 0       : dev_iCA_INCREMENT;
    assign     iDATA_IN       = simit ? 0       : dev_iDATA_IN;
    assign     iINPUTBUS      = simit ? 0       : dev_iINPUTBUS;
    assign     iMEMINCR       = simit ? 0       : dev_iMEMINCR;
    assign     iMEM           = simit ? 0       : dev_iMEM;
    assign     iMEM_P         = simit ? 0       : dev_iMEM_P;
    assign     iTHREECYCLE    = simit ? 0       : dev_iTHREECYCLE;
    assign     i_AC_CLEAR     = simit ? 1       : dev_i_AC_CLEAR;
    assign     i_BRK_RQST     = simit ? 1       : dev_i_BRK_RQST;
    assign     i_DMAADDR      = simit ? 12'hFFF : dev_i_DMAADDR;
    assign     i_DMADATA      = simit ? 12'hFFF : dev_i_DMADATA;
    assign     i_EA           = simit ? 1       : dev_i_EA;
    assign     i_EMA          = simit ? 1       : dev_i_EMA;
    assign     i_INT_INHIBIT  = simit ? 1       : dev_i_INT_INHIBIT;
    assign     i_INT_RQST     = simit ? 1       : dev_i_INT_RQST;
    assign     i_IO_SKIP      = simit ? 1       : dev_i_IO_SKIP;
    assign     i_MEMDONE      = simit ? 1       : dev_i_MEMDONE;
    assign     i_STROBE       = simit ? 1       : dev_i_STROBE;

    // when simulating, send signals from the simulated PDP-8/L on to the devices
    // when not simming, send signals from the hardware PDP-8/L on to the devices

    assign dev_oBAC           = simit ? sim_oBAC           : oBAC;
    assign dev_oBIOP1         = simit ? sim_oBIOP1         : qBIOP1;
    assign dev_oBIOP2         = simit ? sim_oBIOP2         : qBIOP2;
    assign dev_oBIOP4         = simit ? sim_oBIOP4         : qBIOP4;
    assign dev_oBMB           = simit ? sim_oBMB           : oBMB;
    assign dev_oBTP2          = simit ? sim_oBTP2          : qBTP2;
    assign dev_oBTP3          = simit ? sim_oBTP3          : qBTP3;
    assign dev_oBTS_1         = simit ? sim_oBTS_1         : qBTS_1;
    assign dev_oBTS_3         = simit ? sim_oBTS_3         : qBTS_3;
    assign dev_oBWC_OVERFLOW  = simit ? sim_oBWC_OVERFLOW  : qBWC_OVERFLOW;
    assign dev_oB_BREAK       = simit ? sim_oB_BREAK       : qB_BREAK;
    assign dev_oE_SET_F_SET   = simit ? sim_oE_SET_F_SET   : qE_SET_F_SET;
    assign dev_oJMP_JMS       = simit ? sim_oJMP_JMS       : qJMP_JMS;
    assign dev_oLINE_LOW      = simit ? sim_oLINE_LOW      : qLINE_LOW;
    assign dev_oMA            = simit ? sim_oMA            : oMA;
    assign dev_oMEMSTART      = simit ? sim_oMEMSTART      : qMEMSTART;
    assign dev_o_ADDR_ACCEPT  = simit ? sim_o_ADDR_ACCEPT  : q_ADDR_ACCEPT;
    assign dev_o_BF_ENABLE    = simit ? sim_o_BF_ENABLE    : q_BF_ENABLE;
    assign dev_o_BUSINIT      = simit ? sim_o_BUSINIT      : q_BUSINIT;
    assign dev_o_B_RUN        = simit ? sim_o_B_RUN        : q_B_RUN;
    assign dev_o_DF_ENABLE    = simit ? sim_o_DF_ENABLE    : q_DF_ENABLE;
    assign dev_o_KEY_CLEAR    = simit ? sim_o_KEY_CLEAR    : q_KEY_CLEAR;
    assign dev_o_KEY_DF       = simit ? sim_o_KEY_DF       : q_KEY_DF;
    assign dev_o_KEY_IF       = simit ? sim_o_KEY_IF       : q_KEY_IF;
    assign dev_o_KEY_LOAD     = simit ? sim_o_KEY_LOAD     : q_KEY_LOAD;
    assign dev_o_LOAD_SF      = simit ? sim_o_LOAD_SF      : q_LOAD_SF;
    assign dev_o_SP_CYC_NEXT  = simit ? sim_o_SP_CYC_NEXT  : q_SP_CYC_NEXT;

    // reading arm registers gets device bus signals
    //  for 'i' signals: wired-or of all devices and what was written to arm registers
    //  for 'o' signals: selected signal from simulated PDP-8/L or real PDP-8/L

    assign regctla[00] = dev_iBEMA;
    assign regctla[01] = dev_iCA_INCREMENT;
    assign regctla[02] = dev_iDATA_IN;
    assign regctla[03] = dev_iMEMINCR;
    assign regctla[04] = dev_iMEM_P;
    assign regctla[05] = dev_iTHREECYCLE;
    assign regctla[06] = dev_i_AC_CLEAR;
    assign regctla[07] = dev_i_BRK_RQST;
    assign regctla[08] = dev_i_EA;
    assign regctla[09] = dev_i_EMA;
    assign regctla[10] = dev_i_INT_INHIBIT;
    assign regctla[11] = dev_i_INT_RQST;
    assign regctla[12] = dev_i_IO_SKIP;
    assign regctla[13] = dev_i_MEMDONE;
    assign regctla[14] = dev_i_STROBE;
    assign regctla[26:15] = 0;
    assign regctla[27] = simit;
    assign regctla[28] = 0;
    assign regctla[29] = softreset;
    assign regctla[30] = nanostep;
    assign regctla[31] = nanocycle;

    assign regctlb[00] = arm_swCONT;
    assign regctlb[01] = arm_swDEP;
    assign regctlb[02] = arm_swDFLD;
    assign regctlb[03] = arm_swEXAM;
    assign regctlb[04] = arm_swIFLD;
    assign regctlb[05] = arm_swLDAD;
    assign regctlb[06] = arm_swMPRT;
    assign regctlb[07] = arm_swSTEP;
    assign regctlb[08] = arm_swSTOP;
    assign regctlb[09] = arm_swSTART;
    assign regctlb[31:10] = 0;

    assign regctlc[15:00] = { 4'b0, dev_iINPUTBUS };
    assign regctlc[31:16] = { 4'b0, dev_iMEM      };
    assign regctld[15:00] = { 4'b0, dev_i_DMAADDR };
    assign regctld[31:16] = { 4'b0, dev_i_DMADATA };
    assign regctle[15:00] = { 4'b0, arm_swSR      };
    assign regctle[31:16] = 0;

    assign regctlf[00] = dev_oBIOP1;
    assign regctlf[01] = dev_oBIOP2;
    assign regctlf[02] = dev_oBIOP4;
    assign regctlf[03] = dev_oBTP2;
    assign regctlf[04] = dev_oBTP3;
    assign regctlf[05] = dev_oBTS_1;
    assign regctlf[06] = dev_oBTS_3;
    assign regctlf[07] = 0;
    assign regctlf[08] = dev_oBWC_OVERFLOW;
    assign regctlf[09] = dev_oB_BREAK;
    assign regctlf[10] = dev_oE_SET_F_SET;
    assign regctlf[11] = dev_oJMP_JMS;
    assign regctlf[12] = dev_oLINE_LOW;
    assign regctlf[13] = dev_oMEMSTART;
    assign regctlf[14] = dev_o_ADDR_ACCEPT;
    assign regctlf[15] = dev_o_BF_ENABLE;
    assign regctlf[16] = dev_o_BUSINIT;
    assign regctlf[17] = dev_o_B_RUN;
    assign regctlf[18] = dev_o_DF_ENABLE;
    assign regctlf[19] = dev_o_KEY_CLEAR;
    assign regctlf[20] = dev_o_KEY_DF;
    assign regctlf[21] = dev_o_KEY_IF;
    assign regctlf[22] = dev_o_KEY_LOAD;
    assign regctlf[23] = dev_o_LOAD_SF;
    assign regctlf[24] = dev_o_SP_CYC_NEXT;
    assign regctlf[31:25] = 0;

    assign regctlg[00] = sim_lbBRK;
    assign regctlg[01] = sim_lbCA;
    assign regctlg[02] = sim_lbDEF;
    assign regctlg[03] = sim_lbEA;
    assign regctlg[04] = sim_lbEXE;
    assign regctlg[05] = sim_lbFET;
    assign regctlg[06] = sim_lbION;
    assign regctlg[07] = sim_lbLINK;
    assign regctlg[08] = sim_lbRUN;
    assign regctlg[09] = sim_lbWC;
    assign regctlg[10] = debounced;
    assign regctlg[11] = lastswLDAD;
    assign regctlg[15:12] = 0;
    assign regctlg[27:16] = { sim_lbIR, 9'b000000000 };
    assign regctlg[31:28] = 0;

    assign regctlh[15:00] = { 4'b0, dev_oBAC };
    assign regctlh[31:16] = { 4'b0, dev_oBMB };
    assign regctli[15:00] = { 4'b0, dev_oMA  };
    assign regctli[31:16] = { 4'b0, sim_lbAC };
    assign regctlj[15:00] = { 4'b0, sim_lbMA };
    assign regctlj[31:16] = { 4'b0, sim_lbMB };

    ///////////////////////////////////
    //  simulated PDP-8/L processor  //
    ///////////////////////////////////

    assign inuseclock = nanocycle ? nanostep : CLOCK;
    assign inusereset = softreset | ~ RESET_N;

    assign LEDoutR = ~ inusereset;
    assign LEDoutG = ~ inuseclock;
    assign LEDoutB = ~ nanocycle;

    pdp8lsim siminst (
        .CLOCK         (CLOCK),
        .RESET         (inusereset),
        .iBEMA         (sim_iBEMA),
        .iCA_INCREMENT (sim_iCA_INCREMENT),
        .iDATA_IN      (sim_iDATA_IN),
        .iINPUTBUS     (sim_iINPUTBUS),
        .iMEMINCR      (sim_iMEMINCR),
        .iMEM          (sim_iMEM),
        .iMEM_P        (sim_iMEM_P),
        .iTHREECYCLE   (sim_iTHREECYCLE),
        .i_AC_CLEAR    (sim_i_AC_CLEAR),
        .i_BRK_RQST    (sim_i_BRK_RQST),
        .i_DMAADDR     (sim_i_DMAADDR),
        .i_DMADATA     (sim_i_DMADATA),
        .i_EA          (sim_i_EA),
        .i_EMA         (sim_i_EMA),
        .i_INT_INHIBIT (sim_i_INT_INHIBIT),
        .i_INT_RQST    (sim_i_INT_RQST),
        .i_IO_SKIP     (sim_i_IO_SKIP),
        .i_MEMDONE     (sim_i_MEMDONE),
        .i_STROBE      (sim_i_STROBE),
        .oBAC          (sim_oBAC),
        .oBIOP1        (sim_oBIOP1),
        .oBIOP2        (sim_oBIOP2),
        .oBIOP4        (sim_oBIOP4),
        .oBMB          (sim_oBMB),
        .oBTP2         (sim_oBTP2),
        .oBTP3         (sim_oBTP3),
        .oBTS_1        (sim_oBTS_1),
        .oBTS_3        (sim_oBTS_3),
        .oBWC_OVERFLOW (sim_oBWC_OVERFLOW),
        .oB_BREAK      (sim_oB_BREAK),
        .oE_SET_F_SET  (sim_oE_SET_F_SET),
        .oJMP_JMS      (sim_oJMP_JMS),
        .oLINE_LOW     (sim_oLINE_LOW),
        .oMA           (sim_oMA),
        .oMEMSTART     (sim_oMEMSTART),
        .o_ADDR_ACCEPT (sim_o_ADDR_ACCEPT),
        .o_BF_ENABLE   (sim_o_BF_ENABLE),
        .o_BUSINIT     (sim_o_BUSINIT),
        .o_B_RUN       (sim_o_B_RUN),
        .o_DF_ENABLE   (sim_o_DF_ENABLE),
        .o_KEY_CLEAR   (sim_o_KEY_CLEAR),
        .o_KEY_DF      (sim_o_KEY_DF),
        .o_KEY_IF      (sim_o_KEY_IF),
        .o_KEY_LOAD    (sim_o_KEY_LOAD),
        .o_LOAD_SF     (sim_o_LOAD_SF),
        .o_SP_CYC_NEXT (sim_o_SP_CYC_NEXT),
        .lbAC          (sim_lbAC),
        .lbBRK         (sim_lbBRK),
        .lbCA          (sim_lbCA),
        .lbDEF         (sim_lbDEF),
        .lbEA          (sim_lbEA),
        .lbEXE         (sim_lbEXE),
        .lbFET         (sim_lbFET),
        .lbION         (sim_lbION),
        .lbIR          (sim_lbIR),
        .lbLINK        (sim_lbLINK),
        .lbMA          (sim_lbMA),
        .lbMB          (sim_lbMB),
        .lbRUN         (sim_lbRUN),
        .lbWC          (sim_lbWC),
        .swCONT        (arm_swCONT),
        .swDEP         (arm_swDEP),
        .swDFLD        (arm_swDFLD),
        .swEXAM        (arm_swEXAM),
        .swIFLD        (arm_swIFLD),
        .swLDAD        (arm_swLDAD),
        .swMPRT        (arm_swMPRT),
        .swSTEP        (arm_swSTEP),
        .swSTOP        (arm_swSTOP),
        .swSR          (arm_swSR),
        .swSTART       (arm_swSTART),

        .majstate      (regctlk[02:00]),
        .timedelay     (regctlk[08:03]),
        .timestate     (regctlk[13:09]),
        .cyclectr      (regctlk[23:14]),
        .debounced     (debounced),
        .lastswLDAD    (lastswLDAD),
        .nanocycle     (nanocycle),
        .nanostep      (nanostep),
        .lastnanostep  (lastnanostep)
    );

    assign regctlk[31:29] = 0;

    /////////////////////
    //  io interfaces  //
    /////////////////////

    assign ioreset = inusereset | ~ dev_o_BUSINIT;     // reset io devices

    // generate iopstart pulse for an io instruction followed by iopstop
    //  iopstart is pulsed 130nS after the first iop for an instruction and lasts a single fpga clock cycle
    //   it is delayed if armwrite is happening at the same time
    //   interfaces know they can decode the io opcode in dev_oBMB and drive the busses
    //  iopstop is turned on 70nS after the end of that same iop and may last a long time
    //   interfaces must stop driving busses at this time
    //   it may happen same time as armwrite but since it lasts a long time, it will still be seen

    // interfaces are assumed to have this form:
    //   if ioreset do ioreset processing
    //   else if armwrite do armwrite processing
    //   else if iopstart do iopstart processing
    //   else if iopstop do iopstop processing

    assign firstiop1 = dev_oBIOP1 & dev_oBMB[0]   ==   1'b1;    // any 110xxxxxxxx1 executes only on IOP1
    assign firstiop2 = dev_oBIOP2 & dev_oBMB[1:0] ==  2'b10;    // any 110xxxxxxx10 executes only on IOP2
    assign firstiop4 = dev_oBIOP4 & dev_oBMB[2:0] == 3'b100;    // any 110xxxxxx100 executes only on IOP4
                                                                // any 110xxxxxx000 never executes

    always @(posedge CLOCK) begin
        if (ioreset) begin
            iopsetcount <= 0;
            iopclrcount <= 7;
        end else if (firstiop1 | firstiop2 | firstiop4) begin
            // somewhere inside the first IOP for an instruction
            // 130nS into it, blip a iopstart pulse for one fpga clock
            //   but hold it off if there would be an armwrite at same time
            iopclrcount <= 0;
            if ((iopsetcount < 13) | ((iopsetcount == 13) & ~ armwrite) | (iopsetcount == 14)) begin
                iopsetcount <= iopsetcount + 1;
            end
        end else begin
            // somewhere outside the first IOPn for an instruction
            // 70nS into it, raise and hold the stop signal until next iopulse
            iopsetcount <= 0;
            if (iopclrcount < 7) begin
                iopclrcount <= iopclrcount + 1;
            end
        end
    end

    assign iopstart = iopsetcount == 13 & ~ armwrite;       // IOP started 130nS ago, process the opcode in dev_oBMB, start driving busses
    assign iopstop  = iopclrcount ==  7;                    // IOP finished 70nS ago, stop driving io busses

    // PIOBUS control
    //   outside io pulses, PIOBUS is gating MB contents into us
    //   during first half of io pulse, PIOBUS is gating AC contents into us
    //   during second half of io pulse and for 40nS after, PIOBUS is gating INPUTBUS out to PDP-8/L

    // receive AC contents from PIOBUS, but it is valid only during first half of io pulse (ie, when r_BAC is asserted), garbage otherwise
    assign oBAC = bPIOBUSF & bPIOBUSN & bPIOBUSL  &  bPIOBUSM & bPIOBUSE & bPIOBUSD  &
                  bPIOBUSK & bPIOBUSC & bPIOBUSJ  &  bPIOBUSB & bPIOBUSH & bPIOBUSA;

    // gate INPUTBUS out to PIOBUS whenever it is enabled (from second half of io pulse to 40nS afterward)
    assign bPIOBUSA = x_INPUTBUS ? 1'bZ : iINPUTBUS[00];
    assign bPIOBUSH = x_INPUTBUS ? 1'bZ : iINPUTBUS[01];
    assign bPIOBUSB = x_INPUTBUS ? 1'bZ : iINPUTBUS[02];
    assign bPIOBUSJ = x_INPUTBUS ? 1'bZ : iINPUTBUS[03];
    assign bPIOBUSL = x_INPUTBUS ? 1'bZ : iINPUTBUS[04];
    assign bPIOBUSE = x_INPUTBUS ? 1'bZ : iINPUTBUS[05];
    assign bPIOBUSM = x_INPUTBUS ? 1'bZ : iINPUTBUS[06];
    assign bPIOBUSF = x_INPUTBUS ? 1'bZ : iINPUTBUS[07];
    assign bPIOBUSN = x_INPUTBUS ? 1'bZ : iINPUTBUS[08];
    assign bPIOBUSC = x_INPUTBUS ? 1'bZ : iINPUTBUS[09];
    assign bPIOBUSK = x_INPUTBUS ? 1'bZ : iINPUTBUS[10];
    assign bPIOBUSD = x_INPUTBUS ? 1'bZ : iINPUTBUS[11];

    // latch MB contents from piobus when not in io pulse
    // MB holds io opcode being executed during io pulses and has been valid for a few hundred nanoseconds
    // clock it continuously when outside of io pulse so it tracks MB contents for other purposes (eg, writing extended memory)
    always @(posedge CLOCK) begin
        if (inusereset) begin
            r_BAC <= 1;
            r_BMB <= 1;
            x_INPUTBUS <= 1;
        end else if ((iopsetcount == 0) & (iopclrcount == 7)) begin
            // not doing any io, continuously clock in MB contents
            r_BAC <= 1;
            r_BMB <= 0;
            x_INPUTBUS <= 1;
            oBMB  <= bPIOBUSF & bPIOBUSN & bPIOBUSM  &  bPIOBUSE & bPIOBUSC & bPIOBUSJ  &
                     bPIOBUSD & bPIOBUSL & bPIOBUSK  &  bPIOBUSB & bPIOBUSA & bPIOBUSH;
        end else if (iopsetcount ==  1 & iopclrcount == 0) begin
            // just started a long (500+ nS) io pulse, turn off passing MB onto PIOBUS
            // ...and leave oBMB contents as they were (contains io opcode)
            r_BMB <= 1;
        end else if (iopsetcount ==  2 & iopclrcount == 0) begin
            // ... then turn on passing AC onto PIOBUS
            r_BAC <= 0;
        end else if (iopsetcount == 14 & iopclrcount == 0) begin
            r_BAC <= 1;
        end else if (iopsetcount == 15 & iopclrcount == 0) begin
            x_INPUTBUS <= 0;
        end else if (iopsetcount ==  0 & iopclrcount == 4) begin
            x_INPUTBUS <= 1;
        end
    end

    // internal pio busses - wire-ored(active high)/-anded(active low) from device to processor

    assign dev_iBEMA         =      arm_iBEMA;
    assign dev_iCA_INCREMENT =      arm_iCA_INCREMENT;
    assign dev_iDATA_IN      =      arm_iDATA_IN;
    assign dev_iINPUTBUS     =      arm_iINPUTBUS  | ttibus  | tt40ibus  | rkibus | xmibus;
    assign dev_iMEMINCR      =      arm_iMEMINCR;
    assign dev_iMEM          =      arm_iMEM          | xmmem;
    assign dev_iMEM_P        =      arm_iMEM_P;
    assign dev_iTHREECYCLE   =      arm_iTHREECYCLE;
    assign dev_i_AC_CLEAR    = ~ (~ arm_i_AC_CLEAR | ttacclr | tt40acclr | rkacclr);
    assign dev_i_BRK_RQST    =      arm_i_BRK_RQST;
    assign dev_i_DMAADDR     =      arm_i_DMAADDR;
    assign dev_i_DMADATA     =      arm_i_DMADATA;
    assign dev_i_EA          =      arm_i_EA          & xm_ea;
    assign dev_i_EMA         =      arm_i_EMA;
    assign dev_i_INT_INHIBIT =      arm_i_INT_INHIBIT & xm_intinh;
    assign dev_i_INT_RQST    = ~ (~ arm_i_INT_RQST | ttintrq | tt40intrq | rkintrq);
    assign dev_i_IO_SKIP     = ~ (~ arm_i_IO_SKIP  | ttioskp | tt40ioskp | rkioskp);
    assign dev_i_MEMDONE     =      arm_i_MEMDONE     & xm_mwdone;
    assign dev_i_STROBE      =      arm_i_STROBE      & xm_mrdone;

    // teletype interfaces

    pdp8ltty ttinst (
        .CLOCK (CLOCK),
        .RESET (ioreset),

        .armwrite (ttawrite),
        .armraddr (readaddr[3:2]),
        .armwaddr (writeaddr[3:2]),
        .armwdata (saxi_WDATA),
        .armrdata (ttardata),

        .iopstart (iopstart),
        .iopstop  (iopstop),
        .ioopcode (dev_oBMB),
        .cputodev (dev_oBAC),

        .devtocpu (ttibus),
        .AC_CLEAR (ttacclr),
        .IO_SKIP  (ttioskp),
        .INT_RQST (ttintrq)
    );

    pdp8ltty #(.KBDEV (8'o40)) tt40inst (
        .CLOCK (CLOCK),
        .RESET (ioreset),

        .armwrite (tt40awrite),
        .armraddr (readaddr[3:2]),
        .armwaddr (writeaddr[3:2]),
        .armwdata (saxi_WDATA),
        .armrdata (tt40ardata),

        .iopstart (iopstart),
        .iopstop  (iopstop),
        .ioopcode (dev_oBMB),
        .cputodev (dev_oBAC),

        .devtocpu (tt40ibus),
        .AC_CLEAR (tt40acclr),
        .IO_SKIP  (tt40ioskp),
        .INT_RQST (tt40intrq)
    );

    // disk interface

    pdp8lrk8je rkinst (
        .CLOCK (CLOCK),
        .RESET (ioreset),

        .armwrite (rkawrite),
        .armraddr (readaddr[4:2]),
        .armwaddr (writeaddr[4:2]),
        .armwdata (saxi_WDATA),
        .armrdata (rkardata),

        .iopstart (iopstart),
        .iopstop  (iopstop),
        .ioopcode (dev_oBMB),
        .cputodev (dev_oBAC),

        .devtocpu (rkibus),
        .AC_CLEAR (rkacclr),
        .IO_SKIP  (rkioskp),
        .INT_RQST (rkintrq)
    );

    // extended memory interface

    pdp8lxmem xminst (
        .CLOCK (CLOCK),
        .RESET (ioreset),

        .armwrite (xmawrite),           // arm writing to backside register
        .armraddr (readaddr[3:2]),      // arm reading from backside register
        .armwaddr (writeaddr[3:2]),     // arm writing to backside register
        .armwdata (saxi_WDATA),         // data to write to backside register
        .armrdata (xmardata),           // data being read from backside register

        .iopstart (iopstart),           // an io iopcode is being executed by the cpu
        .iopstop  (iopstop),            // the io cycle has ended
        .ioopcode (dev_oBMB),           // io opcode
        .cputodev (dev_oBAC),           // data being sent to device
        .devtocpu (xmibus),             // data being received from device

        .memstart (dev_oMEMSTART),      // pulse to start a memory cycle
        .memaddr (dev_oMA),             // address within memory
        .memwdat (dev_oBMB),            // data being written to memory
        .memrdat (xmmem),               // data that was read from memory
        ._mrdone (xm_mrdone),           // pulse indicating read data is valid
        ._mwdone (xm_mwdone),           // pulse indicating write has completed

        ._df_enab (dev_o_DF_ENABLE),    // next mem cycle should use data field
        .exefet (dev_oE_SET_F_SET),     // next mem cycle is for fetch or execute
        ._intack (dev_o_LOAD_SF),       // next mem cycle is ackmowledging interrupt
        .jmpjms (dev_oJMP_JMS),         // instruction register holds JMP or JMS instruction
        .ts3 (dev_oBTS_3),              // processor in time state 3
        ._zf_enab (dev_o_SP_CYC_NEXT),  // special (WC or CA) cycle is next
        ._ea (xm_ea),                   // _EA=1 use 4K stack and cpu's controller; _EA=0 use this controller
        ._intinh (xm_intinh),           // block interrupt delivery

        .xbraddr (xbraddr),
        .xbrwdat (xbrwdat),
        .xbrrdat (xbrrdat),
        .xbrenab (xbrenab),
        .xbrwena (xbrwena)
    );

endmodule
