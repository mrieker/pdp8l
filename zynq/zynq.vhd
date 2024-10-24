--    Copyright (C) Mike Rieker, Beverly, MA USA
--    www.outerworldapps.com
--
--    This program is free software; you can redistribute it and/or modify
--    it under the terms of the GNU General Public License as published by
--    the Free Software Foundation; version 2 of the License.
--
--    This program is distributed in the hope that it will be useful,
--    but WITHOUT ANY WARRANTY; without even the implied warranty of
--    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
--    GNU General Public License for more details.
--
--    EXPECT it to FAIL when someone's HeALTh or PROpeRTy is at RISk.
--
--    You should have received a copy of the GNU General Public License
--    along with this program; if not, write to the Free Software
--    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
--
--    http://www.gnu.org/licenses/gpl-2.0.html

-- main program for the zynq implementation
-- contains gpio-like and paddle registers accessed via the axi bus
-- also contains a dma circuit just for testing dma code (not used for pdp-8)
--  and contains a led pwm circuit just for testing led (not used for pdp-8)

library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.NUMERIC_STD.ALL;

entity Zynq is
    Port (  CLOCK : in STD_LOGIC;
            RESET_N : in STD_LOGIC;
            LEDoutR : out STD_LOGIC;     -- IO_B34_LN6 R14
            LEDoutG : out STD_LOGIC;     -- IO_B34_LP7 Y16
            LEDoutB : out STD_LOGIC;     -- IO_B34_LN7 Y17

            xnanostep : out STD_LOGIC;
            xlastnano : out STD_LOGIC;
            xnanocycle : out STD_LOGIC;

            -- arm processor memory bus interface (AXI)
            -- we are a slave for accessing the control registers (read & write)
            saxi_ARADDR : in std_logic_vector (11 downto 0);
            saxi_ARREADY : out std_logic;
            saxi_ARVALID : in std_logic;
            saxi_AWADDR : in std_logic_vector (11 downto 0);
            saxi_AWREADY : out std_logic;
            saxi_AWVALID : in std_logic;
            saxi_BREADY : in std_logic;
            saxi_BRESP : out std_logic_vector (1 downto 0);
            saxi_BVALID : out std_logic;
            saxi_RDATA : out std_logic_vector (31 downto 0);
            saxi_RREADY : in std_logic;
            saxi_RRESP : out std_logic_vector (1 downto 0);
            saxi_RVALID : out std_logic;
            saxi_WDATA : in std_logic_vector (31 downto 0);
            saxi_WREADY : out std_logic;
            saxi_WVALID : in std_logic;

            -- - we are a master for accessing the ring buffer (read only)
            maxi_ARADDR : out std_logic_vector (31 downto 0);
            maxi_ARBURST : out std_logic_vector (1 downto 0);
            maxi_ARCACHE : out std_logic_vector (3 downto 0);
            maxi_ARID : out std_logic_vector (0 downto 0);
            maxi_ARLEN : out std_logic_vector (7 downto 0);
            maxi_ARLOCK : out std_logic_vector (1 downto 0);
            maxi_ARPROT : out std_logic_vector (2 downto 0);
            maxi_ARQOS : out std_logic_vector (3 downto 0);
            maxi_ARREADY : in std_logic;
            maxi_ARREGION : out std_logic_vector (3 downto 0);
            maxi_ARSIZE : out std_logic_vector (2 downto 0);
            maxi_ARUSER : out std_logic_vector (0 downto 0);
            maxi_ARVALID : out std_logic;

            maxi_AWADDR : out std_logic_vector (31 downto 0);
            maxi_AWBURST : out std_logic_vector (1 downto 0);
            maxi_AWCACHE : out std_logic_vector (3 downto 0);
            maxi_AWID : out std_logic_vector (0 downto 0);
            maxi_AWLEN : out std_logic_vector (7 downto 0);
            maxi_AWLOCK : out std_logic_vector (1 downto 0);
            maxi_AWPROT : out std_logic_vector (2 downto 0);
            maxi_AWQOS : out std_logic_vector (3 downto 0);
            maxi_AWREADY : in std_logic;
            maxi_AWREGION : out std_logic_vector (3 downto 0);
            maxi_AWSIZE : out std_logic_vector (2 downto 0);
            maxi_AWUSER : out std_logic_vector (0 downto 0);
            maxi_AWVALID : out std_logic;

            maxi_BREADY : out std_logic;
            maxi_BVALID : in std_logic;

            maxi_RDATA : in std_logic_vector (31 downto 0);
            maxi_RLAST : in std_logic;
            maxi_RREADY : out std_logic;
            maxi_RVALID : in std_logic;

            maxi_WDATA : out std_logic_vector (31 downto 0);
            maxi_WLAST : out std_logic;
            maxi_WREADY : in std_logic;
            maxi_WSTRB : out std_logic_vector (3 downto 0);
            maxi_WUSER : out std_logic_vector (0 downto 0);
            maxi_WVALID : out std_logic);
end Zynq;

