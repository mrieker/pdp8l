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

    output     i3CYCLE,
    output     iAC_CLEAR,
    output     iBRK_RQST,
    output     i_EA,
    output     iEMA,
    output     iEXTDMAADD_12,
    output     iINT_INHIBIT,
    output     iIO_SKIP,
    output     i_MEMDONE,
    output     iMEMINCR,
    output     i_STROBE,

    output     i_B36V1,
    output     iBEMA,
    output     i_CA_INCRMNT,
    output     i_D36B2,
    output     i_DATA_IN,
    output     iINT_RQST,
    output     i_MEM_07,

    input  o_ADDR_ACCEPT,
    input  oB_RUN,
    input  o_BF_ENABLE,
    input  oBUSINIT,
    input  o_DF_ENABLE,
    input  o_KEY_CLEAR,
    input  o_KEY_DF,
    input  o_KEY_IF,
    input  o_KEY_LOAD,
    input  o_LOAD_SF,
    input  o_SP_CYC_NEXT,
    input  o_B_BREAK,
    input  oBIOP1,
    input  oBIOP2,
    input  oBIOP4,
    input  oBTP2,
    input  oBTP3,
    input  oBTS_1,
    input  oBTS_3,
    input  o_BWC_OVERFLOW,
    input  oC36B2,
    input  oD35B2,
    input  oE_SET_F_SET,
    input  oJMP_JMS,
    input  oLINE_LOW,
    input  oMEMSTART,
    output r_BAC,
    output r_BMB,
    output r_MA,
    output x_DMAADDR,
    output x_DMADATA,
    output x_INPUTBUS,
    output x_MEM,

    output[14:00] xbraddr,
    output[11:00] xbrwdat,
    input[11:00] xbrrdat,
    output xbrenab,
    output xbrwena,

    output[14:00] vidaddra, vidaddrb,
    output[21:00] viddataa,
    output videnaba, vidwrena, videnabb,
    input[21:00] viddatab,

    // i2c interface to front panel
    inout      bFPI2CLOCK,      // bi-dir clock bus
    inout      bFPI2CDATA,      // bi-dir data bus
    output reg i_FPI2CDENA,     // low to turn on bi-dir driver; high to shut off
    output reg iFPI2CDDIR,      // high when sending; low when receiving

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
    localparam VERSION = 32'h384C4097;

    reg[11:02] readaddr, writeaddr;
    wire debounced, lastswLDAD, lastswSTART, simmemen;

    // pdp8/l module signals

    //      i... : signals going to hardware PDP-8/L
    //      o... : signals coming from hardware PDP-8/L

    //  sim_i... : signals going to simulated PDP-8/L (pdp8lsim.v)
    //  sim_o... : signals coming from simulated PDP-8/L (pdp8lsim.v)

    //  dev_i... : signals from io devices going to both i... and sim_i...
    //  dev_o... : selected from o... or sim_o... going to io devices

    wire[31:00] regctla, regctlb, regctlc, regctld, regctle, regctlf, regctlg, regctlh, regctli, regctlj, regctlk;

    wire sim_iBEMA;
    wire sim_i_CA_INCRMNT;
    wire sim_i_DATA_IN;
    wire[11:00] sim_iINPUTBUS;
    wire sim_iMEMINCR;
    wire[11:00] sim_i_MEM;
    wire sim_i_MEM_P;
    wire sim_i3CYCLE;
    wire sim_iAC_CLEAR;
    wire sim_iBRK_RQST;
    wire[11:00] sim_iDMAADDR;
    wire[11:00] sim_iDMADATA;
    wire sim_i_EA;
    wire sim_iEMA;
    wire sim_iINT_INHIBIT;
    wire sim_iINT_RQST;
    wire sim_iIO_SKIP;
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
    wire sim_o_BWC_OVERFLOW;
    wire sim_o_B_BREAK;
    wire sim_oE_SET_F_SET;
    wire sim_oJMP_JMS;
    wire sim_oLINE_LOW;
    wire[11:00] sim_oMA;
    wire sim_oMEMSTART;
    wire sim_o_ADDR_ACCEPT;
    wire sim_o_BF_ENABLE;
    wire sim_oBUSINIT;
    wire sim_oB_RUN;
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
    wire dev_i_CA_INCRMNT;
    wire dev_i_DATA_IN;
    wire[11:00] dev_iINPUTBUS;
    wire dev_iMEMINCR;
    wire[11:00] dev_i_MEM;
    wire dev_i_MEM_P;
    wire dev_i3CYCLE;
    wire dev_iAC_CLEAR;
    wire dev_iBRK_RQST;
    wire[11:00] dev_iDMAADDR;
    wire[11:00] dev_iDMADATA;
    wire dev_i_EA;
    wire dev_iEMA;
    wire dev_iINT_INHIBIT;
    wire dev_iINT_RQST;
    wire dev_iIO_SKIP;
    wire dev_i_MEMDONE;
    wire dev_i_STROBE;
    wire dev_i_B36V1 = 1;
    wire dev_i_D36B2;
    wire[11:00] dev_oBAC;
    wire dev_oBIOP1;
    wire dev_oBIOP2;
    wire dev_oBIOP4;
    wire[11:00] dev_oBMB;
    wire dev_oBTP2;
    wire dev_oBTP3;
    wire dev_oBTS_1;
    wire dev_oBTS_3;
    wire dev_o_BWC_OVERFLOW;
    wire dev_o_B_BREAK;
    wire dev_oE_SET_F_SET;
    wire dev_oJMP_JMS;
    wire dev_oLINE_LOW;
    wire[11:00] dev_oMA;
    wire dev_oMEMSTART;
    wire dev_o_ADDR_ACCEPT;
    wire dev_o_BF_ENABLE;
    wire dev_oBUSINIT;
    wire dev_oB_RUN;
    wire dev_o_DF_ENABLE;
    wire dev_o_KEY_CLEAR;
    wire dev_o_KEY_DF;
    wire dev_o_KEY_IF;
    wire dev_o_KEY_LOAD;
    wire dev_o_LOAD_SF;
    wire dev_o_SP_CYC_NEXT;

    wire[11:00] iDMAADDR, iDMADATA;      // dma address, data going out to real PDP-8/L via our DMABUS
                                         // ...used to access real PDP-8/L 4K core memory
    wire[11:00] iINPUTBUS;               // io data going out to real PDP-8/L INPUTBUS via our PIOBUS
    wire[11:00] i_MEM;                   // extended memory data going to real PDP-8/L MEM via our MEMBUS
    wire i_MEM_P;                        // extended memory parity going to real PDP-8/L MEM_P via our MEMBUSA
    reg[11:00] oBAC;                     // sampled real PDP-8/L AC contents, used during i/o pulse processing
    reg[11:00] oBMB;                     // sampled real PDP-8/L MB contents, used for i/o opcode, writing extended memory, reading core memory
    reg[11:00] oMA;                      // sampled real PDP-8/L MA contents, used by PDP-8/L to access extended memory
    reg[9:0] meminprog;

    reg bareit, simit, lastts1, lastts3, didio;
    reg nanocontin, nanocstep, nanotrigger, fpgareset;
    wire iopstart, iopstop;
    wire acclr, intrq, ioskp;
    reg[3:0] iopsetcount;               // count fpga cycles where an IOP is on
    reg[3:0] iopclrcount;               // count fpga cycles where no IOP is on
    reg[31:00] memcycctr;

    // synchroniSed input wires
    wire q_ADDR_ACCEPT;
    wire qB_RUN;
    wire qBUSINIT;
    wire q_KEY_CLEAR;
    wire q_KEY_LOAD;
    wire q_LOAD_SF;
    wire qBIOP1;
    wire qBIOP2;
    wire qBIOP4;
    wire qBTP2;
    wire qBTP3;
    wire qBTS_1;
    wire qBTS_3;
    wire qD35B2;
    wire qE_SET_F_SET;
    wire qLINE_LOW;
    wire qMEMSTART;

    // arm interface wires;
    reg arm_iBEMA;
    reg arm_i_CA_INCRMNT;
    reg arm_i_DATA_IN;
    reg arm_iMEMINCR;
    reg arm_i_MEM_P;
    reg arm_i3CYCLE;
    reg arm_iAC_CLEAR;
    reg arm_iBRK_RQST;
    reg arm_i_EA;
    reg arm_iEMA;
    reg arm_iINT_INHIBIT;
    reg arm_iINT_RQST;
    reg arm_iIO_SKIP;
    reg arm_i_MEMDONE;
    reg arm_i_STROBE;
    reg arm_i_D36B2;
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
    reg[11:00] arm_i_MEM;
    reg[11:00] arm_iDMAADDR;
    reg[11:00] arm_iDMADATA;
    reg[11:00] arm_swSR;
    reg arm_x_MEM, arm_x_INPUTBUS, arm_x_DMADATA, arm_x_DMAADDR;
    reg arm_r_MA, arm_r_BMB, arm_r_BAC, arm_hizmembus;
    reg dev_x_INPUTBUS, dev_x_DMADATA, dev_x_DMAADDR;
    reg dev_r_BMB, dev_r_BAC;
    wire dev_x_MEM, dev_r_MA, dev_hizmembus;
    wire hizmembus;

    // shadow interface wires
    wire[31:00] shardata;

    // tty interface wires
    wire[31:00] ttardata;
    wire[11:00] ttibus;
    wire ttacclr, ttintrq, ttioskp;
    wire[31:00] tt40ardata;
    wire[11:00] tt40ibus;
    wire tt40acclr, tt40intrq, tt40ioskp;
    wire[31:00] tt42ardata;
    wire[11:00] tt42ibus;
    wire tt42acclr, tt42intrq, tt42ioskp;
    wire[31:00] tt44ardata;
    wire[11:00] tt44ibus;
    wire tt44acclr, tt44intrq, tt44ioskp;
    wire[31:00] tt46ardata;
    wire[11:00] tt46ibus;
    wire tt46acclr, tt46intrq, tt46ioskp;

    // disk interface wires
    wire[31:00] rkardata;
    wire[11:00] rkibus;
    wire rkacclr, rkintrq, rkioskp;

    // front panel i2c interface
    wire[31:00] fpi2crdata;

    // extended memory interface wires
    wire[31:00] xmardata;
    wire[11:00] xmibus;
    wire[11:00] xmmem;
    wire[2:0] xmfield;
    wire xm_intinh, xm_mrdone, xm_mwdone, xm_ea;

    // core memory interface wires
    wire[31:00] cmardata;
    wire cmbrkrqst, cmbrkwrite, cmbrk3cycl, cmbrkcainc;
    wire[11:00] cmbrkaddr, cmbrkwdat;
    wire[2:0] cmbrkema;

    // tape interface wires
    wire[31:00] tcardata;
    wire[11:00] tcibus;
    wire tcacclr, tcintrq, tcioskp;

    // paper tape reader interface wires
    wire[31:00] prardata;
    wire[11:00] pribus;
    wire pracclr, printrq, prioskp;

    // paper tape punch interface wires
    wire[31:00] ppardata;
    wire ppintrq, ppioskp;

    // video interface wires
    wire[31:00] vcardata;
    wire[11:00] vcibus;
    wire vcacclr, vcintrq, vcioskp;

    // pulsebit interface wires
    wire[31:00] pbardata;
    wire[11:00] pbibus;
    wire pbacclr, pbioskp;
    wire pulsebit;

    // bus values that are constants
    assign saxi_BRESP = 0;  // A3.4.4/A10.3 transfer OK
    assign saxi_RRESP = 0;  // A3.4.4/A10.3 transfer OK

    reg[52:00] ilaarray[4095:0], ilardata;
    reg[11:00] ilaafter, ilaindex;
    reg ilaarmed;

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
        (readaddr        == 10'b0000001100) ? { meminprog, 1'b0, xbrenab, 3'b0, xbrwena, 1'b0, xbraddr } :
        (readaddr        == 10'b0000001101) ? { 4'b0, xbrwdat, 4'b0, xbrrdat } :
        (readaddr        == 10'b0000001110) ? memcycctr  :
        (readaddr        == 10'b0000001111) ? {
            bDMABUSA, bDMABUSB, bDMABUSC, bDMABUSD, bDMABUSE, bDMABUSF, bDMABUSH, bDMABUSJ, bDMABUSK, bDMABUSL, bDMABUSM, bDMABUSN,
            12'b0, hizmembus, r_BAC, r_BMB, r_MA, x_DMAADDR, x_DMADATA, x_INPUTBUS, x_MEM } :
        (readaddr        == 10'b0000010000) ? {
            bMEMBUSA, bMEMBUSB, bMEMBUSC, bMEMBUSD, bMEMBUSE, bMEMBUSF, bMEMBUSH, bMEMBUSJ, bMEMBUSK, bMEMBUSL, bMEMBUSM, bMEMBUSN,
            bPIOBUSA, bPIOBUSB, bPIOBUSC, bPIOBUSD, bPIOBUSE, bPIOBUSF, bPIOBUSH, bPIOBUSJ, bPIOBUSK, bPIOBUSL, bPIOBUSM, bPIOBUSN,
            8'b0 } :
        (readaddr        == 10'b0000010001) ? { ilaarmed, 3'b0, ilaafter, 4'b0, ilaindex } :
        (readaddr        == 10'b0000010010) ? {        ilardata[31:00] } :
        (readaddr        == 10'b0000010011) ? { 11'b0, ilardata[52:32] } :
        (readaddr[11:05] ==  7'b0000100)    ? rkardata   :
        (readaddr[11:05] ==  7'b0000101)    ? vcardata   :
        (readaddr[11:05] ==  7'b0000110)    ? fpi2crdata :
        (readaddr[11:05] ==  8'b0000111)    ? xmardata   :
        (readaddr[11:05] ==  8'b0001000)    ? shardata   :
        (readaddr[11:04] ==  8'b00010010)   ? cmardata   :
        (readaddr[11:04] ==  8'b00010011)   ? ttardata   :
        (readaddr[11:04] ==  8'b00010100)   ? tt40ardata :
        (readaddr[11:04] ==  8'b00010101)   ? tt42ardata :
        (readaddr[11:04] ==  8'b00010110)   ? tt44ardata :
        (readaddr[11:04] ==  8'b00010111)   ? tt46ardata :
        (readaddr[11:04] ==  8'b00011000)   ? pbardata   :
        (readaddr[11:03] ==  9'b000110010)  ? prardata   :
        (readaddr[11:03] ==  9'b000110011)  ? ppardata   :
        (readaddr[11:03] ==  9'b000110100)  ? tcardata   :
        32'hDEADBEEF;

    wire armwrite   = saxi_WREADY & saxi_WVALID;    // arm is writing a register (single fpga clock cycle)
    wire rkawrite   = armwrite & writeaddr[11:05] == 7'b0000100;
    wire vcawrite   = armwrite & writeaddr[11:05] == 7'b0000101;
    wire fpi2cwrite = armwrite & writeaddr[11:05] == 7'b0000110;
    wire xmawrite   = armwrite & writeaddr[11:05] == 7'b0000111;
    wire shawrite   = armwrite & writeaddr[11:05] == 7'b0001000;
    wire cmawrite   = armwrite & writeaddr[11:04] == 8'b00010010;
    wire ttawrite   = armwrite & writeaddr[11:04] == 8'b00010011;
    wire tt40awrite = armwrite & writeaddr[11:04] == 8'b00010100;
    wire tt42awrite = armwrite & writeaddr[11:04] == 8'b00010101;
    wire tt44awrite = armwrite & writeaddr[11:04] == 8'b00010110;
    wire tt46awrite = armwrite & writeaddr[11:04] == 8'b00010111;
    wire pbawrite   = armwrite & writeaddr[11:04] == 9'b00011000;
    wire prawrite   = armwrite & writeaddr[11:03] == 9'b000110010;
    wire ppawrite   = armwrite & writeaddr[11:03] == 9'b000110011;
    wire tcawrite   = armwrite & writeaddr[11:03] == 9'b000110100;

    // A3.3.1 Read transaction dependencies
    // A3.3.1 Write transaction dependencies
    //        AXI4 write response dependency
    always @(posedge CLOCK) begin
        if (~ RESET_N) begin
            saxi_ARREADY <= 1;                          // we are ready to accept read address
            saxi_RVALID  <= 0;                          // we are not sending out read data

            saxi_AWREADY <= 1;                          // we are ready to accept write address
            saxi_WREADY  <= 0;                          // we are not ready to accept write data
            saxi_BVALID  <= 0;                          // we are not acknowledging any write

            simit       <= 0;                           // assume want to access real PDP-8/L by default
            fpgareset   <= 0;                           // we are resetting FPGA, don't do infinite reset loop
            nanocontin  <= 1;                           // turn on 100MHz FPGA clocking
            bareit      <= 0;                           // normal signals by default (not manual override)

        end else begin

            /////////////////////
            //  register read  //
            /////////////////////

            // check for PS sending us a read address
            if (saxi_ARREADY & saxi_ARVALID) begin
                readaddr <= saxi_ARADDR[11:02];             // save address bits we care about
                saxi_ARREADY <= 0;                          // we are no longer accepting a read address
                saxi_RVALID <= 1;                           // we are sending out the corresponding data

            // check for PS acknowledging receipt of data
            end else if (saxi_RVALID & saxi_RREADY) begin
                saxi_ARREADY <= 1;                          // we are ready to accept an address again
                saxi_RVALID <= 0;                           // we are no longer sending out data
            end

            //////////////////////
            //  register write  //
            //////////////////////

            // check for PS sending us write data
            if (armwrite) begin
                case (writeaddr)                            // write data to register
                     10'b0000000001: begin
                        arm_iBEMA         <= saxi_WDATA[00];
                        arm_i_CA_INCRMNT  <= saxi_WDATA[01];
                        arm_i_DATA_IN     <= saxi_WDATA[02];
                        arm_iMEMINCR      <= saxi_WDATA[03];
                        arm_i_MEM_P       <= saxi_WDATA[04];
                        arm_i3CYCLE       <= saxi_WDATA[05];
                        arm_iAC_CLEAR     <= saxi_WDATA[06];
                        arm_iBRK_RQST     <= saxi_WDATA[07];
                        arm_i_EA          <= saxi_WDATA[08];
                        arm_iEMA          <= saxi_WDATA[09];
                        arm_iINT_INHIBIT  <= saxi_WDATA[10];
                        arm_iINT_RQST     <= saxi_WDATA[11];
                        arm_iIO_SKIP      <= saxi_WDATA[12];
                        arm_i_MEMDONE     <= saxi_WDATA[13];
                        arm_i_STROBE      <= saxi_WDATA[14];
                        ////i_B36V1           <= saxi_WDATA[15];
                        arm_i_D36B2       <= saxi_WDATA[16];
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
                        arm_swSR          <= saxi_WDATA[31:20];
                    end

                    10'b0000000011: begin
                        arm_iINPUTBUS     <= saxi_WDATA[11:00];
                        arm_i_MEM         <= saxi_WDATA[27:16];
                    end

                    10'b0000000100: begin
                        arm_iDMAADDR      <= saxi_WDATA[11:00];
                        arm_iDMADATA      <= saxi_WDATA[27:16];
                    end

                    10'b0000000101: begin
                        simit             <= saxi_WDATA[00];
                        fpgareset         <= saxi_WDATA[01];
                        nanocontin        <= saxi_WDATA[02];
                        bareit            <= saxi_WDATA[06];
                    end

                    10'b0000001111: begin
                        arm_x_MEM         <= saxi_WDATA[00];
                        arm_x_INPUTBUS    <= saxi_WDATA[01];
                        arm_x_DMADATA     <= saxi_WDATA[02];
                        arm_x_DMAADDR     <= saxi_WDATA[03];
                        arm_r_MA          <= saxi_WDATA[04];
                        arm_r_BMB         <= saxi_WDATA[05];
                        arm_r_BAC         <= saxi_WDATA[06];
                        arm_hizmembus     <= saxi_WDATA[07];
                    end
                endcase
                saxi_AWREADY <= 1;                          // we are ready to accept an address again
                saxi_WREADY  <= 0;                          // we are no longer accepting write data
                saxi_BVALID  <= 1;                          // we have accepted the data

            end else begin
                // check for PS sending us a write address
                if (saxi_AWREADY & saxi_AWVALID) begin
                    writeaddr <= saxi_AWADDR[11:02];        // save address bits we care about
                    saxi_AWREADY <= 0;                      // we are no longer accepting a write address
                    saxi_WREADY  <= 1;                      // we are ready to accept write data
                end

                // check for PS acknowledging write acceptance
                if (saxi_BVALID & saxi_BREADY) begin
                    saxi_BVALID <= 0;
                end
            end
        end

        if (~ armwrite & pwronreset) begin
            arm_x_MEM         <= 1;
            arm_x_INPUTBUS    <= 1;
            arm_x_DMADATA     <= 1;
            arm_x_DMAADDR     <= 1;
            arm_r_MA          <= 1;
            arm_r_BMB         <= 1;
            arm_r_BAC         <= 1;
            arm_hizmembus     <= 1;

            arm_iBEMA         <= 0;
            arm_i_CA_INCRMNT  <= 1;
            arm_i_DATA_IN     <= 1;
            arm_iMEMINCR      <= 0;
            arm_i_MEM_P       <= 1;
            arm_i3CYCLE       <= 0;
            arm_iAC_CLEAR     <= 0;
            arm_iBRK_RQST     <= 0;
            arm_i_EA          <= 1;
            arm_iEMA          <= 0;
            arm_iINT_INHIBIT  <= 0;
            arm_iINT_RQST     <= 0;
            arm_iIO_SKIP      <= 0;
            arm_i_MEMDONE     <= 1;
            arm_i_STROBE      <= 1;
            ////i_B36V1           <= 1;
            arm_i_D36B2       <= 1;

            arm_iINPUTBUS     <= 12'o0000;
            arm_i_MEM         <= 12'o7777;
            arm_iDMAADDR      <= 12'o0000;
            arm_iDMADATA      <= 12'o0000;
        end
    end

    ////////////////////////////////////////////////
    //  synchronize signals output by the PDP-8L  //
    ////////////////////////////////////////////////

    synk synkaa (CLOCK, q_ADDR_ACCEPT, o_ADDR_ACCEPT);
    synk synkbr (CLOCK, qB_RUN,        oB_RUN);
    synk synkbi (CLOCK, qBUSINIT,      oBUSINIT);
    synk synkkc (CLOCK, q_KEY_CLEAR,   o_KEY_CLEAR);
    synk synkkl (CLOCK, q_KEY_LOAD,    o_KEY_LOAD);
    synk synkls (CLOCK, q_LOAD_SF,     o_LOAD_SF);
    synk synkp1 (CLOCK, qBIOP1,        oBIOP1);
    synk synkp2 (CLOCK, qBIOP2,        oBIOP2);
    synk synkp4 (CLOCK, qBIOP4,        oBIOP4);
    synk synkt2 (CLOCK, qBTP2,         oBTP2);
    synk synkp3 (CLOCK, qBTP3,         oBTP3);
    synk synkt1 (CLOCK, qBTS_1,        oBTS_1);
    synk synkt3 (CLOCK, qBTS_3,        oBTS_3);
    synk synkd3 (CLOCK, qD35B2,        oD35B2);
    synk synkef (CLOCK, qE_SET_F_SET,  oE_SET_F_SET);
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

    assign sim_iBEMA          = simit ? dev_iBEMA        : 0;
    assign sim_i_CA_INCRMNT   = simit ? dev_i_CA_INCRMNT : 1;
    assign sim_i_DATA_IN      = simit ? dev_i_DATA_IN    : 1;
    assign sim_iINPUTBUS      = simit ? dev_iINPUTBUS    : 12'h000;
    assign sim_iMEMINCR       = simit ? dev_iMEMINCR     : 0;
    assign sim_i_MEM          = simit ? dev_i_MEM        : 12'hFFF;
    assign sim_i_MEM_P        = simit ? dev_i_MEM_P      : 1;
    assign sim_i3CYCLE        = simit ? dev_i3CYCLE      : 0;
    assign sim_iAC_CLEAR      = simit ? dev_iAC_CLEAR    : 0;
    assign sim_iBRK_RQST      = simit ? dev_iBRK_RQST    : 0;
    assign sim_iDMAADDR       = simit ? dev_iDMAADDR     : 12'h000;
    assign sim_iDMADATA       = simit ? dev_iDMADATA     : 12'h000;
    assign sim_i_EA           = simit ? dev_i_EA         : 1;
    assign sim_iEMA           = simit ? dev_iEMA         : 0;
    assign sim_iINT_INHIBIT   = simit ? dev_iINT_INHIBIT : 0;
    assign sim_iINT_RQST      = simit ? dev_iINT_RQST    : 0;
    assign sim_iIO_SKIP       = simit ? dev_iIO_SKIP     : 0;
    assign sim_i_MEMDONE      = simit ? dev_i_MEMDONE    : 1;
    assign sim_i_STROBE       = simit ? dev_i_STROBE     : 1;

    assign     iBEMA          = simit ? 0       : dev_iBEMA;
    assign     i_CA_INCRMNT   = simit ? 1       : dev_i_CA_INCRMNT;
    assign     i_DATA_IN      = simit ? 1       : dev_i_DATA_IN;
    assign     iINPUTBUS      = simit ? 12'h000 : dev_iINPUTBUS;
    assign     iMEMINCR       = simit ? 0       : dev_iMEMINCR;
    assign     i_MEM          = simit ? 12'hFFF : dev_i_MEM;
    assign     i_MEM_P        = simit ? 1       : dev_i_MEM_P;
    assign     i3CYCLE        = simit ? 0       : dev_i3CYCLE;
    assign     iAC_CLEAR      = simit ? 0       : dev_iAC_CLEAR;
    assign     iBRK_RQST      = simit ? 0       : dev_iBRK_RQST;
    assign     iDMAADDR       = simit ? 12'h000 : dev_iDMAADDR;
    assign     iDMADATA       = simit ? 12'h000 : dev_iDMADATA;
    assign     i_EA           = simit ? 1       : dev_i_EA;
    assign     iEMA           = simit ? 0       : dev_iEMA;
    assign     iINT_INHIBIT   = simit ? 0       : dev_iINT_INHIBIT;
    assign     iINT_RQST      = simit ? 0       : dev_iINT_RQST;
    assign     iIO_SKIP       = simit ? 0       : dev_iIO_SKIP;
    assign     i_MEMDONE      = simit ? 1       : dev_i_MEMDONE;
    assign     i_STROBE       = simit ? 1       : dev_i_STROBE;
    assign     i_D36B2        = dev_i_D36B2;

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
    assign dev_o_BWC_OVERFLOW = simit ? sim_o_BWC_OVERFLOW : o_BWC_OVERFLOW;
    assign dev_o_B_BREAK      = simit ? sim_o_B_BREAK      : o_B_BREAK;
    assign dev_oE_SET_F_SET   = simit ? sim_oE_SET_F_SET   : qE_SET_F_SET;
    assign dev_oJMP_JMS       = simit ? sim_oJMP_JMS       : oJMP_JMS;
    assign dev_oLINE_LOW      = simit ? sim_oLINE_LOW      : qLINE_LOW;
    assign dev_oMA            = simit ? sim_oMA            : oMA;
    assign dev_oMEMSTART      = simit ? sim_oMEMSTART      : qMEMSTART;
    assign dev_o_ADDR_ACCEPT  = simit ? sim_o_ADDR_ACCEPT  : q_ADDR_ACCEPT;
    assign dev_o_BF_ENABLE    = simit ? sim_o_BF_ENABLE    : o_BF_ENABLE;
    assign dev_oBUSINIT       = simit ? sim_oBUSINIT       : qBUSINIT;
    assign dev_oB_RUN         = simit ? sim_oB_RUN         : qB_RUN;
    assign dev_o_DF_ENABLE    = simit ? sim_o_DF_ENABLE    : o_DF_ENABLE;
    assign dev_o_KEY_CLEAR    = simit ? sim_o_KEY_CLEAR    : q_KEY_CLEAR;
    assign dev_o_KEY_DF       = simit ? sim_o_KEY_DF       : o_KEY_DF;
    assign dev_o_KEY_IF       = simit ? sim_o_KEY_IF       : o_KEY_IF;
    assign dev_o_KEY_LOAD     = simit ? sim_o_KEY_LOAD     : q_KEY_LOAD;
    assign dev_o_LOAD_SF      = simit ? sim_o_LOAD_SF      : q_LOAD_SF;
    assign dev_o_SP_CYC_NEXT  = simit ? sim_o_SP_CYC_NEXT  : o_SP_CYC_NEXT;

    // reading arm registers gets device bus signals
    //  for 'i' signals: wired-or of all devices and what was written to arm registers
    //  for 'o' signals: selected signal from simulated PDP-8/L or real PDP-8/L

    assign regctla[00] = dev_iBEMA;
    assign regctla[01] = dev_i_CA_INCRMNT;
    assign regctla[02] = dev_i_DATA_IN;
    assign regctla[03] = dev_iMEMINCR;
    assign regctla[04] = dev_i_MEM_P;
    assign regctla[05] = dev_i3CYCLE;
    assign regctla[06] = dev_iAC_CLEAR;
    assign regctla[07] = dev_iBRK_RQST;
    assign regctla[08] = dev_i_EA;
    assign regctla[09] = dev_iEMA;
    assign regctla[10] = dev_iINT_INHIBIT;
    assign regctla[11] = dev_iINT_RQST;
    assign regctla[12] = dev_iIO_SKIP;
    assign regctla[13] = dev_i_MEMDONE;
    assign regctla[14] = dev_i_STROBE;
    assign regctla[15] = dev_i_B36V1;
    assign regctla[16] = dev_i_D36B2;
    assign regctla[25:17] = 0;

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
    assign regctlb[19:10] = 0;
    assign regctlb[31:20] = arm_swSR;

    assign regctlc[15:00] = { 4'b0, dev_iINPUTBUS };
    assign regctlc[31:16] = { 4'b0, dev_i_MEM     };
    assign regctld[15:00] = { 4'b0, dev_iDMAADDR  };
    assign regctld[31:16] = { 4'b0, dev_iDMADATA  };

    assign regctle[00] = simit;
    assign regctle[01] = fpgareset;
    assign regctle[02] = nanocontin;
    assign regctle[03] = nanotrigger;
    assign regctle[04] = nanocstep;
    assign regctle[05] = 0;
    assign regctle[06] = bareit;
    assign regctle[31:07] = 0;

    assign regctlf[00] = dev_oBIOP1;
    assign regctlf[01] = dev_oBIOP2;
    assign regctlf[02] = dev_oBIOP4;
    assign regctlf[03] = dev_oBTP2;
    assign regctlf[04] = dev_oBTP3;
    assign regctlf[05] = dev_oBTS_1;
    assign regctlf[06] = dev_oBTS_3;
    assign regctlf[07] = oC36B2;
    assign regctlf[08] = dev_o_BWC_OVERFLOW;
    assign regctlf[09] = dev_o_B_BREAK;
    assign regctlf[10] = dev_oE_SET_F_SET;
    assign regctlf[11] = dev_oJMP_JMS;
    assign regctlf[12] = dev_oLINE_LOW;
    assign regctlf[13] = dev_oMEMSTART;
    assign regctlf[14] = dev_o_ADDR_ACCEPT;
    assign regctlf[15] = dev_o_BF_ENABLE;
    assign regctlf[16] = dev_oBUSINIT;
    assign regctlf[17] = dev_oB_RUN;
    assign regctlf[18] = dev_o_DF_ENABLE;
    assign regctlf[19] = dev_o_KEY_CLEAR;
    assign regctlf[20] = dev_o_KEY_DF;
    assign regctlf[21] = dev_o_KEY_IF;
    assign regctlf[22] = dev_o_KEY_LOAD;
    assign regctlf[23] = dev_o_LOAD_SF;
    assign regctlf[24] = dev_o_SP_CYC_NEXT;
    assign regctlf[25] = oD35B2;
    assign regctlf[26] = didio;
    assign regctlf[31:27] = 0;

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
    assign regctlg[12] = lastswSTART;
    assign regctlg[13] = simmemen;
    assign regctlg[15:14] = 0;
    assign regctlg[27:16] = { sim_lbIR, 9'b000000000 };
    assign regctlg[31:28] = 0;

    assign regctlh[15:00] = { 4'b0, dev_oBAC };
    assign regctlh[31:16] = { 4'b0, dev_oBMB };
    assign regctli[15:00] = { 4'b0, dev_oMA  };
    assign regctli[31:16] = { 4'b0, sim_lbAC };
    assign regctlj[15:00] = { 4'b0, sim_lbMA };
    assign regctlj[31:16] = { 4'b0, sim_lbMB };

    // some arm program is resetting or zynq is powering up
    // - resets devices
    wire pwronreset = fpgareset | ~ RESET_N;

    // count memory cycles
    // dectape uses it for timing in z8ltc08.cc
    always @(posedge CLOCK) begin
        if (~ RESET_N) begin
            memcycctr <= 0;
        end else if (~ lastts3 & dev_oBTS_3) begin
            memcycctr <= memcycctr + 1;
        end
    end

    // see if PDP is doing stuff with the busses
    // everything the PDP does begins with a MEMSTART pulse and lasts less than 5uS
    // so we start a counter at 1023 at memstart and count it down to 0 (taking 10.23uS)
    // ...restarting whenever we see memstart
    // this is kind of like RUN, but takes into account cycles initiated by the console
    always @(posedge CLOCK) begin
        if (pwronreset) begin
            meminprog <= 0;
        end else if (nanocstep) begin
            if (dev_oMEMSTART) begin
                meminprog <= 1023;
            end else if (meminprog != 0) begin
                meminprog <= meminprog - 1;
            end
        end
    end

    ////////////////////////
    //  multiplex DMABUS  //
    ////////////////////////

    always @(posedge CLOCK) begin
        if (~ RESET_N) begin
            lastts1 <= 0;
            lastts3 <= 0;
            dev_x_DMAADDR <= 1;
            dev_x_DMADATA <= 1;
        end else begin
            if (nanocstep) begin

                // on falling edge of TS1, start sending write data to PDP
                // PDP gates DMADATA into reg gates during TS2 (see vol 2 p6 B-6,C-7)
                if (lastts1 & ~ dev_oBTS_1) begin
                    dev_x_DMAADDR <= 1;
                    dev_x_DMADATA <= 0;
                end

                // on falling edge of TS3, start sending address to PDP for next cycle
                // PDP gates DMAADDR into reg gates during TS4 (see vol 2 p6 B-5)
                else if (lastts3 & ~ dev_oBTS_3) begin
                    dev_x_DMAADDR <= 0;
                    dev_x_DMADATA <= 1;
                end
            end

            // save for transition detection
            lastts1 <= dev_oBTS_1;
            lastts3 <= dev_oBTS_3;
        end
    end

    assign x_DMAADDR = bareit ? arm_x_DMAADDR : dev_x_DMAADDR;
    assign x_DMADATA = bareit ? arm_x_DMADATA : dev_x_DMADATA;

    assign bDMABUSA = (~ x_DMAADDR & iDMAADDR[11-01]) | (~ x_DMADATA & iDMADATA[11-00]);
    assign bDMABUSB = (~ x_DMAADDR & iDMAADDR[11-10]) | (~ x_DMADATA & iDMADATA[11-01]);
    assign bDMABUSC = (~ x_DMAADDR & iDMAADDR[11-03]) | (~ x_DMADATA & iDMADATA[11-09]);
    assign bDMABUSD = (~ x_DMAADDR & iDMAADDR[11-11]) | (~ x_DMADATA & iDMADATA[11-03]);
    assign bDMABUSE = (~ x_DMAADDR & iDMAADDR[11-04]) | (~ x_DMADATA & iDMADATA[11-06]);
    assign bDMABUSF = (~ x_DMAADDR & iDMAADDR[11-06]) | (~ x_DMADATA & iDMADATA[11-07]);
    assign bDMABUSH = (~ x_DMAADDR & iDMAADDR[11-00]) | (~ x_DMADATA & iDMADATA[11-02]);
    assign bDMABUSJ = (~ x_DMAADDR & iDMAADDR[11-09]) | (~ x_DMADATA & iDMADATA[11-10]);
    assign bDMABUSK = (~ x_DMAADDR & iDMAADDR[11-02]) | (~ x_DMADATA & iDMADATA[11-04]);
    assign bDMABUSL = (~ x_DMAADDR & iDMAADDR[11-05]) | (~ x_DMADATA & iDMADATA[11-11]);
    assign bDMABUSM = (~ x_DMAADDR & iDMAADDR[11-07]) | (~ x_DMADATA & iDMADATA[11-05]);
    assign bDMABUSN = (~ x_DMAADDR & iDMAADDR[11-08]) | (~ x_DMADATA & iDMADATA[11-08]);

    ////////////////////////
    //  multiplex MEMBUS  //
    ////////////////////////

    // timing is controlled by pdp8lxmem module
    //  r_MA is asserted during first half of TS1 of an extended memory cycle
    //  x_MEM is asserted from middle of TS1 to end of TS4 for extended memory cycle

    assign r_MA  = bareit ? arm_r_MA  : dev_r_MA;
    assign x_MEM = bareit ? arm_x_MEM : dev_x_MEM;
    assign hizmembus = bareit ? arm_hizmembus : dev_hizmembus;

    // gate memory read data to PDP when it is reading from extended memory
    // data is gated from middle of TS1 to end of TS4 when using extended memory
    // send out zeroes when PDP is using its own core stack so we don't jam jMEM going to PDP
    assign bMEMBUSA = hizmembus ? 1'bZ : (x_MEM ? 0 : i_MEM[11-00]);
    assign bMEMBUSC = hizmembus ? 1'bZ : (x_MEM ? 0 : i_MEM[11-01]);
    assign bMEMBUSK = hizmembus ? 1'bZ : (x_MEM ? 0 : i_MEM[11-02]);
    assign bMEMBUSM = hizmembus ? 1'bZ : (x_MEM ? 0 : i_MEM[11-03]);
    assign bMEMBUSE = hizmembus ? 1'bZ : (x_MEM ? 0 : i_MEM[11-04]);
    assign bMEMBUSF = hizmembus ? 1'bZ : (x_MEM ? 0 : i_MEM[11-05]);
    assign bMEMBUSN = hizmembus ? 1'bZ : (x_MEM ? 0 : i_MEM[11-06]);
    assign bMEMBUSH = hizmembus ? 1'bZ : (x_MEM ? 0 : i_MEM_P);
    assign bMEMBUSJ = hizmembus ? 1'bZ : (x_MEM ? 0 : i_MEM[11-08]);
    assign bMEMBUSB = hizmembus ? 1'bZ : (x_MEM ? 0 : i_MEM[11-09]);
    assign bMEMBUSL = hizmembus ? 1'bZ : (x_MEM ? 0 : i_MEM[11-10]);
    assign bMEMBUSD = hizmembus ? 1'bZ : (x_MEM ? 0 : i_MEM[11-11]);
    assign i_MEM_07 =                     x_MEM ? 0 : i_MEM[11-07];

    ////////////////////////
    //  multiplex PIOBUS  //
    ////////////////////////

    // PIOBUS control
    //   outside io pulses, PIOBUS is gating MB contents into us
    //   during first half of io pulse, PIOBUS is gating AC contents into us
    //   during second half of io pulse and for 40nS after, PIOBUS is gating INPUTBUS out to PDP-8/L
    //   - PIOBUS must be zeroes if none of our devices are selected during this time
    //     so we don't jam up the j_INPUTBUS going to the PDP

    // gate INPUTBUS out to PIOBUS whenever it is enabled (from second half of io pulse to 40nS afterward)
    // note that iINPUTBUS will be all zeroes if none of our devices are selected cuz it is or of all device outputs
    // ...which causes all PIOBUS lines to be zero and open-draining all the j_INPUTBUS transistors
    assign bPIOBUSA = x_INPUTBUS ? 1'bZ : iINPUTBUS[11-00];
    assign bPIOBUSH = x_INPUTBUS ? 1'bZ : iINPUTBUS[11-01];
    assign bPIOBUSB = x_INPUTBUS ? 1'bZ : iINPUTBUS[11-02];
    assign bPIOBUSC = x_INPUTBUS ? 1'bZ : iINPUTBUS[11-03];
    assign bPIOBUSK = x_INPUTBUS ? 1'bZ : iINPUTBUS[11-04];
    assign bPIOBUSM = x_INPUTBUS ? 1'bZ : iINPUTBUS[11-05];
    assign bPIOBUSE = x_INPUTBUS ? 1'bZ : iINPUTBUS[11-06];
    assign bPIOBUSF = x_INPUTBUS ? 1'bZ : iINPUTBUS[11-07];
    assign bPIOBUSN = x_INPUTBUS ? 1'bZ : iINPUTBUS[11-08];
    assign bPIOBUSJ = x_INPUTBUS ? 1'bZ : iINPUTBUS[11-09];
    assign bPIOBUSD = x_INPUTBUS ? 1'bZ : iINPUTBUS[11-10];
    assign bPIOBUSL = x_INPUTBUS ? 1'bZ : iINPUTBUS[11-11];

    // latch MB contents from piobus when not in io pulse
    // MB holds io opcode being executed during io pulses and has been valid for a few hundred nanoseconds
    // clock it continuously when outside of io pulse so it tracks MB contents for other purposes
    //  (eg, writing extended memory or reading core memory)

    //  if (it as been a while since end of io pulses) begin
    //    make sure we aren't sending INPUTBUS out over the PIOBUS
    //    if (processor can possibly be doing a memory cycle) begin
    //      if (within a few fpga cycles after TS3 dropped) begin
    //        use PIOBUS to update local copy of AC
    //      end else begin
    //        use PIOBUS to update local copy of MB
    //      end
    //    end else begin
    //      use PIOBUS to update local copy of AC
    //      use PIOBUS to update local copy of MB
    //    end
    //  end else if (within first part of io pulse) begin
    //    use PIOBUS to update local copy of AC
    //  end else if (within rest of io pulse or a little after) begin
    //    use PIOBUS to send INPUTBUS out over the PIOBUS
    //  end else begin
    //    use PIOBUS to update local copy of MB
    //  end

    // io pulses (oBIOP1,2,4) from the PDP are at least 440nS wide
    //   and have at least 240nS separating them from anything else
    // (maint vol 1 p 4-22)

    reg[1:0] memmadel, piobacdel, piobmbdel;
    reg[2:0] sincets3;

    wire[11:00] bmbbits = {
        bPIOBUSH, bPIOBUSA, bPIOBUSB,  bPIOBUSK, bPIOBUSL, bPIOBUSD,
        bPIOBUSJ, bPIOBUSC, bPIOBUSE,  bPIOBUSM, bPIOBUSN, bPIOBUSF
    };

    always @(posedge CLOCK) begin
        if (pwronreset) begin
            dev_r_BAC <= 1;
            dev_r_BMB <= 1;
            dev_x_INPUTBUS <= 1;
            memmadel  <= 0;
            piobacdel <= 0;
            piobmbdel <= 0;
            sincets3  <= 0;
        end else if (nanocstep) begin
            if ((iopsetcount == 0) & (iopclrcount == 15)) begin

                // not doing any i/o, stop sending reply back to PDP
                dev_x_INPUTBUS <= 1;

                // see if PDP is doing a memory cycle (either running or console)
                if (meminprog[9:4] != 0) begin

                    // grab copy of AC in first few cycles after any TS3 ends
                    //  the AC is updated for AND TAD DCA OPR by posedge TP3
                    //  so should be updated by the time we see TS3 drop
                    // this is just used for console display of AC contents
                    // if the TS3 is for an IOT, this should be over long before first i/o pulse
                    if (lastts3 & ~ dev_oBTS_3) begin
                        sincets3  <= 1;
                        dev_r_BAC <= 1;
                        dev_r_BMB <= 1;
                    end else if (sincets3 == 1) begin
                        sincets3  <= 2;
                        dev_r_BAC <= 0;
                    end else if (sincets3 == 6) begin
                        sincets3  <= 7;
                        dev_r_BAC <= 1;
                    end else if (sincets3 == 7) begin
                        sincets3  <= 0;
                        dev_r_BMB <= 0;
                    end else if (sincets3 != 0) begin
                        sincets3  <= sincets3 + 1;
                    end

                    // otherwise continuously clock in MB contents
                    else begin
                        dev_r_BAC <= 1;
                        dev_r_BMB <= 0;
                    end
                end

                // last 160nS well after a memory cycle ended without a new one starting
                // processor is assumed to be stopped and not doing any console memory cycle
                // get final update of AC and MB registers
                // we can get yanked out of this sequence at any time by the processor starting a memory cycle
                else begin
                    case (meminprog[3:0])
                        15: begin
                            dev_r_BAC <= 1;         // turn everything on PIOBUS off for 10nS
                            dev_r_BMB <= 1;
                            dev_x_INPUTBUS <= 1;
                        end
                        14: begin
                            dev_r_BAC <= 0;         // gate AC onto PIOBUS and start capturing
                        end
                         7: begin
                            dev_r_BAC <= 1;         // turn AC off and stop capturing
                        end
                         6: begin
                            dev_r_BMB <= 0;         // gate MB onto PIOBUS and start capturing
                        end
                    endcase
                end
            end else if (iopsetcount ==  1 & iopclrcount == 0) begin
                // just started a long (440+ nS) io pulse, turn off receiving MB from PIOBUS
                // ...and leave oBMB contents as they were (contains io opcode)
                dev_r_BMB <= 1;
                sincets3  <= 0;
            end else if (iopsetcount ==  2 & iopclrcount ==  0) begin
                // ... then turn on receiving AC from PIOBUS
                dev_r_BAC <= 0;
            end else if (iopsetcount == 14 & iopclrcount ==  0) begin
                // ... then turn off receiving AC from PIOBUS
                dev_r_BAC <= 1;
            end else if (iopsetcount == 15 & iopclrcount ==  0) begin
                // ... then turn on sending io results to PIOBUS
                dev_x_INPUTBUS <= 0;
            end else if (iopsetcount ==  0 & iopclrcount ==  7) begin
                // ... then turn off sending io results to PIOBUS
                dev_x_INPUTBUS <= 1;
            end else if (iopsetcount ==  0 & iopclrcount ==  8) begin
                // ... then turn on retrieving possibly updated AC via PIOBUS
                dev_r_BAC <= 0;
            end else if (iopsetcount ==  0 & iopclrcount == 14) begin
                // ... then turn off receiving AC from PIOBUS
                dev_r_BAC <= 1;
            end

            // update local copy of AC whenever it is gated onto PIOBUS
            // might get a couple false readings when dev_r_BAC first asserted but should catch up
            // - updates during first part of i/o pulses so it is valid for i/o opcode processing
            // - also updates just after end of TS3 so AC is updated for console while running
            // - also updates when PDP stops for console while stopped
            // delay when first turned on to give gates time to turn around, eliminating a glitch
            if (dev_r_BAC) begin
                piobacdel <= 0;
            end else if (piobacdel != 3) begin
                piobacdel <= piobacdel + 1;
            end else begin
                oBAC <= {
                    bPIOBUSA, bPIOBUSH, bPIOBUSB, bPIOBUSJ, bPIOBUSC, bPIOBUSK,
                    bPIOBUSD, bPIOBUSE, bPIOBUSM, bPIOBUSL, bPIOBUSN, bPIOBUSF };
            end

            // likewise with MB
            // - updates when not updating AC and not sending i/o input data out to PDP
            //   so it has i/o opcode for IOT instructions
            //   and data read from memory for DMA cycles
            //   and data to write to memory for external memory cycles
            //   and is also up-to-date for console
            // delay when first turned on to give gates time to turn around, eliminating a glitch
            if (dev_r_BMB) begin
                piobmbdel <= 0;
            end else if (piobmbdel != 3) begin
                piobmbdel <= piobmbdel + 1;
            end else begin
                oBMB <= bmbbits;
            end

            // likewise with MA while we're at it
            if (dev_r_MA) begin
                memmadel <= 0;
            end else if (memmadel != 3) begin
                memmadel <= memmadel + 1;
            end else begin
                oMA <= {
                    bMEMBUSH, bMEMBUSA, bMEMBUSB,  bMEMBUSJ, bMEMBUSE, bMEMBUSM,
                    bMEMBUSN, bMEMBUSF, bMEMBUSK,  bMEMBUSC, bMEMBUSL, bMEMBUSD };
            end
        end
    end

    assign r_BAC      = bareit ? arm_r_BAC      : dev_r_BAC;
    assign r_BMB      = bareit ? arm_r_BMB      : dev_r_BMB;
    assign x_INPUTBUS = bareit ? arm_x_INPUTBUS : dev_x_INPUTBUS;

    wire[11:00] bare12 = { 12 { bareit } };

    // internal pio busses - wire-ored(active high)/-anded(active low) from device to processor
    // block device outputs if bareit so pins can be directly controlled by arm
    assign dev_iBEMA         =   (  arm_iBEMA         | ~ bareit & (xmfield != 0));
    assign dev_i_CA_INCRMNT  = ~ (~ arm_i_CA_INCRMNT  | ~ bareit & cmbrkcainc);
    assign dev_i_DATA_IN     = ~ (~ arm_i_DATA_IN     | ~ bareit & cmbrkwrite);
    assign dev_iINPUTBUS     =   (  arm_iINPUTBUS     | ~ bare12 & (ttibus  | tt40ibus  | rkibus | vcibus | xmibus | tcibus | tt42ibus | tt44ibus | tt46ibus | pribus | pbibus));
    assign dev_iMEMINCR      =      arm_iMEMINCR;
    assign dev_i_MEM         = ~ (~ arm_i_MEM          | ~ bare12 & xmmem);
    assign dev_i_MEM_P       =      arm_i_MEM_P;
    assign dev_i3CYCLE       =   (  arm_i3CYCLE       | ~ bareit & cmbrk3cycl);
    assign dev_iAC_CLEAR     =   (  arm_iAC_CLEAR     | ~ bareit & (ttacclr | tt40acclr | rkacclr | vcacclr | tcacclr | tt42acclr | tt44acclr | tt46acclr | pracclr | pbacclr));
    assign dev_iBRK_RQST     =   (  arm_iBRK_RQST     | ~ bareit & cmbrkrqst);
    assign dev_iDMAADDR      =   (  arm_iDMAADDR      | ~ bare12 & cmbrkaddr);
    assign dev_iDMADATA      =   (  arm_iDMADATA      | ~ bare12 & cmbrkwdat);
    assign dev_i_EA          = ~ (~ arm_i_EA          | ~ bareit & ~ xm_ea);
    assign dev_iEMA          =   (  arm_iEMA          | ~ bareit & (xmfield != 0));
    assign dev_iINT_INHIBIT  =   (  arm_iINT_INHIBIT  | ~ bareit & ~ xm_intinh);
    assign dev_iINT_RQST     =   (  arm_iINT_RQST     | ~ bareit & (ttintrq | tt40intrq | rkintrq | vcintrq | tcintrq | tt42intrq | tt44intrq | tt46intrq | printrq | ppintrq));
    assign dev_iIO_SKIP      =   (  arm_iIO_SKIP      | ~ bareit & (ttioskp | tt40ioskp | rkioskp | vcioskp | tcioskp | tt42ioskp | tt44ioskp | tt46ioskp | prioskp | ppioskp | pbioskp));
    assign dev_i_MEMDONE     = ~ (~ arm_i_MEMDONE     | ~ bareit & ~ xm_mwdone);
    assign dev_i_STROBE      = ~ (~ arm_i_STROBE      | ~ bareit & ~ xm_mrdone);
    assign dev_i_D36B2       = ~ (~ arm_i_D36B2       | ~ bareit & ~ pulsebit);

    ///////////////////////////////////
    //  simulated PDP-8/L processor  //
    ///////////////////////////////////

    // when nanocontin is set to 1 by the arm,
    //   this continuously shifts 1s into nanotrigger and nanoctep and everything runs normally
    // when nanocontin is set to 0 by the arm,
    //   it continuously shifts 0s into nanotrigger and nanocstep and everything stops
    //   but then when a 1 is written to nanotrigger by the arm,
    //     it shifts a 1 into nanocstep for 1 cycle and so everyting steps exactly one cycle
    //     then it resumes shifting 0s into nanotrigger and nanocstep so everything stops
    //     thus cstep lasts for a single cycle separate from an arm write
    always @(posedge CLOCK) begin
        if (~ RESET_N) begin
            nanotrigger <= 0;
            nanocstep   <= 0;
        end else begin
            // we can shift nanotrigger only when it is not being written by the arm
            nanotrigger <= (armwrite & (writeaddr == 10'b0000000101)) ? saxi_WDATA[03] : nanocontin;
            // nanocstep is read-only to the arm so can always be shifted other than reset
            nanocstep   <= nanotrigger;
        end
    end

    assign LEDoutR = simit;
    assign LEDoutG = ~ dev_oB_RUN;
    assign LEDoutB = 1;

    pdp8lsim siminst (
        .CLOCK          (CLOCK),
        .CSTEP          (nanocstep),
        .RESET          (pwronreset | ~ simit),
        .iBEMA          (sim_iBEMA),
        .i_CA_INCRMNT   (sim_i_CA_INCRMNT),
        .i_DATA_IN      (sim_i_DATA_IN),
        .iINPUTBUS      (sim_iINPUTBUS),
        .iMEMINCR       (sim_iMEMINCR),
        .i_MEM          (sim_i_MEM),
        .i_MEM_P        (sim_i_MEM_P),
        .i3CYCLE        (sim_i3CYCLE),
        .iAC_CLEAR      (sim_iAC_CLEAR),
        .iBRK_RQST      (sim_iBRK_RQST),
        .iDMAADDR       (sim_iDMAADDR),
        .iDMADATA       (sim_iDMADATA),
        .i_EA           (sim_i_EA),
        .iEMA           (sim_iEMA),
        .iINT_INHIBIT   (sim_iINT_INHIBIT),
        .iINT_RQST      (sim_iINT_RQST),
        .iIO_SKIP       (sim_iIO_SKIP),
        .i_MEMDONE      (sim_i_MEMDONE),
        .i_STROBE       (sim_i_STROBE),
        .oBAC           (sim_oBAC),
        .oBIOP1         (sim_oBIOP1),
        .oBIOP2         (sim_oBIOP2),
        .oBIOP4         (sim_oBIOP4),
        .oBMB           (sim_oBMB),
        .oBTP2          (sim_oBTP2),
        .oBTP3          (sim_oBTP3),
        .oBTS_1         (sim_oBTS_1),
        .oBTS_3         (sim_oBTS_3),
        .o_BWC_OVERFLOW (sim_o_BWC_OVERFLOW),
        .o_B_BREAK      (sim_o_B_BREAK),
        .oE_SET_F_SET   (sim_oE_SET_F_SET),
        .oJMP_JMS       (sim_oJMP_JMS),
        .oLINE_LOW      (sim_oLINE_LOW),
        .oMA            (sim_oMA),
        .oMEMSTART      (sim_oMEMSTART),
        .o_ADDR_ACCEPT  (sim_o_ADDR_ACCEPT),
        .o_BF_ENABLE    (sim_o_BF_ENABLE),
        .oBUSINIT       (sim_oBUSINIT),
        .oB_RUN         (sim_oB_RUN),
        .o_DF_ENABLE    (sim_o_DF_ENABLE),
        .o_KEY_CLEAR    (sim_o_KEY_CLEAR),
        .o_KEY_DF       (sim_o_KEY_DF),
        .o_KEY_IF       (sim_o_KEY_IF),
        .o_KEY_LOAD     (sim_o_KEY_LOAD),
        .o_LOAD_SF      (sim_o_LOAD_SF),
        .o_SP_CYC_NEXT  (sim_o_SP_CYC_NEXT),
        .lbAC           (sim_lbAC),
        .lbBRK          (sim_lbBRK),
        .lbCA           (sim_lbCA),
        .lbDEF          (sim_lbDEF),
        .lbEA           (sim_lbEA),
        .lbEXE          (sim_lbEXE),
        .lbFET          (sim_lbFET),
        .lbION          (sim_lbION),
        .lbIR           (sim_lbIR),
        .lbLINK         (sim_lbLINK),
        .lbMA           (sim_lbMA),
        .lbMB           (sim_lbMB),
        .lbRUN          (sim_lbRUN),
        .lbWC           (sim_lbWC),
        .swCONT         (arm_swCONT),
        .swDEP          (arm_swDEP),
        .swDFLD         (arm_swDFLD),
        .swEXAM         (arm_swEXAM),
        .swIFLD         (arm_swIFLD),
        .swLDAD         (arm_swLDAD),
        .swMPRT         (arm_swMPRT),
        .swSTEP         (arm_swSTEP),
        .swSTOP         (arm_swSTOP),
        .swSR           (arm_swSR),
        .swSTART        (arm_swSTART),

        .majstate       (regctlk[03:00]),
        .timedelay      (regctlk[09:04]),
        .timestate      (regctlk[14:10]),
        .cyclectr       (regctlk[24:15]),
        .nextmajst      (regctlk[28:25]),
        .debounced      (debounced),
        .lastswLDAD     (lastswLDAD),
        .lastswSTART    (lastswSTART),
        .memen          (simmemen)
    );

    assign regctlk[31:29] = 0;

    ////////////////////////////////
    //  shadow PDP-8/L processor  //
    ////////////////////////////////

    wire shad_error, shad_freeze;
    wire[3:0] shad_tstate;
    wire[3:0] shad_tdelay;

    pdp8lshad shadinst (
        .CLOCK (CLOCK),
        .CSTEP (nanocstep),
        .RESET (pwronreset),

        .armwrite (shawrite),
        .armraddr (readaddr[4:2]),
        .armwaddr (writeaddr[4:2]),
        .armwdata (saxi_WDATA),
        .armrdata (shardata),

        .iBEMA          (dev_iBEMA),
        .i_CA_INCRMNT   (dev_i_CA_INCRMNT),
        .i_DATA_IN      (dev_i_DATA_IN),
        .iINPUTBUS      (dev_iINPUTBUS),
        .iMEMINCR       (dev_iMEMINCR),
        .i_MEM          (dev_i_MEM),
        .i_MEM_P        (dev_i_MEM_P),
        .i3CYCLE        (dev_i3CYCLE),
        .iAC_CLEAR      (dev_iAC_CLEAR),
        .iBRK_RQST      (dev_iBRK_RQST),
        .iDMAADDR       (dev_iDMAADDR),
        .iDMADATA       (dev_iDMADATA),
        .i_EA           (dev_i_EA),
        .iEMA           (dev_iEMA),
        .iINT_INHIBIT   (dev_iINT_INHIBIT),
        .iINT_RQST      (dev_iINT_RQST),
        .iIO_SKIP       (dev_iIO_SKIP),
        .i_MEMDONE      (dev_i_MEMDONE),
        .i_STROBE       (dev_i_STROBE),

        .oBAC           (dev_oBAC),
        .oBIOP1         (dev_oBIOP1),
        .oBIOP2         (dev_oBIOP2),
        .oBIOP4         (dev_oBIOP4),
        .oBMB           (dev_oBMB),
        .oBTP2          (dev_oBTP2),
        .oBTP3          (dev_oBTP3),
        .oBTS_1         (dev_oBTS_1),
        .oBTS_3         (dev_oBTS_3),
        .o_BWC_OVERFLOW (dev_o_BWC_OVERFLOW),
        .o_B_BREAK      (dev_o_B_BREAK),
        .oE_SET_F_SET   (dev_oE_SET_F_SET),
        .oJMP_JMS       (dev_oJMP_JMS),
        .oLINE_LOW      (dev_oLINE_LOW),
        .oMA            (dev_oMA),
        .oMEMSTART      (dev_oMEMSTART),
        .o_ADDR_ACCEPT  (dev_o_ADDR_ACCEPT),
        .o_BF_ENABLE    (dev_o_BF_ENABLE),
        .oBUSINIT       (dev_oBUSINIT),
        .oB_RUN         (dev_oB_RUN),
        .o_DF_ENABLE    (dev_o_DF_ENABLE),
        .o_KEY_CLEAR    (dev_o_KEY_CLEAR),
        .o_KEY_DF       (dev_o_KEY_DF),
        .o_KEY_IF       (dev_o_KEY_IF),
        .o_KEY_LOAD     (dev_o_KEY_LOAD),
        .o_LOAD_SF      (dev_o_LOAD_SF),
        .o_SP_CYC_NEXT  (dev_o_SP_CYC_NEXT),

        .timestate      (shad_tstate),
        .timedelay      (shad_tdelay),
        .error          (shad_error),
        .freeze         (shad_freeze)
    );

    /////////////////////
    //  io interfaces  //
    /////////////////////

    wire iobusreset = pwronreset | dev_oBUSINIT;    // power on reset or start switch

    // generate iopstart pulse for an io instruction followed by iopstop
    //  iopstart is pulsed 130nS after the first iop for an instruction and lasts a single CSTEP clock cycle
    //   it is delayed if armwrite is happening at the same time
    //   interfaces know they can decode the io opcode in dev_oBMB and drive the busses
    //  iopstop is turned on 70nS after the end of that same iop and lasts dozens or more cycles
    //   interfaces must stop driving busses at this time
    //   it may happen same time as armwrite but since it lasts a long time, it will be seen on subsequent cycles

    // interfaces are assumed to have this form:
    //   if pwronreset do pwronreset processing
    //   else if armwrite do armwrite processing    // must take priority over iopstart cuz it lasts only 1 cycle
    //   else if (CSTEP) begin
    //     if iopstart do iopstart processing       // single cycle delayed if same time as armwrite
    //     else if iopstop do iopstop processing    // lasts for dozens of cycles
    //   end

    always @(posedge CLOCK) begin
        if (pwronreset) begin
            didio       <=  0;
            iopsetcount <=  0;
            iopclrcount <= 15;
        end else if (nanocstep) begin
            if (lastts1 & ~ dev_oBTS_1) didio <= 0;
            else if (dev_oBIOP1 | dev_oBIOP2 | dev_oBIOP4) begin
                // somewhere inside any IOPn for an instruction
                // 130nS into it, blip a iopstart pulse for first IOP of the instruction for one CSTEP clock
                didio       <= 1;
                iopclrcount <= 0;
                if ((iopsetcount < 13) | (iopsetcount == 13 & ~ armwrite) | (iopsetcount == 14)) begin
                    iopsetcount <= iopsetcount + 1;
                end
            end else begin
                // somewhere outside an IOPn for an instruction
                // 150nS into the void, raise and hold the stop signal until next iopulse (from this or other IO instruction)
                iopsetcount <= 0;
                if (iopclrcount != 15) begin
                    iopclrcount <= iopclrcount + 1;
                end
            end
        end
    end

    wire firstiop =
        (dev_oBIOP1 & dev_oBMB[0]   ==   1'b1) |    // any 110xxxxxxxx1 executes only on IOP1
        (dev_oBIOP2 & dev_oBMB[1:0] ==  2'b10) |    // any 110xxxxxxx10 executes only on IOP2
        (dev_oBIOP4 & dev_oBMB[2:0] == 3'b100);     // any 110xxxxxx100 executes only on IOP4
                                                    // any 110xxxxxx000 never executes

    assign iopstart = iopsetcount == 13 & firstiop & ~ armwrite;    // IOP started 130nS ago, process the opcode in dev_oBMB, start driving busses
    assign iopstop  = iopclrcount == 15;                            // IOP finished 150nS ago, stop driving io busses (output zeroes)

    // teletype interfaces

    pdp8ltty ttinst (
        .CLOCK (CLOCK),
        .CSTEP (nanocstep),
        .RESET (pwronreset),
        .BINIT (iobusreset),

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

    pdp8ltty #(.KBDEV (6'o40)) tt40inst (
        .CLOCK (CLOCK),
        .CSTEP (nanocstep),
        .RESET (pwronreset),
        .BINIT (iobusreset),

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

    pdp8ltty #(.KBDEV (6'o42)) tt42inst (
        .CLOCK (CLOCK),
        .CSTEP (nanocstep),
        .RESET (pwronreset),
        .BINIT (iobusreset),

        .armwrite (tt42awrite),
        .armraddr (readaddr[3:2]),
        .armwaddr (writeaddr[3:2]),
        .armwdata (saxi_WDATA),
        .armrdata (tt42ardata),

        .iopstart (iopstart),
        .iopstop  (iopstop),
        .ioopcode (dev_oBMB),
        .cputodev (dev_oBAC),

        .devtocpu (tt42ibus),
        .AC_CLEAR (tt42acclr),
        .IO_SKIP  (tt42ioskp),
        .INT_RQST (tt42intrq)
    );

    pdp8ltty #(.KBDEV (6'o44)) tt44inst (
        .CLOCK (CLOCK),
        .CSTEP (nanocstep),
        .RESET (pwronreset),
        .BINIT (iobusreset),

        .armwrite (tt44awrite),
        .armraddr (readaddr[3:2]),
        .armwaddr (writeaddr[3:2]),
        .armwdata (saxi_WDATA),
        .armrdata (tt44ardata),

        .iopstart (iopstart),
        .iopstop  (iopstop),
        .ioopcode (dev_oBMB),
        .cputodev (dev_oBAC),

        .devtocpu (tt44ibus),
        .AC_CLEAR (tt44acclr),
        .IO_SKIP  (tt44ioskp),
        .INT_RQST (tt44intrq)
    );

    pdp8ltty #(.KBDEV (6'o46)) tt46inst (
        .CLOCK (CLOCK),
        .CSTEP (nanocstep),
        .RESET (pwronreset),
        .BINIT (iobusreset),

        .armwrite (tt46awrite),
        .armraddr (readaddr[3:2]),
        .armwaddr (writeaddr[3:2]),
        .armwdata (saxi_WDATA),
        .armrdata (tt46ardata),

        .iopstart (iopstart),
        .iopstop  (iopstop),
        .ioopcode (dev_oBMB),
        .cputodev (dev_oBAC),

        .devtocpu (tt46ibus),
        .AC_CLEAR (tt46acclr),
        .IO_SKIP  (tt46ioskp),
        .INT_RQST (tt46intrq)
    );

    // disk interface

    pdp8lrk8je rkinst (
        .CLOCK (CLOCK),
        .CSTEP (nanocstep),
        .RESET (pwronreset),
        .BINIT (iobusreset),

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
    // gives arm and pdp access to the extended memory block
    // arm always has access to the full 32K
    // pdp always has access to the upper 28K
    // pdp can be given access to the lower 4K (disabling its access to its 4K core)

    wire[14:00] cmxbraddr, xmxbraddr;
    wire[11:00] cmxbrwdat, xmxbrwdat;
    wire cmxbrenab, cmxbrwena, xmxbrenab, xmxbrwena;

    assign xbraddr = cmxbraddr | xmxbraddr;
    assign xbrwdat = cmxbrwdat | xmxbrwdat;
    assign xbrenab = cmxbrenab | xmxbrenab;
    assign xbrwena = cmxbrwena | xmxbrwena;

    wire xmemblock, xmemidle;

    wire[3:0] xmstate;
    wire[2:0] xmdfld;

    pdp8lxmem xminst (
        .CLOCK (CLOCK),
        .CSTEP (nanocstep),
        .RESET (pwronreset),
        .BINIT (iobusreset),

        .armwrite (xmawrite),           // arm writing to backside register
        .armraddr (readaddr[4:2]),      // arm reading from backside register
        .armwaddr (writeaddr[4:2]),     // arm writing to backside register
        .armwdata (saxi_WDATA),         // data to write to backside register
        .armrdata (xmardata),           // data being read from backside register

        .iopstart (iopstart),           // an io iopcode is being executed by the cpu
        .iopstop  (iopstop),            // the io cycle has ended
        .ioopcode (dev_oBMB),           // io opcode
        .cputodev (dev_oBAC),           // data being sent to device
        .devtocpu (xmibus),             // data being received from device

        .freeze   (shad_freeze),        //< freeze on shadow error

        .memstart (dev_oMEMSTART),      // pulse to start reading
        .memaddr  (dev_oMA),            // address within memory
        .memwdat  (dev_oBMB),           // data being written to memory
        .memrdat  (xmmem),              // data that was read from memory
        ._mrdone  (xm_mrdone),          // pulse indicating read data is valid
        ._mwdone  (xm_mwdone),          // pulse indicating write has completed

        .brkfld (cmbrkema),             // extended memory address for break (dma) cycles
        .field  (xmfield),

        ._bf_enab (dev_o_BF_ENABLE),    // next mem cycle should use break field
        ._df_enab (dev_o_DF_ENABLE),    // next mem cycle should use data field
        .exefet   (dev_oE_SET_F_SET),   // next mem cycle is for fetch or execute
        ._intack  (dev_o_LOAD_SF),      // next mem cycle is ackmowledging interrupt
        .jmpjms   (dev_oJMP_JMS),       // instruction register holds JMP or JMS instruction
        .ts1      (dev_oBTS_1),
        .ts3      (dev_oBTS_3),
        .tp3      (dev_oBTP3),
        ._zf_enab (dev_o_SP_CYC_NEXT),  // special (WC or CA) cycle is next
        ._ea      (xm_ea),              // _EA=1 use 4K core stack and cpu's controller; _EA=0 use this controller
        ._intinh  (xm_intinh),          // block interrupt delivery

        .meminprog (meminprog),         //> counts down fpga cycles after memstart
        .r_MA      (dev_r_MA),          //> gate memory address in from memory bus
        .x_MEM     (dev_x_MEM),         //> gate memory data out to memory bus
        .hizmembus (dev_hizmembus),     //> hi-Z MEMBUS lines

        .ldaddrsw (~ dev_o_KEY_LOAD),
        .ldaddfld ({ 2'b00, ~ dev_o_KEY_DF }),
        .ldadifld ({ 2'b00, ~ dev_o_KEY_IF }),

        .xmemblock (xmemblock),         //< block xmem from starting a cycle
        .xmemidle  (xmemidle),          //> xmem is not doing a cycle

        .xbraddr (xmxbraddr),
        .xbrwdat (xmxbrwdat),
        .xbrrdat (xbrrdat),
        .xbrenab (xmxbrenab),
        .xbrwena (xmxbrwena)

        ,.xmstate (xmstate)
        ,.dfld (xmdfld)
    );

    // core memory interface
    // gives arm access to the pdp's 4K core memory via pdp's dma interface
    // can also access the upper 28K via dma for testing
    // pdp must be running for this to work
    // does not process any pdp io instructions

    wire cmnobrkopt;
    wire[4:0] cmbusyonarm;

    pdp8lcmem cminst (
        .CLOCK (CLOCK),
        .CSTEP (nanocstep),
        .RESET (pwronreset),

        .armwrite (cmawrite),                   //< arm writing to backside register
        .armraddr (readaddr[3:2]),              //< arm reading from backside register
        .armwaddr (writeaddr[3:2]),             //< arm writing to backside register
        .armwdata (saxi_WDATA),                 //< data to write to backside register
        .armrdata (cmardata),                   //> data being read from backside register

        .brkrqst  (cmbrkrqst),                  //> dma cycle being requested
        .brkwrite (cmbrkwrite),                 //> dma cycle wants to write memory
        .brk3cycl (cmbrk3cycl),                 //> dma 3-cycle
        .brkcainc (cmbrkcainc),                 //> increment current address in 3-cycle
        .brkema   (cmbrkema),                   //> upper address bits
        .brkaddr  (cmbrkaddr),                  //> lower address bits
        .brkwdat  (cmbrkwdat),                  //> data to write to memory
        .brkrdat  (dev_oBMB),                   //< data read from memory
        .brkcycle (~ dev_o_B_BREAK),            //< this is the BREAK cycle
        .brkts1   (dev_oBTS_1),                 //< TS1 of memory cycle (memory read occurring)
        .brkts3   (dev_oBTS_3),                 //< TS3 of memory cycle (memory writeback occurring)
        .brkwcovf (~ dev_o_BWC_OVERFLOW),       //< wordcount overflow for 3-cycle

        .xmemblock (xmemblock),                 //> block xmem from starting a cycle
        .xmemidle  (xmemidle),                  //< xmem is not doing a cycle

        .xbraddr (cmxbraddr),
        .xbrwdat (cmxbrwdat),
        .xbrenab (cmxbrenab),
        .xbrwena (cmxbrwena),
        .xbrrdat (xbrrdat)

        ,.nobrkopt (cmnobrkopt)
        ,.busyonarm (cmbusyonarm)
    );

    // tape interface

    pdp8ltc08 tcinst (
        .CLOCK (CLOCK),
        .CSTEP (nanocstep),
        .RESET (pwronreset),
        .BINIT (iobusreset),

        .armwrite (tcawrite),
        .armraddr (readaddr[2]),
        .armwaddr (writeaddr[2]),
        .armwdata (saxi_WDATA),
        .armrdata (tcardata),

        .iopstart (iopstart),
        .iopstop  (iopstop),
        .ioopcode (dev_oBMB),
        .cputodev (dev_oBAC),

        .devtocpu (tcibus),
        .AC_CLEAR (tcacclr),
        .IO_SKIP  (tcioskp),
        .INT_RQST (tcintrq)
    );

    // video interface

    pdp8lvc8 vcinst (
        .CLOCK (CLOCK),
        .CSTEP (nanocstep),
        .RESET (pwronreset),
        .BINIT (iobusreset),

        .armwrite (vcawrite),
        .armraddr (readaddr[4:2]),
        .armwaddr (writeaddr[4:2]),
        .armwdata (saxi_WDATA),
        .armrdata (vcardata),

        .iopstart (iopstart),
        .iopstop  (iopstop),
        .ioopcode (dev_oBMB),
        .cputodev (dev_oBAC),

        .devtocpu (vcibus),
        .AC_CLEAR (vcacclr),
        .IO_SKIP  (vcioskp),
        .INT_RQST (vcintrq),

        .vidaddra (vidaddra),
        .vidaddrb (vidaddrb),
        .viddataa (viddataa),
        .videnaba (videnaba),
        .vidwrena (vidwrena),
        .videnabb (videnabb),
        .viddatab (viddatab)
    );

    // real PDP-8/L front panel i2c interface
    // connects to pipan8l/pcb board i2c interface

    reg fpi2cdathiz;
    wire fpi2cclo, fpi2cdao;

    assign bFPI2CLOCK = fpi2cclo;                   // clock line 74AXP245 is wired as totem pole always on
    assign bFPI2CDATA = fpi2cdathiz ? 1'bZ : 1'b0;  // FPGA data pin is always either in hi-Z or sending a zero

    always @(posedge CLOCK) begin
        if (~ RESET_N) begin
            i_FPI2CDENA <= 1;
            iFPI2CDDIR  <= 1;
            fpi2cdathiz <= 1;
        end else begin

            // send out a zero whenever dataout is zero
            if (~ fpi2cdao) begin
                if (~ iFPI2CDDIR) begin
                    iFPI2CDDIR  <= 1;               // 74AXP4T245 was receiving, change to sending first
                end else begin
                    fpi2cdathiz <= 0;               // now it is safe to turn on FPGA pin and drive a zero
                    i_FPI2CDENA <= 0;               // make sure 74AXP4T245 is turned on
                end
            end

            // receive whenever dataout is one
            else begin
                if (~ fpi2cdathiz) begin
                    fpi2cdathiz <= 1;              // FPGA was sending, change to receiving first
                end else begin
                    iFPI2CDDIR  <= 0;              // make sure 74AXP4T245 is receiving
                    i_FPI2CDENA <= 0;              // make sure 74AXP4T245 is turned on
                end
            end
        end
    end

    wire[2:0] i2cstate;
    wire[14:00] i2ccount;
    wire[63:00] i2cstatus;
    pdp8lfpi2c fpi2cinst (
        .CLOCK (CLOCK),
        .RESET (pwronreset),

        .armwrite (fpi2cwrite),
        .armraddr (readaddr[4:2]),
        .armwaddr (writeaddr[4:2]),
        .armwdata (saxi_WDATA),
        .armrdata (fpi2crdata),

        .i2cclo (fpi2cclo),
        .i2ccli (bFPI2CLOCK),
        .i2cdao (fpi2cdao),
        .i2cdai (bFPI2CDATA),
        .i2cstate (i2cstate),
        .i2ccount (i2ccount),
        .status (i2cstatus)
    );

    // paper tape reader interface
    pdp8lptr prinst (
        .CLOCK (CLOCK),
        .CSTEP (nanocstep),
        .RESET (pwronreset),
        .BINIT (iobusreset),

        .armwrite (prawrite),
        .armraddr (readaddr[2]),
        .armwaddr (writeaddr[2]),
        .armwdata (saxi_WDATA),
        .armrdata (prardata),

        .iopstart (iopstart),
        .iopstop  (iopstop),
        .ioopcode (dev_oBMB),
        .cputodev (dev_oBAC),

        .devtocpu (pribus),
        .AC_CLEAR (pracclr),
        .IO_SKIP  (prioskp),
        .INT_RQST (printrq)
    );

    // pulse bit interface
    pdp8lpbit pbinst (
        .CLOCK (CLOCK),
        .CSTEP (nanocstep),
        .RESET (pwronreset),

        .armwrite (pbawrite),
        .armraddr (readaddr[3:2]),
        .armwaddr (writeaddr[3:2]),
        .armwdata (saxi_WDATA),
        .armrdata (pbardata),

        .iopstart (iopstart),
        .iopstop  (iopstop),
        .ioopcode (dev_oBMB),
        .cputodev (dev_oBAC),

        .devtocpu (pbibus),
        .AC_CLEAR (pbacclr),
        .IO_SKIP  (pbioskp),

        .pulse (pulsebit)
    );

    // paper tape punch interface
    pdp8lptp ppinst (
        .CLOCK (CLOCK),
        .CSTEP (nanocstep),
        .RESET (pwronreset),
        .BINIT (iobusreset),

        .armwrite (ppawrite),
        .armraddr (readaddr[2]),
        .armwaddr (writeaddr[2]),
        .armwdata (saxi_WDATA),
        .armrdata (ppardata),

        .iopstart (iopstart),
        .iopstop  (iopstop),
        .ioopcode (dev_oBMB),
        .cputodev (dev_oBAC),

        .IO_SKIP  (ppioskp),
        .INT_RQST (ppintrq)
    );

    // integrated logic analyzer
    //  ilaarmed = 0: trigger condition satisfied
    //             1: waiting for trigger condition
    //  ilaafter = number of cycles to record after trigger condition satisfied
    //  ilaindex = next entry in ilaarray to write

    assign i_B36V1 = ilaarmed;

    always @(posedge CLOCK) begin
        if (~ RESET_N) begin
            ilaarmed <= 0;
            ilaafter <= 0;
        end else begin

            if (armwrite & (writeaddr == 10'b0000010001)) begin

                // arm processor is writing control register
                ilaarmed <= saxi_WDATA[31];
                ilaafter <= saxi_WDATA[27:16];
                ilaindex <= saxi_WDATA[11:00];
                ilardata <= ilaarray[saxi_WDATA[11:00]];
            end else begin

                // capture signals while before trigger and for ilaafter cycles thereafter
                if (ilaarmed | (ilaafter != 0)) begin
                    ilaarray[ilaindex] <= {
                        xmstate,
                        cmnobrkopt,
                        cmbusyonarm,
                        xbraddr,
                        xbrrdat,
                        xbrwdat,
                        cmxbrenab,
                        cmxbrwena,
                        xmxbrenab,
                        xmxbrwena
                    };

                    ilaindex <= ilaindex + 1;
                    if (~ ilaarmed) ilaafter <= ilaafter - 1;
                end

                // check trigger condition
                // - disk controller requesting dma cycle
                if (cmawrite & (writeaddr[3:2] == 1) & saxi_WDATA[31]) begin
                    ilaarmed <= 0;
                end
            end
        end
    end
endmodule