architecture rtl of Zynq is

    -- declare axi slave port signals (used by sim ps code to access our control registers)
    ATTRIBUTE X_INTERFACE_INFO : STRING;
    ATTRIBUTE X_INTERFACE_INFO OF saxi_ARADDR: SIGNAL IS "xilinx.com:interface:aximm:1.0 S00_AXI ARADDR";
    ATTRIBUTE X_INTERFACE_INFO OF saxi_ARREADY: SIGNAL IS "xilinx.com:interface:aximm:1.0 S00_AXI ARREADY";
    ATTRIBUTE X_INTERFACE_INFO OF saxi_ARVALID: SIGNAL IS "xilinx.com:interface:aximm:1.0 S00_AXI ARVALID";
    ATTRIBUTE X_INTERFACE_INFO OF saxi_AWADDR: SIGNAL IS "xilinx.com:interface:aximm:1.0 S00_AXI AWADDR";
    ATTRIBUTE X_INTERFACE_INFO OF saxi_AWREADY: SIGNAL IS "xilinx.com:interface:aximm:1.0 S00_AXI AWREADY";
    ATTRIBUTE X_INTERFACE_INFO OF saxi_AWVALID: SIGNAL IS "xilinx.com:interface:aximm:1.0 S00_AXI AWVALID";
    ATTRIBUTE X_INTERFACE_INFO OF saxi_BREADY: SIGNAL IS "xilinx.com:interface:aximm:1.0 S00_AXI BREADY";
    ATTRIBUTE X_INTERFACE_INFO OF saxi_BRESP: SIGNAL IS "xilinx.com:interface:aximm:1.0 S00_AXI BRESP";
    ATTRIBUTE X_INTERFACE_INFO OF saxi_BVALID: SIGNAL IS "xilinx.com:interface:aximm:1.0 S00_AXI BVALID";
    ATTRIBUTE X_INTERFACE_INFO OF saxi_RDATA: SIGNAL IS "xilinx.com:interface:aximm:1.0 S00_AXI RDATA";
    ATTRIBUTE X_INTERFACE_INFO OF saxi_RREADY: SIGNAL IS "xilinx.com:interface:aximm:1.0 S00_AXI RREADY";
    ATTRIBUTE X_INTERFACE_INFO OF saxi_RRESP: SIGNAL IS "xilinx.com:interface:aximm:1.0 S00_AXI RRESP";
    ATTRIBUTE X_INTERFACE_INFO OF saxi_RVALID: SIGNAL IS "xilinx.com:interface:aximm:1.0 S00_AXI RVALID";
    ATTRIBUTE X_INTERFACE_INFO OF saxi_WDATA: SIGNAL IS "xilinx.com:interface:aximm:1.0 S00_AXI WDATA";
    ATTRIBUTE X_INTERFACE_INFO OF saxi_WREADY: SIGNAL IS "xilinx.com:interface:aximm:1.0 S00_AXI WREADY";
    ATTRIBUTE X_INTERFACE_INFO OF saxi_WVALID: SIGNAL IS "xilinx.com:interface:aximm:1.0 S00_AXI WVALID";

    -- declare axi master port signals (used by this code to access ring contents via dma)
    ATTRIBUTE X_INTERFACE_INFO OF maxi_ARADDR: SIGNAL IS "xilinx.com:interface:aximm:1.0 M00_AXI ARADDR";
    ATTRIBUTE X_INTERFACE_INFO OF maxi_ARBURST: SIGNAL IS "xilinx.com:interface:aximm:1.0 M00_AXI ARBURST";
    ATTRIBUTE X_INTERFACE_INFO OF maxi_ARCACHE: SIGNAL IS "xilinx.com:interface:aximm:1.0 M00_AXI ARCACHE";
    ATTRIBUTE X_INTERFACE_INFO OF maxi_ARID: SIGNAL IS "xilinx.com:interface:aximm:1.0 M00_AXI ARID";
    ATTRIBUTE X_INTERFACE_INFO OF maxi_ARLEN: SIGNAL IS "xilinx.com:interface:aximm:1.0 M00_AXI ARLEN";
    ATTRIBUTE X_INTERFACE_INFO OF maxi_ARLOCK: SIGNAL IS "xilinx.com:interface:aximm:1.0 M00_AXI ARLOCK";
    ATTRIBUTE X_INTERFACE_INFO OF maxi_ARPROT: SIGNAL IS "xilinx.com:interface:aximm:1.0 M00_AXI ARPROT";
    ATTRIBUTE X_INTERFACE_INFO OF maxi_ARQOS: SIGNAL IS "xilinx.com:interface:aximm:1.0 M00_AXI ARQOS";
    ATTRIBUTE X_INTERFACE_INFO OF maxi_ARREADY: SIGNAL IS "xilinx.com:interface:aximm:1.0 M00_AXI ARREADY";
    ATTRIBUTE X_INTERFACE_INFO OF maxi_ARREGION: SIGNAL IS "xilinx.com:interface:aximm:1.0 M00_AXI ARREGION";
    ATTRIBUTE X_INTERFACE_INFO OF maxi_ARSIZE: SIGNAL IS "xilinx.com:interface:aximm:1.0 M00_AXI ARSIZE";
    ATTRIBUTE X_INTERFACE_INFO OF maxi_ARUSER: SIGNAL IS "xilinx.com:interface:aximm:1.0 M00_AXI ARUSER";
    ATTRIBUTE X_INTERFACE_INFO OF maxi_ARVALID: SIGNAL IS "xilinx.com:interface:aximm:1.0 M00_AXI ARVALID";

    ATTRIBUTE X_INTERFACE_INFO OF maxi_AWADDR: SIGNAL IS "xilinx.com:interface:aximm:1.0 M00_AXI AWADDR";
    ATTRIBUTE X_INTERFACE_INFO OF maxi_AWBURST: SIGNAL IS "xilinx.com:interface:aximm:1.0 M00_AXI AWBURST";
    ATTRIBUTE X_INTERFACE_INFO OF maxi_AWCACHE: SIGNAL IS "xilinx.com:interface:aximm:1.0 M00_AXI AWCACHE";
    ATTRIBUTE X_INTERFACE_INFO OF maxi_AWID: SIGNAL IS "xilinx.com:interface:aximm:1.0 M00_AXI AWID";
    ATTRIBUTE X_INTERFACE_INFO OF maxi_AWLEN: SIGNAL IS "xilinx.com:interface:aximm:1.0 M00_AXI AWLEN";
    ATTRIBUTE X_INTERFACE_INFO OF maxi_AWLOCK: SIGNAL IS "xilinx.com:interface:aximm:1.0 M00_AXI AWLOCK";
    ATTRIBUTE X_INTERFACE_INFO OF maxi_AWPROT: SIGNAL IS "xilinx.com:interface:aximm:1.0 M00_AXI AWPROT";
    ATTRIBUTE X_INTERFACE_INFO OF maxi_AWQOS: SIGNAL IS "xilinx.com:interface:aximm:1.0 M00_AXI AWQOS";
    ATTRIBUTE X_INTERFACE_INFO OF maxi_AWREADY: SIGNAL IS "xilinx.com:interface:aximm:1.0 M00_AXI AWREADY";
    ATTRIBUTE X_INTERFACE_INFO OF maxi_AWREGION: SIGNAL IS "xilinx.com:interface:aximm:1.0 M00_AXI AWREGION";
    ATTRIBUTE X_INTERFACE_INFO OF maxi_AWSIZE: SIGNAL IS "xilinx.com:interface:aximm:1.0 M00_AXI AWSIZE";
    ATTRIBUTE X_INTERFACE_INFO OF maxi_AWUSER: SIGNAL IS "xilinx.com:interface:aximm:1.0 M00_AXI AWUSER";
    ATTRIBUTE X_INTERFACE_INFO OF maxi_AWVALID: SIGNAL IS "xilinx.com:interface:aximm:1.0 M00_AXI AWVALID";

    ATTRIBUTE X_INTERFACE_INFO OF maxi_BREADY: SIGNAL IS "xilinx.com:interface:aximm:1.0 M00_AXI BREADY";
    ATTRIBUTE X_INTERFACE_INFO OF maxi_BVALID: SIGNAL IS "xilinx.com:interface:aximm:1.0 M00_AXI BVALID";

    ATTRIBUTE X_INTERFACE_INFO OF maxi_RDATA: SIGNAL IS "xilinx.com:interface:aximm:1.0 M00_AXI RDATA";
    ATTRIBUTE X_INTERFACE_INFO OF maxi_RLAST: SIGNAL IS "xilinx.com:interface:aximm:1.0 M00_AXI RLAST";
    ATTRIBUTE X_INTERFACE_INFO OF maxi_RREADY: SIGNAL IS "xilinx.com:interface:aximm:1.0 M00_AXI RREADY";
    ATTRIBUTE X_INTERFACE_INFO OF maxi_RVALID: SIGNAL IS "xilinx.com:interface:aximm:1.0 M00_AXI RVALID";

    ATTRIBUTE X_INTERFACE_INFO OF maxi_WDATA: SIGNAL IS "xilinx.com:interface:aximm:1.0 M00_AXI WDATA";
    ATTRIBUTE X_INTERFACE_INFO OF maxi_WLAST: SIGNAL IS "xilinx.com:interface:aximm:1.0 M00_AXI WLAST";
    ATTRIBUTE X_INTERFACE_INFO OF maxi_WREADY: SIGNAL IS "xilinx.com:interface:aximm:1.0 M00_AXI WREADY";
    ATTRIBUTE X_INTERFACE_INFO OF maxi_WSTRB: SIGNAL IS "xilinx.com:interface:aximm:1.0 M00_AXI WSTRB";
    ATTRIBUTE X_INTERFACE_INFO OF maxi_WUSER: SIGNAL IS "xilinx.com:interface:aximm:1.0 M00_AXI WUSER";
    ATTRIBUTE X_INTERFACE_INFO OF maxi_WVALID: SIGNAL IS "xilinx.com:interface:aximm:1.0 M00_AXI WVALID";

    constant VERSION : std_logic_vector (31 downto 0) := x"384C4025";   -- [31:16] = '8L'; [15:12] = (log2 len)-1; [11:00] = version

    constant BURSTLEN : natural := 10;

    signal saxiARREADY, saxiAWREADY, saxiBVALID, saxiRVALID, saxiWREADY : std_logic;
    signal ledr, ledg, ledb : std_logic;

    constant PERIOD : natural := 1024*1024*256;     -- power of 2

    signal blubright, divider, fader, grnbright, ratio, redbright : natural range 0 to PERIOD-1;
    signal countup : boolean;

    signal rpi_qena, rpi_dena : std_logic;

    signal fpsint, fpsclm, fpsdam, fpsdas : std_logic;
    signal intscl, intsdai, intsdao : std_logic;
    signal fpsda0s : std_logic;

    signal readaddr, writeaddr : std_logic_vector (11 downto 2);

    signal dmardaddr, dmawtaddr : std_logic_vector (31 downto 0);
    signal maxiARVALID, maxiRREADY, maxiAWVALID, maxiWVALID, maxiBREADY : std_logic;

    signal dmareadsel, dmawritesel : natural range 0 to 9;
    signal temp0, temp1, temp2, temp3, temp4, temp5, temp6, temp7, temp8, temp9 : std_logic_vector (31 downto 0);

    -- pdp8/l module signals

    signal regctla, regctlb, regctlc, regctld, regctle, regctlf, regctlg, regctlh, regctli, regctlj, regctlk : std_logic_vector (31 downto 0);

    signal iBEMA         : std_logic;
    signal iCA_INCREMENT : std_logic;
    signal iDATA_IN      : std_logic;
    signal iINPUTBUS     : std_logic_vector (11 downto 0);
    signal iMEMINCR      : std_logic;
    signal iMEM          : std_logic_vector (11 downto 0);
    signal iMEM_P        : std_logic;
    signal iTHREECYCLE   : std_logic;
    signal i_AC_CLEAR    : std_logic;
    signal i_BRK_RQST    : std_logic;
    signal i_DMAADDR     : std_logic_vector (11 downto 0);
    signal i_DMADATA     : std_logic_vector (11 downto 0);
    signal i_EA          : std_logic;
    signal i_EMA         : std_logic;
    signal i_INT_INHIBIT : std_logic;
    signal i_INT_RQST    : std_logic;
    signal i_IO_SKIP     : std_logic;
    signal i_MEMDONE     : std_logic;
    signal i_STROBE      : std_logic;
    signal oBAC          : std_logic_vector (11 downto 0);
    signal oBIOP1        : std_logic;
    signal oBIOP2        : std_logic;
    signal oBIOP4        : std_logic;
    signal oBMB          : std_logic_vector (11 downto 0);
    signal oBTP2         : std_logic;
    signal oBTP3         : std_logic;
    signal oBTS_1        : std_logic;
    signal oBTS_3        : std_logic;
    signal oBUSINIT      : std_logic;
    signal oBWC_OVERFLOW : std_logic;
    signal oB_BREAK      : std_logic;
    signal oE_SET_F_SET  : std_logic;
    signal oJMP_JMS      : std_logic;
    signal oLINE_LOW     : std_logic;
    signal oMA           : std_logic_vector (11 downto 0);
    signal oMEMSTART     : std_logic;
    signal o_ADDR_ACCEPT : std_logic;
    signal o_BF_ENABLE   : std_logic;
    signal o_BUSINIT     : std_logic;
    signal o_B_RUN       : std_logic;
    signal o_DF_ENABLE   : std_logic;
    signal o_KEY_CLEAR   : std_logic;
    signal o_KEY_DF      : std_logic;
    signal o_KEY_IF      : std_logic;
    signal o_KEY_LOAD    : std_logic;
    signal o_LOAD_SF     : std_logic;
    signal o_SP_CYC_NEXT : std_logic;

    signal lbAC    : std_logic_vector (11 downto 0);
    signal lbBRK   : std_logic;
    signal lbCA    : std_logic;
    signal lbDEF   : std_logic;
    signal lbEA    : std_logic;
    signal lbEXE   : std_logic;
    signal lbFET   : std_logic;
    signal lbION   : std_logic;
    signal lbIR    : std_logic_vector (2 downto 0);
    signal lbLINK  : std_logic;
    signal lbMA    : std_logic_vector (11 downto 0);
    signal lbMB    : std_logic_vector (11 downto 0);
    signal lbRUN   : std_logic;
    signal lbWC    : std_logic;
    signal swCONT  : std_logic;
    signal swDEP   : std_logic;
    signal swDFLD  : std_logic;
    signal swEXAM  : std_logic;
    signal swIFLD  : std_logic;
    signal swLDAD  : std_logic;
    signal swMPRT  : std_logic;
    signal swSTEP  : std_logic;
    signal swSTOP  : std_logic;
    signal swSR    : std_logic_vector (11 downto 0);
    signal swSTART : std_logic;

    signal lastnanostep, nanocycle, nanostep, softreset, testioins : std_logic;
    signal inuseclock, inusereset : std_logic;

    -- arm interface signals
    signal arm_acclr, arm_intrq, arm_ioskip : std_logic;
    signal armibus : std_logic_vector (11 downto 0);

    -- tty interface signals
    signal ttardata : std_logic_vector (31 downto 0);
    signal ttibus : std_logic_vector (11 downto 0);
    signal ttawpuls, ttacclr, ttintrq, ttioskip : std_logic;
    signal tt40ardata : std_logic_vector (31 downto 0);
    signal tt40ibus : std_logic_vector (11 downto 0);
    signal tt40awpuls, tt40acclr, tt40intrq, tt40ioskip : std_logic;

component pdp8l port (
    CLOCK : in std_logic;
    RESET : in std_logic;

    iBEMA         : in std_logic;
    iCA_INCREMENT : in std_logic;
    iDATA_IN      : in std_logic;
    iINPUTBUS     : in std_logic_vector (11 downto 0);
    iMEMINCR      : in std_logic;
    iMEM          : in std_logic_vector (11 downto 0);
    iMEM_P        : in std_logic;
    iTHREECYCLE   : in std_logic;
    i_AC_CLEAR    : in std_logic;
    i_BRK_RQST    : in std_logic;
    i_DMAADDR     : in std_logic_vector (11 downto 0);
    i_DMADATA     : in std_logic_vector (11 downto 0);
    i_EA          : in std_logic;
    i_EMA         : in std_logic;
    i_INT_INHIBIT : in std_logic;
    i_INT_RQST    : in std_logic;
    i_IO_SKIP     : in std_logic;
    i_MEMDONE     : in std_logic;
    i_STROBE      : in std_logic;
    oBAC          : out std_logic_vector (11 downto 0);
    oBIOP1        : out std_logic;
    oBIOP2        : out std_logic;
    oBIOP4        : out std_logic;
    oBMB          : out std_logic_vector (11 downto 0);
    oBTP2         : out std_logic;
    oBTP3         : out std_logic;
    oBTS_1        : out std_logic;
    oBTS_3        : out std_logic;
    oBUSINIT      : out std_logic;
    oBWC_OVERFLOW : out std_logic;
    oB_BREAK      : out std_logic;
    oE_SET_F_SET  : out std_logic;
    oJMP_JMS      : out std_logic;
    oLINE_LOW     : out std_logic;
    oMA           : out std_logic_vector (11 downto 0);
    oMEMSTART     : out std_logic;
    o_ADDR_ACCEPT : out std_logic;
    o_BF_ENABLE   : out std_logic;
    o_BUSINIT     : out std_logic;
    o_B_RUN       : out std_logic;
    o_DF_ENABLE   : out std_logic;
    o_KEY_CLEAR   : out std_logic;
    o_KEY_DF      : out std_logic;
    o_KEY_IF      : out std_logic;
    o_KEY_LOAD    : out std_logic;
    o_LOAD_SF     : out std_logic;
    o_SP_CYC_NEXT : out std_logic;

    lbAC    : out std_logic_vector (11 downto 0);
    lbBRK   : out std_logic;
    lbCA    : out std_logic;
    lbDEF   : out std_logic;
    lbEA    : out std_logic;
    lbEXE   : out std_logic;
    lbFET   : out std_logic;
    lbION   : out std_logic;
    lbIR    : out std_logic_vector (2 downto 0);
    lbLINK  : out std_logic;
    lbMA    : out std_logic_vector (11 downto 0);
    lbMB    : out std_logic_vector (11 downto 0);
    lbRUN   : out std_logic;
    lbWC    : out std_logic;
    swCONT  : in std_logic;
    swDEP   : in std_logic;
    swDFLD  : in std_logic;
    swEXAM  : in std_logic;
    swIFLD  : in std_logic;
    swLDAD  : in std_logic;
    swMPRT  : in std_logic;
    swSTEP  : in std_logic;
    swSTOP  : in std_logic;
    swSR    : in std_logic_vector (11 downto 0);
    swSTART : in std_logic

    ;majstate  : out std_logic_vector (2 downto 0)
    ;timedelay : out std_logic_vector (5 downto 0)
    ;timestate : out std_logic_vector (4 downto 0)
    ;cyclectr  : out std_logic_vector (9 downto 0)

    ;nanocycle : in std_logic
    ;nanostep  : in std_logic
    ;lastnanostep : out std_logic
);
end component;

 -- component pdp8ltty port (
 --     CLOCK, RESET : in std_logic;
 --     armwpulse : in std_logic;
 --     armraddr, armwaddr : in std_logic_vector (1 downto 0);
 --     armwdata : in std_logic_vector (31 downto 0);
 --     armrdata : out std_logic_vector (31 downto 0);
 --
 --     INPUTBUS : out std_logic_vector (11 downto 0);
 --     AC_CLEAR : out std_logic;
 --     INT_RQST : out std_logic;
 --     IO_SKIP : out std_logic;
 --
 --     BAC : in std_logic_vector (11 downto 0);
 --     BIOP1 : in std_logic;
 --     BIOP2 : in std_logic;
 --     BIOP4 : in std_logic;
 --     BMB : in std_logic_vector (11 downto 0);
 --     BUSINIT : in std_logic
 -- );
 -- end component;

begin
    xnanostep <= nanostep;
    xlastnano <= lastnanostep;
    xnanocycle <= nanocycle;

    -- bus values that are constants
    saxi_BRESP <= b"00";        -- A3.4.4/A10.3 transfer OK
    saxi_RRESP <= b"00";        -- A3.4.4/A10.3 transfer OK

    -- buffered outputs (outputs we read internally)
    saxi_ARREADY <= saxiARREADY;
    saxi_AWREADY <= saxiAWREADY;
    saxi_BVALID  <= saxiBVALID;
    saxi_RVALID  <= saxiRVALID;
    saxi_WREADY  <= saxiWREADY;

    ---------------------------------------------
    --  DMA test code -- not needed for pdp8l  --
    ---------------------------------------------

    maxi_ARBURST <= b"01";      -- A3.4.1/A10.3 burst type = INCR
    maxi_ARCACHE <= b"0000"; ----TODO---- b"0110";    -- A4.2 use read cache
    maxi_ARID <= b"0";          -- A10.3 transaction id 0
    maxi_ARLEN <= std_logic_vector (to_unsigned (BURSTLEN - 1, 8));  -- A3.4.1/A10.3 burst length
    maxi_ARLOCK <= b"00";       -- A7.4/A10.3 normal access
    maxi_ARPROT <= b"001";      -- A4.7 access permissions (privileged, secure, data)
    maxi_ARQOS <= b"0000";      -- A8.1.1/A10.3 no QoS requirement
    maxi_ARREGION <= b"0000";
    maxi_ARSIZE <= b"010";      -- A3.4.1 transfer size = 4 bytes each
    maxi_ARUSER <= b"0";

    maxi_AWBURST <= b"01";      -- A3.4.1/A10.3 burst type = INCR
    maxi_AWCACHE <= b"0000";
    maxi_AWID <= b"0";
    maxi_AWLEN <= std_logic_vector (to_unsigned (BURSTLEN - 1, 8));  -- A3.4.1/A10.3 burst length
    maxi_AWLOCK <= b"00";
    maxi_AWPROT <= b"001";      -- A4.7 access permissions (privileged, secure, data)
    maxi_AWQOS <= b"0000";
    maxi_AWREGION <= b"0000";
    maxi_AWSIZE <= b"010";      -- A3.4.1 transfer size = 4 bytes each
    maxi_AWUSER <= b"0";
    maxi_WSTRB <= b"1111";
    maxi_WUSER <= b"0";

    maxi_ARVALID <= maxiARVALID;
    maxi_RREADY  <= maxiRREADY;

    maxi_AWVALID <= maxiAWVALID;
    maxi_WVALID  <= maxiWVALID;
    maxi_WLAST   <= '1' when dmawritesel = BURSTLEN - 1 else '0';
    maxi_BREADY  <= maxiBREADY;

    maxi_WDATA <=
        temp0 when dmawritesel = 0 else
        temp1 when dmawritesel = 1 else
        temp2 when dmawritesel = 2 else
        temp3 when dmawritesel = 3 else
        temp4 when dmawritesel = 4 else
        temp5 when dmawritesel = 5 else
        temp6 when dmawritesel = 6 else
        temp7 when dmawritesel = 7 else
        temp8 when dmawritesel = 8 else
        temp9 when dmawritesel = 9 else
        x"DEADBEEF";

    -------------------------------------
    -- send register being read to ARM --
    -------------------------------------

    saxi_RDATA <=   VERSION when readaddr = b"0000000000" else              -- 00000000xx00
                    regctla when readaddr = b"0000000001" else
                    regctlb when readaddr = b"0000000010" else
                    regctlc when readaddr = b"0000000011" else
                    regctld when readaddr = b"0000000100" else
                    regctle when readaddr = b"0000000101" else
                    regctlf when readaddr = b"0000000110" else
                    regctlg when readaddr = b"0000000111" else
                    regctlh when readaddr = b"0000001000" else
                    regctli when readaddr = b"0000001001" else
                    regctlj when readaddr = b"0000001010" else
                    regctlk when readaddr = b"0000001011" else
                    ttardata   when readaddr(11 downto 4) = b"00001000" else  -- 00001000xx00
                    tt40ardata when readaddr(11 downto 4) = b"00001001" else  -- 00001001xx00
                    x"DEADBEEF";

    -- A3.3.1 Read transaction dependencies
    -- A3.3.1 Write transaction dependencies
    --        AXI4 write response dependency
    process (CLOCK, RESET_N)
    begin
        if RESET_N = '0' then
            saxiARREADY <= '1';                             -- we are ready to accept read address
            saxiRVALID <= '0';                              -- we are not sending out read data

            saxiAWREADY <= '1';                             -- we are ready to accept write address
            saxiWREADY <= '0';                              -- we are not ready to accept write data
            saxiBVALID <= '0';                              -- we are not acknowledging any write

            nanocycle <= '1';                               -- by default, software provides clock
            nanostep  <= '0';                               -- by default, software clock is low
            softreset <= '1';                               -- by default, software reset asserted
            testioins <= '0';                               -- by default, not testing io instruction

            -- reset dma read registers
            dmardaddr <= (others => '0');
            maxiARVALID <= '0';
            maxiRREADY <= '0';

            -- reset dma write registers
            dmawtaddr <= (others => '0');
            maxiAWVALID <= '0';
            maxiWVALID <= '0';
            maxiBREADY <= '0';
        elsif rising_edge (CLOCK) then

            ---------------------
            --  register read  --
            ---------------------

            -- check for PS sending us a read address
            if saxiARREADY = '1' and saxi_ARVALID = '1' then
                readaddr <= saxi_ARADDR(11 downto 2);       -- save address bits we care about
                saxiARREADY <= '0';                         -- we are no longer accepting a read address
                saxiRVALID <= '1';                          -- we are sending out the corresponding data

            -- check for PS acknowledging receipt of data
            elsif saxiRVALID = '1' and saxi_RREADY = '1' then
                saxiARREADY <= '1';                         -- we are ready to accept an address again
                saxiRVALID <= '0';                          -- we are no longer sending out data
            end if;

            ----------------------
            --  register write  --
            ----------------------

            -- check for PS sending us write data
            if saxiWREADY = '1' and saxi_WVALID = '1' then
                case writeaddr is                           -- write data to register
                    when b"00000001" =>
                        iBEMA         <= saxi_WDATA(00);
                        iCA_INCREMENT <= saxi_WDATA(01);
                        iDATA_IN      <= saxi_WDATA(02);
                        iMEMINCR      <= saxi_WDATA(03);
                        iMEM_P        <= saxi_WDATA(04);
                        iTHREECYCLE   <= saxi_WDATA(05);
                        arm_acclr     <= saxi_WDATA(06);
                        i_BRK_RQST    <= saxi_WDATA(07);
                        i_EA          <= saxi_WDATA(08);
                        i_EMA         <= saxi_WDATA(09);
                        i_INT_INHIBIT <= saxi_WDATA(10);
                        arm_intrq     <= saxi_WDATA(11);
                        arm_ioskip    <= saxi_WDATA(12);
                        i_MEMDONE     <= saxi_WDATA(13);
                        i_STROBE      <= saxi_WDATA(14);
                        testioins     <= saxi_WDATA(28);
                        softreset     <= saxi_WDATA(29);
                        nanostep      <= saxi_WDATA(30);
                        nanocycle     <= saxi_WDATA(31);

                    when b"00000010" =>
                        swCONT        <= saxi_WDATA(00);
                        swDEP         <= saxi_WDATA(01);
                        swDFLD        <= saxi_WDATA(02);
                        swEXAM        <= saxi_WDATA(03);
                        swIFLD        <= saxi_WDATA(04);
                        swLDAD        <= saxi_WDATA(05);
                        swMPRT        <= saxi_WDATA(06);
                        swSTEP        <= saxi_WDATA(07);
                        swSTOP        <= saxi_WDATA(08);
                        swSTART       <= saxi_WDATA(09);

                    when b"00000011" =>
                        armibus   <= saxi_WDATA(11 downto 00);
                        iMEM      <= saxi_WDATA(27 downto 16);

                    when b"00000100" =>
                        i_DMAADDR <= saxi_WDATA(11 downto 00);
                        i_DMADATA <= saxi_WDATA(27 downto 16);

                    when b"00000101" =>
                        swSR      <= saxi_WDATA(11 downto 00);

                    when others => null;
                end case;
                saxiAWREADY <= '1';                         -- we are ready to accept an address again
                saxiWREADY <= '0';                          -- we are no longer accepting write data
                saxiBVALID <= '1';                          -- we have accepted the data

            else
                -- check for PS sending us a write address
                if saxiAWREADY = '1' and saxi_AWVALID = '1' then
                    writeaddr <= saxi_AWADDR(11 downto 2);  -- save address bits we care about
                    saxiAWREADY <= '0';                     -- we are no longer accepting a write address
                    saxiWREADY <= '1';                      -- we are ready to accept write data
                end if;

                -- check for PS acknowledging write acceptance
                if saxiBVALID = '1' and saxi_BREADY = '1' then
                    saxiBVALID <= '0';
                end if;
            end if;

            -----------------------------------------
            --  dma read                           --
            --  read into temp0..9 from dmardaddr  --
            -----------------------------------------

            if maxiARVALID = '0' and maxiRREADY = '0' and dmardaddr(0) = '1' then
                maxi_ARADDR <= dmardaddr and x"FFFFF7FC";
                dmareadsel  <= 0;
                maxiARVALID <= '1';
                maxiRREADY  <= '1';
            end if;

            if maxiARVALID = '1' and maxi_ARREADY = '1' then
                maxiARVALID <= '0';
            end if;

            if maxiRREADY = '1' and maxi_RVALID = '1' then
                case dmareadsel is
                    when 0 => temp0 <= maxi_RDATA;
                    when 1 => temp1 <= maxi_RDATA;
                    when 2 => temp2 <= maxi_RDATA;
                    when 3 => temp3 <= maxi_RDATA;
                    when 4 => temp4 <= maxi_RDATA;
                    when 5 => temp5 <= maxi_RDATA;
                    when 6 => temp6 <= maxi_RDATA;
                    when 7 => temp7 <= maxi_RDATA;
                    when 8 => temp8 <= maxi_RDATA;
                    when 9 => temp9 <= maxi_RDATA;
                    when others => null;
                end case;
                dmardaddr(11 downto 2) <= std_logic_vector (unsigned (dmardaddr(11 downto 2)) + 1);
                if dmareadsel = BURSTLEN - 1 then
                    maxiRREADY   <= '0';
                    dmardaddr(0) <= '0';
                else
                    dmareadsel   <= dmareadsel + 1;
                end if;
            end if;

            ------------------------------------------
            --  dma write                           --
            --  write from temp0..9 into dmawtaddr  --
            ------------------------------------------

            -- if not doing anything and enabled to start, start writing
            if maxiAWVALID = '0' and maxiWVALID = '0' and maxiBREADY = '0' and dmawtaddr(0) = '1' then
                maxi_AWADDR <= dmawtaddr and x"FFFFF7FC";
                dmawritesel <= 0;
                maxiAWVALID <= '1';                     -- start sending address
                maxiWVALID  <= '1';                     -- start sending temp0
            end if;

            -- if mem controller accepted the address, stop sending it and accept completion status
            if maxiAWVALID = '1' and maxi_AWREADY = '1' then
                maxiAWVALID <= '0';                     -- stop sending address
                maxiBREADY  <= '1';                     -- able to accept completion status
            end if;

            -- if mem controller accepted data, stop sending last word or start sending next word
            if maxiWVALID = '1' and maxi_WREADY = '1' then
                dmawtaddr(11 downto 2) <= std_logic_vector (unsigned (dmawtaddr(11 downto 2)) + 1);
                if dmawritesel = BURSTLEN - 1 then
                    maxiWVALID  <= '0';                 -- stop sending last word
                else
                    dmawritesel <= dmawritesel + 1;     -- start sending next word
                end if;
            end if;

            -- if mem controller completed write, start writing next burst or stop writing
            if maxiBREADY = '1' and maxi_BVALID = '1' then
                maxiBREADY   <= '0';                    -- no longer accepting completion status
                dmawtaddr(1) <= '0';                    -- shift down start bit
                dmawtaddr(0) <= dmawtaddr(1);
            end if;
        end if;
    end process;

    -- pdp8l processor code in here

    regctla(00) <= iBEMA;
    regctla(01) <= iCA_INCREMENT;
    regctla(02) <= iDATA_IN;
    regctla(03) <= iMEMINCR;
    regctla(04) <= iMEM_P;
    regctla(05) <= iTHREECYCLE;
    regctla(06) <= i_AC_CLEAR;
    regctla(07) <= i_BRK_RQST;
    regctla(08) <= i_EA;
    regctla(09) <= i_EMA;
    regctla(10) <= i_INT_INHIBIT;
    regctla(11) <= i_INT_RQST;
    regctla(12) <= i_IO_SKIP;
    regctla(13) <= i_MEMDONE;
    regctla(14) <= i_STROBE;
    regctla(27 downto 15) <= (others => '0');
    regctla(28) <= testioins;
    regctla(29) <= softreset;
    regctla(30) <= nanostep;
    regctla(31) <= nanocycle;

    regctlb(00) <= swCONT;
    regctlb(01) <= swDEP;
    regctlb(02) <= swDFLD;
    regctlb(03) <= swEXAM;
    regctlb(04) <= swIFLD;
    regctlb(05) <= swLDAD;
    regctlb(06) <= swMPRT;
    regctlb(07) <= swSTEP;
    regctlb(08) <= swSTOP;
    regctlb(09) <= swSTART;
    regctlb(31 downto 10) <= (others => '0');

    regctlc(15 downto 00) <= x"0" & iINPUTBUS;
    regctlc(31 downto 16) <= x"0" & iMEM;
    regctld(15 downto 00) <= x"0" & i_DMAADDR;
    regctld(31 downto 16) <= x"0" & i_DMADATA;
    regctle(15 downto 00) <= x"0" & swSR;
    regctle(31 downto 16) <= x"0000";

    regctlf(00) <= oBIOP1;
    regctlf(01) <= oBIOP2;
    regctlf(02) <= oBIOP4;
    regctlf(03) <= oBTP2;
    regctlf(04) <= oBTP3;
    regctlf(05) <= oBTS_1;
    regctlf(06) <= oBTS_3;
    regctlf(07) <= oBUSINIT;
    regctlf(08) <= oBWC_OVERFLOW;
    regctlf(09) <= oB_BREAK;
    regctlf(10) <= oE_SET_F_SET;
    regctlf(11) <= oJMP_JMS;
    regctlf(12) <= oLINE_LOW;
    regctlf(13) <= oMEMSTART;
    regctlf(14) <= o_ADDR_ACCEPT;
    regctlf(15) <= o_BF_ENABLE;
    regctlf(16) <= o_BUSINIT;
    regctlf(17) <= o_B_RUN;
    regctlf(18) <= o_DF_ENABLE;
    regctlf(19) <= o_KEY_CLEAR;
    regctlf(20) <= o_KEY_DF;
    regctlf(21) <= o_KEY_IF;
    regctlf(22) <= o_KEY_LOAD;
    regctlf(23) <= o_LOAD_SF;
    regctlf(24) <= o_SP_CYC_NEXT;
    regctlf(31 downto 25) <= (others => '0');

    regctlg(00) <= lbBRK;
    regctlg(01) <= lbCA;
    regctlg(02) <= lbDEF;
    regctlg(03) <= lbEA;
    regctlg(04) <= lbEXE;
    regctlg(05) <= lbFET;
    regctlg(06) <= lbION;
    regctlg(07) <= lbLINK;
    regctlg(08) <= lbRUN;
    regctlg(09) <= lbWC;
    regctlg(15 downto 10) <= (others => '0');
    regctlg(27 downto 16) <= lbIR & b"000000000";
    regctlg(31 downto 28) <= x"0";

    regctlh(15 downto 00) <= x"0" & oBAC;
    regctlh(31 downto 16) <= x"0" & oBMB;
    regctli(15 downto 00) <= x"0" & oMA;
    regctli(31 downto 16) <= x"0" & lbAC;
    regctlj(15 downto 00) <= x"0" & lbMA;
    regctlj(31 downto 16) <= x"0" & lbMB;

    inuseclock <= nanostep when nanocycle = '1' else CLOCK;
    inusereset <= softreset or not RESET_N;

    LEDoutR <= not inusereset;
    LEDoutG <= not inuseclock;
    LEDoutB <= not nanocycle;

    iINPUTBUS  <= ttibus or tt40ibus when testioins = '0' else armibus;
    i_AC_CLEAR <= not (ttacclr  or tt40acclr)  when testioins = '0' else arm_acclr;
    i_INT_RQST <= not (ttintrq  or tt40intrq)  when testioins = '0' else arm_intrq;
    i_IO_SKIP  <= not (ttioskip or tt40ioskip) when testioins = '0' else arm_ioskip;

    pdp8linst: pdp8l port map (
        CLOCK         => CLOCK,
        RESET         => inusereset,
        iBEMA         => iBEMA,
        iCA_INCREMENT => iCA_INCREMENT,
        iDATA_IN      => iDATA_IN,
        iINPUTBUS     => iINPUTBUS,
        iMEMINCR      => iMEMINCR,
        iMEM          => iMEM,
        iMEM_P        => iMEM_P,
        iTHREECYCLE   => iTHREECYCLE,
        i_AC_CLEAR    => i_AC_CLEAR,
        i_BRK_RQST    => i_BRK_RQST,
        i_DMAADDR     => i_DMAADDR,
        i_DMADATA     => i_DMADATA,
        i_EA          => i_EA,
        i_EMA         => i_EMA,
        i_INT_INHIBIT => i_INT_INHIBIT,
        i_INT_RQST    => i_INT_RQST,
        i_IO_SKIP     => i_IO_SKIP,
        i_MEMDONE     => i_MEMDONE,
        i_STROBE      => i_STROBE,
        oBAC          => oBAC,
        oBIOP1        => oBIOP1,
        oBIOP2        => oBIOP2,
        oBIOP4        => oBIOP4,
        oBMB          => oBMB,
        oBTP2         => oBTP2,
        oBTP3         => oBTP3,
        oBTS_1        => oBTS_1,
        oBTS_3        => oBTS_3,
        oBUSINIT      => oBUSINIT,
        oBWC_OVERFLOW => oBWC_OVERFLOW,
        oB_BREAK      => oB_BREAK,
        oE_SET_F_SET  => oE_SET_F_SET,
        oJMP_JMS      => oJMP_JMS,
        oLINE_LOW     => oLINE_LOW,
        oMA           => oMA,
        oMEMSTART     => oMEMSTART,
        o_ADDR_ACCEPT => o_ADDR_ACCEPT,
        o_BF_ENABLE   => o_BF_ENABLE,
        o_BUSINIT     => o_BUSINIT,
        o_B_RUN       => o_B_RUN,
        o_DF_ENABLE   => o_DF_ENABLE,
        o_KEY_CLEAR   => o_KEY_CLEAR,
        o_KEY_DF      => o_KEY_DF,
        o_KEY_IF      => o_KEY_IF,
        o_KEY_LOAD    => o_KEY_LOAD,
        o_LOAD_SF     => o_LOAD_SF,
        o_SP_CYC_NEXT => o_SP_CYC_NEXT,
        lbAC          => lbAC,
        lbBRK         => lbBRK,
        lbCA          => lbCA,
        lbDEF         => lbDEF,
        lbEA          => lbEA,
        lbEXE         => lbEXE,
        lbFET         => lbFET,
        lbION         => lbION,
        lbIR          => lbIR,
        lbLINK        => lbLINK,
        lbMA          => lbMA,
        lbMB          => lbMB,
        lbRUN         => lbRUN,
        lbWC          => lbWC,
        swCONT        => swCONT,
        swDEP         => swDEP,
        swDFLD        => swDFLD,
        swEXAM        => swEXAM,
        swIFLD        => swIFLD,
        swLDAD        => swLDAD,
        swMPRT        => swMPRT,
        swSTEP        => swSTEP,
        swSTOP        => swSTOP,
        swSR          => swSR,
        swSTART       => swSTART

        ,majstate  => regctlk(02 downto 00)
        ,timedelay => regctlk(08 downto 03)
        ,timestate => regctlk(13 downto 09)
        ,cyclectr  => regctlk(23 downto 14)

        ,nanocycle => nanocycle
        ,nanostep  => nanostep
        ,lastnanostep => lastnanostep
    );

    regctlk(31 downto 29) <= (others => '0');

    -- teletype interfaces

    ttawpuls   <= '1' when (saxiWREADY = '1' and saxi_WVALID = '1' and writeaddr(11 downto 4) = x"08") else '0';
    tt40awpuls <= '1' when (saxiWREADY = '1' and saxi_WVALID = '1' and writeaddr(11 downto 4) = x"09") else '0';

    ttinst: pdp8ltty port map (
        CLOCK => inuseclock,
        RESET => inusereset,
        armwpulse => ttawpuls,
        armraddr => readaddr(3 downto 2),
        armwaddr => writeaddr(3 downto 2),
        armwdata => saxi_WDATA,
        armrdata => ttardata,

        INPUTBUS => ttibus,
        AC_CLEAR => ttacclr,
        INT_RQST => ttintrq,
        IO_SKIP  => ttioskip,
        BAC => oBAC,
        BIOP1 => oBIOP1,
        BIOP2 => oBIOP2,
        BIOP4 => oBIOP4,
        BMB => oBMB,
        BUSINIT => oBUSINIT
    );

    tt40inst: pdp8ltty
        generic map (KBDEV => std_logic_vector (to_unsigned (8#40#, 6)))
        port map (
            CLOCK => inuseclock,
            RESET => inusereset,
            armwpulse => tt40awpuls,
            armraddr => readaddr(3 downto 2),
            armwaddr => writeaddr(3 downto 2),
            armwdata => saxi_WDATA,
            armrdata => tt40ardata,

            INPUTBUS => tt40ibus,
            AC_CLEAR => tt40acclr,
            INT_RQST => tt40intrq,
            IO_SKIP  => tt40ioskip,
            BAC => oBAC,
            BIOP1 => oBIOP1,
            BIOP2 => oBIOP2,
            BIOP4 => oBIOP4,
            BMB => oBMB,
            BUSINIT => oBUSINIT
        );

end rtl;
