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

            bDMABUSA       : inout std_logic;
            bDMABUSB       : inout std_logic;
            bDMABUSC       : inout std_logic;
            bDMABUSD       : inout std_logic;
            bDMABUSE       : inout std_logic;
            bDMABUSF       : inout std_logic;
            bDMABUSH       : inout std_logic;
            bDMABUSJ       : inout std_logic;
            bDMABUSK       : inout std_logic;
            bDMABUSL       : inout std_logic;
            bDMABUSM       : inout std_logic;
            bDMABUSN       : inout std_logic;
            bMEMBUSA       : inout std_logic;
            bMEMBUSB       : inout std_logic;
            bMEMBUSC       : inout std_logic;
            bMEMBUSD       : inout std_logic;
            bMEMBUSE       : inout std_logic;
            bMEMBUSF       : inout std_logic;
            bMEMBUSH       : inout std_logic;
            bMEMBUSJ       : inout std_logic;
            bMEMBUSK       : inout std_logic;
            bMEMBUSL       : inout std_logic;
            bMEMBUSM       : inout std_logic;
            bMEMBUSN       : inout std_logic;
            bPIOBUSA       : inout std_logic;
            bPIOBUSB       : inout std_logic;
            bPIOBUSC       : inout std_logic;
            bPIOBUSD       : inout std_logic;
            bPIOBUSE       : inout std_logic;
            bPIOBUSF       : inout std_logic;
            bPIOBUSH       : inout std_logic;
            bPIOBUSJ       : inout std_logic;
            bPIOBUSK       : inout std_logic;
            bPIOBUSL       : inout std_logic;
            bPIOBUSM       : inout std_logic;
            bPIOBUSN       : inout std_logic;
            i_AC_CLEAR     : out std_logic;
            i_BRK_RQST     : out std_logic;
            i_EA           : out std_logic;
            i_EMA          : out std_logic;
            i_EXTDMAADD_12 : out std_logic;
            i_INT_INHIBIT  : out std_logic;
            i_INT_RQST     : out std_logic;
            i_IO_SKIP      : out std_logic;
            i_MEMDONE      : out std_logic;
            i_STROBE       : out std_logic;
            iB36V1         : out std_logic;
            iBEMA          : out std_logic;
            iCA_INCREMENT  : out std_logic;
            iD36B2         : out std_logic;
            iDATA_IN       : out std_logic;
            iMEM_07        : out std_logic;
            iMEMINCR       : out std_logic;
            iTHREECYCLE    : out std_logic;
            o_ADDR_ACCEPT  : in std_logic;
            o_B_RUN        : in std_logic;
            o_BF_ENABLE    : in std_logic;
            o_BUSINIT      : in std_logic;
            o_DF_ENABLE    : in std_logic;
            o_KEY_CLEAR    : in std_logic;
            o_KEY_DF       : in std_logic;
            o_KEY_IF       : in std_logic;
            o_KEY_LOAD     : in std_logic;
            o_LOAD_SF      : in std_logic;
            o_SP_CYC_NEXT  : in std_logic;
            oB_BREAK       : in std_logic;
            oBIOP1         : in std_logic;
            oBIOP2         : in std_logic;
            oBIOP4         : in std_logic;
            oBTP2          : in std_logic;
            oBTP3          : in std_logic;
            oBTS_1         : in std_logic;
            oBTS_3         : in std_logic;
            oBWC_OVERFLOW  : in std_logic;
            oC36B2         : in std_logic;
            oD35B2         : in std_logic;
            oE_SET_F_SET   : in std_logic;
            oJMP_JMS       : in std_logic;
            oLINE_LOW      : in std_logic;
            oMEMSTART      : in std_logic;
            r_BAC          : out std_logic;
            r_BMB          : out std_logic;
            r_MA           : out std_logic;
            x_DMAADDR      : out std_logic;
            x_DMADATA      : out std_logic;
            x_INPUTBUS     : out std_logic;
            x_MEM          : out std_logic;

            xnanostep : out STD_LOGIC;
            xlastnano : out STD_LOGIC;
            xnanocycle : out STD_LOGIC;

            xbiop1, xbiop2, xbiop4 : out std_logic;
            xfiop1, xfiop2, xfiop4 : out std_logic;
            xiopsetct : out std_logic_vector (3 downto 0);
            xiopclrct : out std_logic_vector (2 downto 0);
            xiopstart, xiopstop : out std_logic;

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
            saxi_WVALID : in std_logic);
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

    -- [31:16] = '8L'; [15:12] = (log2 len)-1; [11:00] = version
    constant VERSION : std_logic_vector (31 downto 0) := x"384C402C";

    signal saxiARREADY, saxiAWREADY, saxiBVALID, saxiRVALID, saxiWREADY : std_logic;

    signal readaddr, writeaddr : std_logic_vector (11 downto 2);

    -- pdp8/l module signals

    --      i... : signals going to hardware PDP-8/L
    --      o... : signals coming from hardware PDP-8/L

    --  sim_i... : signals going to simulated PDP-8/L (pdp8lsim.v)
    --  sim_o... : signals coming from simulated PDP-8/L (pdp8lsim.v)

    --  dev_i... : signals from io devices going to both i... and sim_i...
    --  dev_o... : selected from o... or sim_o... going to io devices

    signal regctla, regctlb, regctlc, regctld, regctle, regctlf, regctlg, regctlh, regctli, regctlj, regctlk : std_logic_vector (31 downto 0);

    signal sim_iBEMA         , dev_iBEMA         : std_logic;
    signal sim_iCA_INCREMENT , dev_iCA_INCREMENT : std_logic;
    signal sim_iDATA_IN      , dev_iDATA_IN      : std_logic;
    signal sim_iINPUTBUS     , dev_iINPUTBUS     : std_logic_vector (11 downto 0);
    signal sim_iMEMINCR      , dev_iMEMINCR      : std_logic;
    signal sim_iMEM          , dev_iMEM          : std_logic_vector (11 downto 0);
    signal sim_iMEM_P        , dev_iMEM_P        : std_logic;
    signal sim_iTHREECYCLE   , dev_iTHREECYCLE   : std_logic;
    signal sim_i_AC_CLEAR    , dev_i_AC_CLEAR    : std_logic;
    signal sim_i_BRK_RQST    , dev_i_BRK_RQST    : std_logic;
    signal sim_i_DMAADDR     , dev_i_DMAADDR     : std_logic_vector (11 downto 0);
    signal sim_i_DMADATA     , dev_i_DMADATA     : std_logic_vector (11 downto 0);
    signal sim_i_EA          , dev_i_EA          : std_logic;
    signal sim_i_EMA         , dev_i_EMA         : std_logic;
    signal sim_i_INT_INHIBIT , dev_i_INT_INHIBIT : std_logic;
    signal sim_i_INT_RQST    , dev_i_INT_RQST    : std_logic;
    signal sim_i_IO_SKIP     , dev_i_IO_SKIP     : std_logic;
    signal sim_i_MEMDONE     , dev_i_MEMDONE     : std_logic;
    signal sim_i_STROBE      , dev_i_STROBE      : std_logic;
    signal sim_oBAC          , dev_oBAC          : std_logic_vector (11 downto 0);
    signal sim_oBIOP1        , dev_oBIOP1        : std_logic;
    signal sim_oBIOP2        , dev_oBIOP2        : std_logic;
    signal sim_oBIOP4        , dev_oBIOP4        : std_logic;
    signal sim_oBMB          , dev_oBMB          : std_logic_vector (11 downto 0);
    signal sim_oBTP2         , dev_oBTP2         : std_logic;
    signal sim_oBTP3         , dev_oBTP3         : std_logic;
    signal sim_oBTS_1        , dev_oBTS_1        : std_logic;
    signal sim_oBTS_3        , dev_oBTS_3        : std_logic;
    signal sim_oBWC_OVERFLOW , dev_oBWC_OVERFLOW : std_logic;
    signal sim_oB_BREAK      , dev_oB_BREAK      : std_logic;
    signal sim_oE_SET_F_SET  , dev_oE_SET_F_SET  : std_logic;
    signal sim_oJMP_JMS      , dev_oJMP_JMS      : std_logic;
    signal sim_oLINE_LOW     , dev_oLINE_LOW     : std_logic;
    signal sim_oMA           , dev_oMA           : std_logic_vector (11 downto 0);
    signal sim_oMEMSTART     , dev_oMEMSTART     : std_logic;
    signal sim_o_ADDR_ACCEPT , dev_o_ADDR_ACCEPT : std_logic;
    signal sim_o_BF_ENABLE   , dev_o_BF_ENABLE   : std_logic;
    signal sim_o_BUSINIT     , dev_o_BUSINIT     : std_logic;
    signal sim_o_B_RUN       , dev_o_B_RUN       : std_logic;
    signal sim_o_DF_ENABLE   , dev_o_DF_ENABLE   : std_logic;
    signal sim_o_KEY_CLEAR   , dev_o_KEY_CLEAR   : std_logic;
    signal sim_o_KEY_DF      , dev_o_KEY_DF      : std_logic;
    signal sim_o_KEY_IF      , dev_o_KEY_IF      : std_logic;
    signal sim_o_KEY_LOAD    , dev_o_KEY_LOAD    : std_logic;
    signal sim_o_LOAD_SF     , dev_o_LOAD_SF     : std_logic;
    signal sim_o_SP_CYC_NEXT , dev_o_SP_CYC_NEXT : std_logic;

    signal sim_lbAC    : std_logic_vector (11 downto 0);
    signal sim_lbBRK   : std_logic;
    signal sim_lbCA    : std_logic;
    signal sim_lbDEF   : std_logic;
    signal sim_lbEA    : std_logic;
    signal sim_lbEXE   : std_logic;
    signal sim_lbFET   : std_logic;
    signal sim_lbION   : std_logic;
    signal sim_lbIR    : std_logic_vector (2 downto 0);
    signal sim_lbLINK  : std_logic;
    signal sim_lbMA    : std_logic_vector (11 downto 0);
    signal sim_lbMB    : std_logic_vector (11 downto 0);
    signal sim_lbRUN   : std_logic;
    signal sim_lbWC    : std_logic;
    signal sim_swCONT  : std_logic;
    signal sim_swDEP   : std_logic;
    signal sim_swDFLD  : std_logic;
    signal sim_swEXAM  : std_logic;
    signal sim_swIFLD  : std_logic;
    signal sim_swLDAD  : std_logic;
    signal sim_swMPRT  : std_logic;
    signal sim_swSTEP  : std_logic;
    signal sim_swSTOP  : std_logic;
    signal sim_swSR    : std_logic_vector (11 downto 0);
    signal sim_swSTART : std_logic;

    signal i_DMAADDR, i_DMADATA : std_logic_vector (11 downto 0);   -- dma address, data going out to real PDP-8/L via our DMABUS
                                                                    -- ...used to access real PDP-8/L 4K core memory
    signal iINPUTBUS : std_logic_vector (11 downto 0);              -- io data going out to real PDP-8/L INPUTBUS via our PIOBUS
    signal iMEM   : std_logic_vector (11 downto 0);                 -- extended memory data going to real PDP-8/L MEM via our MEMBUS
    signal iMEM_P : std_logic;                                      -- extended memory parity going to real PDP-8/L MEM_P via our MEMBUSA
    signal oBAC   : std_logic_vector (11 downto 0);                 -- real PDP-8/L AC contents, valid only during second half of io pulse
    signal oBMB   : std_logic_vector (11 downto 0);                 -- real PDP-8/L MB contents, theoretically always valid (latched during io pulse)
    signal oMA    : std_logic_vector (11 downto 0);                 -- real PDP-8/L MA contents, used by PDP-8/L to access extended memory
    signal buf_x_INPUTBUS : std_logic;                              -- gates iINPUTBUS out to real PDP-8/L INPUTBUS via our PIOBUS (active low)

    signal simit : boolean;
    signal lastnanostep, nanocycle, nanostep, softreset : std_logic;
    signal inuseclock, inusereset : std_logic;
    signal ioreset, armwrite : boolean;
    signal iopstart, iopstop : boolean;
    signal firstiop1, firstiop2, firstiop4 : boolean;
    signal acclr, intrq, ioskp : std_logic;
    signal iopsetcount : natural range 0 to 15; -- count fpga cycles where an IOP is on
    signal iopclrcount : natural range 0 to  7; -- count fpga cycles where no IOP is on

    -- arm interface signals
    signal arm_acclr, arm_intrq, arm_ioskp : std_logic;
    signal armibus : std_logic_vector (11 downto 0);

    -- tty interface signals
    signal ttardata : std_logic_vector (31 downto 0);
    signal ttibus : std_logic_vector (11 downto 0);
    signal ttawrite, ttacclr, ttintrq, ttioskp : boolean;
    signal tt40ardata : std_logic_vector (31 downto 0);
    signal tt40ibus : std_logic_vector (11 downto 0);
    signal tt40awrite, tt40acclr, tt40intrq, tt40ioskp : boolean;

    -- disk interface signals
    signal rkardata : std_logic_vector (31 downto 0);
    signal rkibus : std_logic_vector (11 downto 0);
    signal rkawrite, rkacclr, rkintrq, rkioskp : boolean;

component pdp8lsim port (
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
 --     armwrite : in std_logic;
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

    -- debug signals
    xnanostep <= nanostep;
    xlastnano <= lastnanostep;
    xnanocycle <= nanocycle;

    xbiop1 <= sim_oBIOP1;
    xbiop2 <= sim_oBIOP2;
    xbiop4 <= sim_oBIOP4;
    xfiop1 <= '1' when firstiop1 else '0';
    xfiop2 <= '1' when firstiop2 else '0';
    xfiop4 <= '1' when firstiop4 else '0';
    xiopsetct <= std_logic_vector (to_unsigned (iopsetcount, 4));
    xiopclrct <= std_logic_vector (to_unsigned (iopclrcount, 3));
    xiopstart <= '1' when iopstart else '0';
    xiopstop  <= '1' when iopstop  else '0';

    -- bus values that are constants
    saxi_BRESP <= b"00";        -- A3.4.4/A10.3 transfer OK
    saxi_RRESP <= b"00";        -- A3.4.4/A10.3 transfer OK

    -- buffered outputs (outputs we read internally)
    saxi_ARREADY <= saxiARREADY;
    saxi_AWREADY <= saxiAWREADY;
    saxi_BVALID  <= saxiBVALID;
    saxi_RVALID  <= saxiRVALID;
    saxi_WREADY  <= saxiWREADY;

    -------------------------------------
    -- send register being read to ARM --
    -------------------------------------

    saxi_RDATA <=   VERSION when readaddr = b"0000000000" else              -- 00000xxxxx00
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
                   rkardata when readaddr(11 downto 5) = b"0000100"  else  -- 0000100xxx00
                   ttardata when readaddr(11 downto 4) = b"00001010" else  -- 00001010xx00
                 tt40ardata when readaddr(11 downto 4) = b"00001011" else  -- 00001011xx00
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
                        sim_iBEMA         <= saxi_WDATA(00);
                        sim_iCA_INCREMENT <= saxi_WDATA(01);
                        sim_iDATA_IN      <= saxi_WDATA(02);
                        sim_iMEMINCR      <= saxi_WDATA(03);
                        sim_iMEM_P        <= saxi_WDATA(04);
                        sim_iTHREECYCLE   <= saxi_WDATA(05);
                        arm_acclr         <= saxi_WDATA(06);
                        sim_i_BRK_RQST    <= saxi_WDATA(07);
                        sim_i_EA          <= saxi_WDATA(08);
                        sim_i_EMA         <= saxi_WDATA(09);
                        sim_i_INT_INHIBIT <= saxi_WDATA(10);
                        arm_intrq         <= saxi_WDATA(11);
                        arm_ioskp         <= saxi_WDATA(12);
                        sim_i_MEMDONE     <= saxi_WDATA(13);
                        sim_i_STROBE      <= saxi_WDATA(14);
                        simit             <= saxi_WDATA(27) = '1';
                        softreset         <= saxi_WDATA(29);
                        nanostep          <= saxi_WDATA(30);
                        nanocycle         <= saxi_WDATA(31);

                    when b"00000010" =>
                        sim_swCONT        <= saxi_WDATA(00);
                        sim_swDEP         <= saxi_WDATA(01);
                        sim_swDFLD        <= saxi_WDATA(02);
                        sim_swEXAM        <= saxi_WDATA(03);
                        sim_swIFLD        <= saxi_WDATA(04);
                        sim_swLDAD        <= saxi_WDATA(05);
                        sim_swMPRT        <= saxi_WDATA(06);
                        sim_swSTEP        <= saxi_WDATA(07);
                        sim_swSTOP        <= saxi_WDATA(08);
                        sim_swSTART       <= saxi_WDATA(09);

                    when b"00000011" =>
                        armibus       <= saxi_WDATA(11 downto 00);
                        sim_iMEM      <= saxi_WDATA(27 downto 16);

                    when b"00000100" =>
                        sim_i_DMAADDR <= saxi_WDATA(11 downto 00);
                        sim_i_DMADATA <= saxi_WDATA(27 downto 16);

                    when b"00000101" =>
                        sim_swSR      <= saxi_WDATA(11 downto 00);

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
        end if;
    end process;

    -----------------------------------------------------
    --  select between simulated and hardware PDP-8/L  --
    -----------------------------------------------------

    -- when simulating, send signals from devices on to the simulated PDP-8/L
    -- when not simming, send signals from devices on to the hardware PDP-8/L

    --TODO--sim_iBEMA         <= dev_iBEMA          when simit else '0'; -- enable when extended mem controller implemented
    --TODO--sim_iCA_INCREMENT <= dev_iCA_INCREMENT  when simit else '0'; -- enable when arm-to-phys-low-4K controller implemented
    --TODO--sim_iDATA_IN      <= dev_iDATA_IN       when simit else '0'; -- enable when arm-to-phys-low-4K controller implemented
    --TODO--sim_iMEMINCR      <= dev_iMEMINCR       when simit else '0'; -- enable when arm-to-phys-low-4K controller implemented
    --TODO--sim_iMEM          <= dev_iMEM           when simit else x"000"; -- enable when extended block memory implemented
    --TODO--sim_iMEM_P        <= dev_iMEM_P         when simit else '0'; -- enable when extended block memory implemented
    --TODO--sim_iTHREECYCLE   <= dev_iTHREECYCLE    when simit else '0'; -- enable when arm-to-phys-low-4K controller implemented
    --TODO--sim_i_BRK_RQST    <= dev_i_BRK_RQST     when simit else '1'; -- enable when arm-to-phys-low-4K controller implemented
    --TODO--sim_i_DMAADDR     <= dev_i_DMAADDR      when simit else x"FFF"; -- enable when arm-to-phys-low-4K controller implemented
    --TODO--sim_i_DMADATA     <= dev_i_DMADATA      when simit else x"FFF"; -- enable when arm-to-phys-low-4K controller implemented
    --TODO--sim_i_EA          <= dev_i_EA           when simit else '1'; -- enable when extended mem controller implemented
    --TODO--sim_i_EMA         <= dev_i_EMA          when simit else '1'; -- enable when extended mem controller implemented
    --TODO--sim_i_INT_INHIBIT <= dev_i_INT_INHIBIT  when simit else '1'; -- enable when extended mem controller implemented
    --TODO--sim_i_MEMDONE     <= dev_i_MEMDONE      when simit else '1'; -- enable when extended block memory implemented
    --TODO--sim_i_STROBE      <= dev_i_STROBE       when simit else '1'; -- enable when extended block memory implemented

    sim_iINPUTBUS  <= armibus   or  dev_iINPUTBUS  when simit else x"000";
    sim_i_AC_CLEAR <= arm_acclr and dev_i_AC_CLEAR when simit else '1';
    sim_i_INT_RQST <= arm_intrq and dev_i_INT_RQST when simit else '1';
    sim_i_IO_SKIP  <= arm_ioskp and dev_i_IO_SKIP  when simit else '1';

    iBEMA          <= '0' when simit else dev_iBEMA         ;
    iCA_INCREMENT  <= '0' when simit else dev_iCA_INCREMENT ;
    iDATA_IN       <= '0' when simit else dev_iDATA_IN      ;
    iINPUTBUS      <= x"000" when simit else dev_iINPUTBUS  ;
    iMEMINCR       <= '0' when simit else dev_iMEMINCR      ;
    iMEM           <= x"000" when simit else dev_iMEM       ;
    iMEM_P         <= '0' when simit else dev_iMEM_P        ;
    iTHREECYCLE    <= '0' when simit else dev_iTHREECYCLE   ;
    i_AC_CLEAR     <= '1' when simit else dev_i_AC_CLEAR    ;
    i_BRK_RQST     <= '1' when simit else dev_i_BRK_RQST    ;
    i_DMAADDR      <= x"FFF" when simit else dev_i_DMAADDR  ;
    i_DMADATA      <= x"FFF" when simit else dev_i_DMADATA  ;
    i_EA           <= '1' when simit else dev_i_EA          ;
    i_EMA          <= '1' when simit else dev_i_EMA         ;
    i_INT_INHIBIT  <= '1' when simit else dev_i_INT_INHIBIT ;
    i_INT_RQST     <= '1' when simit else dev_i_INT_RQST    ;
    i_IO_SKIP      <= '1' when simit else dev_i_IO_SKIP     ;
    i_MEMDONE      <= '1' when simit else dev_i_MEMDONE     ;
    i_STROBE       <= '1' when simit else dev_i_STROBE      ;

    -- when simulating, send signals from the simulated PDP-8/L on to the devices
    -- when not simming, send signals from the hardware PDP-8/L on to the devices

    dev_oBAC           <= sim_oBAC           when simit else oBAC          ;
    dev_oBIOP1         <= sim_oBIOP1         when simit else oBIOP1        ;
    dev_oBIOP2         <= sim_oBIOP2         when simit else oBIOP2        ;
    dev_oBIOP4         <= sim_oBIOP4         when simit else oBIOP4        ;
    dev_oBMB           <= sim_oBMB           when simit else oBMB          ;
    dev_oBTP2          <= sim_oBTP2          when simit else oBTP2         ;
    dev_oBTP3          <= sim_oBTP3          when simit else oBTP3         ;
    dev_oBTS_1         <= sim_oBTS_1         when simit else oBTS_1        ;
    dev_oBTS_3         <= sim_oBTS_3         when simit else oBTS_3        ;
    dev_oBWC_OVERFLOW  <= sim_oBWC_OVERFLOW  when simit else oBWC_OVERFLOW ;
    dev_oB_BREAK       <= sim_oB_BREAK       when simit else oB_BREAK      ;
    dev_oE_SET_F_SET   <= sim_oE_SET_F_SET   when simit else oE_SET_F_SET  ;
    dev_oJMP_JMS       <= sim_oJMP_JMS       when simit else oJMP_JMS      ;
    dev_oLINE_LOW      <= sim_oLINE_LOW      when simit else oLINE_LOW     ;
    dev_oMA            <= sim_oMA            when simit else oMA           ;
    dev_oMEMSTART      <= sim_oMEMSTART      when simit else oMEMSTART     ;
    dev_o_ADDR_ACCEPT  <= sim_o_ADDR_ACCEPT  when simit else o_ADDR_ACCEPT ;
    dev_o_BF_ENABLE    <= sim_o_BF_ENABLE    when simit else o_BF_ENABLE   ;
    dev_o_BUSINIT      <= sim_o_BUSINIT      when simit else o_BUSINIT     ;
    dev_o_B_RUN        <= sim_o_B_RUN        when simit else o_B_RUN       ;
    dev_o_DF_ENABLE    <= sim_o_DF_ENABLE    when simit else o_DF_ENABLE   ;
    dev_o_KEY_CLEAR    <= sim_o_KEY_CLEAR    when simit else o_KEY_CLEAR   ;
    dev_o_KEY_DF       <= sim_o_KEY_DF       when simit else o_KEY_DF      ;
    dev_o_KEY_IF       <= sim_o_KEY_IF       when simit else o_KEY_IF      ;
    dev_o_KEY_LOAD     <= sim_o_KEY_LOAD     when simit else o_KEY_LOAD    ;
    dev_o_LOAD_SF      <= sim_o_LOAD_SF      when simit else o_LOAD_SF     ;
    dev_o_SP_CYC_NEXT  <= sim_o_SP_CYC_NEXT  when simit else o_SP_CYC_NEXT ;

    -----------------------------------
    --  simulated PDP-8/L processor  --
    -----------------------------------

    regctla(00) <= sim_iBEMA;
    regctla(01) <= sim_iCA_INCREMENT;
    regctla(02) <= sim_iDATA_IN;
    regctla(03) <= sim_iMEMINCR;
    regctla(04) <= sim_iMEM_P;
    regctla(05) <= sim_iTHREECYCLE;
    regctla(06) <= sim_i_AC_CLEAR;
    regctla(07) <= sim_i_BRK_RQST;
    regctla(08) <= sim_i_EA;
    regctla(09) <= sim_i_EMA;
    regctla(10) <= sim_i_INT_INHIBIT;
    regctla(11) <= sim_i_INT_RQST;
    regctla(12) <= sim_i_IO_SKIP;
    regctla(13) <= sim_i_MEMDONE;
    regctla(14) <= sim_i_STROBE;
    regctla(26 downto 15) <= (others => '0');
    regctla(27) <= '1' when simit else '0';
    regctla(28) <= '0';
    regctla(29) <= softreset;
    regctla(30) <= nanostep;
    regctla(31) <= nanocycle;

    regctlb(00) <= sim_swCONT;
    regctlb(01) <= sim_swDEP;
    regctlb(02) <= sim_swDFLD;
    regctlb(03) <= sim_swEXAM;
    regctlb(04) <= sim_swIFLD;
    regctlb(05) <= sim_swLDAD;
    regctlb(06) <= sim_swMPRT;
    regctlb(07) <= sim_swSTEP;
    regctlb(08) <= sim_swSTOP;
    regctlb(09) <= sim_swSTART;
    regctlb(31 downto 10) <= (others => '0');

    regctlc(15 downto 00) <= x"0" & sim_iINPUTBUS;
    regctlc(31 downto 16) <= x"0" & sim_iMEM;
    regctld(15 downto 00) <= x"0" & sim_i_DMAADDR;
    regctld(31 downto 16) <= x"0" & sim_i_DMADATA;
    regctle(15 downto 00) <= x"0" & sim_swSR;
    regctle(31 downto 16) <= x"0000";

    regctlf(00) <= sim_oBIOP1;
    regctlf(01) <= sim_oBIOP2;
    regctlf(02) <= sim_oBIOP4;
    regctlf(03) <= sim_oBTP2;
    regctlf(04) <= sim_oBTP3;
    regctlf(05) <= sim_oBTS_1;
    regctlf(06) <= sim_oBTS_3;
    regctlf(07) <= '0';
    regctlf(08) <= sim_oBWC_OVERFLOW;
    regctlf(09) <= sim_oB_BREAK;
    regctlf(10) <= sim_oE_SET_F_SET;
    regctlf(11) <= sim_oJMP_JMS;
    regctlf(12) <= sim_oLINE_LOW;
    regctlf(13) <= sim_oMEMSTART;
    regctlf(14) <= sim_o_ADDR_ACCEPT;
    regctlf(15) <= sim_o_BF_ENABLE;
    regctlf(16) <= sim_o_BUSINIT;
    regctlf(17) <= sim_o_B_RUN;
    regctlf(18) <= sim_o_DF_ENABLE;
    regctlf(19) <= sim_o_KEY_CLEAR;
    regctlf(20) <= sim_o_KEY_DF;
    regctlf(21) <= sim_o_KEY_IF;
    regctlf(22) <= sim_o_KEY_LOAD;
    regctlf(23) <= sim_o_LOAD_SF;
    regctlf(24) <= sim_o_SP_CYC_NEXT;
    regctlf(31 downto 25) <= (others => '0');

    regctlg(00) <= sim_lbBRK;
    regctlg(01) <= sim_lbCA;
    regctlg(02) <= sim_lbDEF;
    regctlg(03) <= sim_lbEA;
    regctlg(04) <= sim_lbEXE;
    regctlg(05) <= sim_lbFET;
    regctlg(06) <= sim_lbION;
    regctlg(07) <= sim_lbLINK;
    regctlg(08) <= sim_lbRUN;
    regctlg(09) <= sim_lbWC;
    regctlg(15 downto 10) <= (others => '0');
    regctlg(27 downto 16) <= sim_lbIR & b"000000000";
    regctlg(31 downto 28) <= x"0";

    regctlh(15 downto 00) <= x"0" & sim_oBAC;
    regctlh(31 downto 16) <= x"0" & sim_oBMB;
    regctli(15 downto 00) <= x"0" & sim_oMA;
    regctli(31 downto 16) <= x"0" & sim_lbAC;
    regctlj(15 downto 00) <= x"0" & sim_lbMA;
    regctlj(31 downto 16) <= x"0" & sim_lbMB;

    inuseclock <= nanostep when nanocycle = '1' else CLOCK;
    inusereset <= softreset or not RESET_N;

    LEDoutR <= not inusereset;
    LEDoutG <= not inuseclock;
    LEDoutB <= not nanocycle;

    siminst: pdp8lsim port map (
        CLOCK         => CLOCK,
        RESET         => inusereset,
        iBEMA         => sim_iBEMA,
        iCA_INCREMENT => sim_iCA_INCREMENT,
        iDATA_IN      => sim_iDATA_IN,
        iINPUTBUS     => sim_iINPUTBUS,
        iMEMINCR      => sim_iMEMINCR,
        iMEM          => sim_iMEM,
        iMEM_P        => sim_iMEM_P,
        iTHREECYCLE   => sim_iTHREECYCLE,
        i_AC_CLEAR    => sim_i_AC_CLEAR,
        i_BRK_RQST    => sim_i_BRK_RQST,
        i_DMAADDR     => sim_i_DMAADDR,
        i_DMADATA     => sim_i_DMADATA,
        i_EA          => sim_i_EA,
        i_EMA         => sim_i_EMA,
        i_INT_INHIBIT => sim_i_INT_INHIBIT,
        i_INT_RQST    => sim_i_INT_RQST,
        i_IO_SKIP     => sim_i_IO_SKIP,
        i_MEMDONE     => sim_i_MEMDONE,
        i_STROBE      => sim_i_STROBE,
        oBAC          => sim_oBAC,
        oBIOP1        => sim_oBIOP1,
        oBIOP2        => sim_oBIOP2,
        oBIOP4        => sim_oBIOP4,
        oBMB          => sim_oBMB,
        oBTP2         => sim_oBTP2,
        oBTP3         => sim_oBTP3,
        oBTS_1        => sim_oBTS_1,
        oBTS_3        => sim_oBTS_3,
        oBWC_OVERFLOW => sim_oBWC_OVERFLOW,
        oB_BREAK      => sim_oB_BREAK,
        oE_SET_F_SET  => sim_oE_SET_F_SET,
        oJMP_JMS      => sim_oJMP_JMS,
        oLINE_LOW     => sim_oLINE_LOW,
        oMA           => sim_oMA,
        oMEMSTART     => sim_oMEMSTART,
        o_ADDR_ACCEPT => sim_o_ADDR_ACCEPT,
        o_BF_ENABLE   => sim_o_BF_ENABLE,
        o_BUSINIT     => sim_o_BUSINIT,
        o_B_RUN       => sim_o_B_RUN,
        o_DF_ENABLE   => sim_o_DF_ENABLE,
        o_KEY_CLEAR   => sim_o_KEY_CLEAR,
        o_KEY_DF      => sim_o_KEY_DF,
        o_KEY_IF      => sim_o_KEY_IF,
        o_KEY_LOAD    => sim_o_KEY_LOAD,
        o_LOAD_SF     => sim_o_LOAD_SF,
        o_SP_CYC_NEXT => sim_o_SP_CYC_NEXT,
        lbAC          => sim_lbAC,
        lbBRK         => sim_lbBRK,
        lbCA          => sim_lbCA,
        lbDEF         => sim_lbDEF,
        lbEA          => sim_lbEA,
        lbEXE         => sim_lbEXE,
        lbFET         => sim_lbFET,
        lbION         => sim_lbION,
        lbIR          => sim_lbIR,
        lbLINK        => sim_lbLINK,
        lbMA          => sim_lbMA,
        lbMB          => sim_lbMB,
        lbRUN         => sim_lbRUN,
        lbWC          => sim_lbWC,
        swCONT        => sim_swCONT,
        swDEP         => sim_swDEP,
        swDFLD        => sim_swDFLD,
        swEXAM        => sim_swEXAM,
        swIFLD        => sim_swIFLD,
        swLDAD        => sim_swLDAD,
        swMPRT        => sim_swMPRT,
        swSTEP        => sim_swSTEP,
        swSTOP        => sim_swSTOP,
        swSR          => sim_swSR,
        swSTART       => sim_swSTART

        ,majstate  => regctlk(02 downto 00)
        ,timedelay => regctlk(08 downto 03)
        ,timestate => regctlk(13 downto 09)
        ,cyclectr  => regctlk(23 downto 14)

        ,nanocycle => nanocycle
        ,nanostep  => nanostep
        ,lastnanostep => lastnanostep
    );

    regctlk(31 downto 29) <= (others => '0');

    ---------------------
    --  io interfaces  --
    ---------------------

    ioreset  <= inusereset = '1' or dev_o_BUSINIT = '0';    -- reset io devices
    armwrite <= saxiWREADY = '1' and saxi_WVALID = '1';     -- arm is writing a backside register (single fpga clock cycle)

    -- generate iopstart pulse for an io instruction followed by iopstop
    --  iopstart is pulsed 130nS after the first iop for an instruction and lasts a single fpga clock cycle
    --   it is delayed if armwrite is happening at the same time
    --   interfaces know they can decode the io opcode in dev_oBMB and drive the busses
    --  iopstop is turned on 70nS after the end of that same iop and may last a long time
    --   interfaces must stop driving busses at this time
    --   it may happen same time as armwrite but since it lasts a long time, it will still be seen

    -- interfaces are assumed to have this form:
    --   if ioreset do ioreset processing
    --   else if armwrite do armwrite processing
    --   else if iopstart do iopstart processing
    --   else if iopstop do iopstop processing

    firstiop1 <= dev_oBIOP1 = '1' and dev_oBMB(0) = '1';                -- any 110xxxxxxxx1 executes only on IOP1
    firstiop2 <= dev_oBIOP2 = '1' and dev_oBMB(1 downto 0) = b"10";     -- any 110xxxxxxx10 executes only on IOP2
    firstiop4 <= dev_oBIOP4 = '1' and dev_oBMB(2 downto 0) = b"100";    -- any 110xxxxxx100 executes only on IOP4
                                                                        -- any 110xxxxxx000 never executes

    process (CLOCK)
    begin
        if rising_edge (CLOCK) then
            if ioreset then
                iopsetcount <= 0;
                iopclrcount <= 7;
            elsif firstiop1 or firstiop2 or firstiop4 then
                -- somewhere inside the first IOP for an instruction
                -- 130nS into it, blip a iopstart pulse for one fpga clock
                --   but hold it off if there would be an armwrite at same time
                iopclrcount <= 0;
                if iopsetcount < 13 or (iopsetcount = 13 and not armwrite) or iopsetcount = 14 then
                    iopsetcount <= iopsetcount + 1;
                end if;
            else
                -- somewhere outside the first IOPn for an instruction
                -- 70nS into it, raise and hold the stop signal until next iopulse
                iopsetcount <= 0;
                if iopclrcount < 7 then
                    iopclrcount <= iopclrcount + 1;
                end if;
            end if;
        end if;
    end process;

    iopstart <= iopsetcount = 13 and not armwrite;  -- IOP started 130nS ago, process the opcode in dev_oBMB, start driving busses
    iopstop  <= iopclrcount =  7;                   -- IOP finished 70nS ago, stop driving io busses

    -- PIOBUS control
    --   outside io pulses, PIOBUS is gating MB contents into us
    --   during first half of io pulse, PIOBUS is gating AC contents into us
    --   during second half of io pulse and for 40nS after, PIOBUS is gating INPUTBUS out to PDP-8/L

    -- receive AC contents from PIOBUS, but it is valid only during first half of io pulse (ie, when r_BAC is asserted), garbage otherwise
    oBAC <= bPIOBUSF & bPIOBUSN & bPIOBUSL  &  bPIOBUSM & bPIOBUSE & bPIOBUSD  &
            bPIOBUSK & bPIOBUSC & bPIOBUSJ  &  bPIOBUSB & bPIOBUSH & bPIOBUSA;

    -- gate INPUTBUS out to PIOBUS whenever it is enabled (from second half of io pulse to 40nS afterward)
    x_INPUTBUS <= buf_x_INPUTBUS;
    bPIOBUSA <= iINPUTBUS(00) when buf_x_INPUTBUS = '0' else 'Z';
    bPIOBUSH <= iINPUTBUS(01) when buf_x_INPUTBUS = '0' else 'Z';
    bPIOBUSB <= iINPUTBUS(02) when buf_x_INPUTBUS = '0' else 'Z';
    bPIOBUSJ <= iINPUTBUS(03) when buf_x_INPUTBUS = '0' else 'Z';
    bPIOBUSL <= iINPUTBUS(04) when buf_x_INPUTBUS = '0' else 'Z';
    bPIOBUSE <= iINPUTBUS(05) when buf_x_INPUTBUS = '0' else 'Z';
    bPIOBUSM <= iINPUTBUS(06) when buf_x_INPUTBUS = '0' else 'Z';
    bPIOBUSF <= iINPUTBUS(07) when buf_x_INPUTBUS = '0' else 'Z';
    bPIOBUSN <= iINPUTBUS(08) when buf_x_INPUTBUS = '0' else 'Z';
    bPIOBUSC <= iINPUTBUS(09) when buf_x_INPUTBUS = '0' else 'Z';
    bPIOBUSK <= iINPUTBUS(10) when buf_x_INPUTBUS = '0' else 'Z';
    bPIOBUSD <= iINPUTBUS(11) when buf_x_INPUTBUS = '0' else 'Z';

    -- latch MB contents from piobus when not in io pulse
    -- MB holds io opcode being executed during io pulses and has been valid for a few hundred nanoseconds
    -- clock it continuously when outside of io pulse so it tracks MB contents for other purposes (eg, writing extended memory)
    process (CLOCK)
    begin
        if rising_edge (CLOCK) then
            if inusereset ='1' then
                r_BAC <= '1';
                r_BMB <= '1';
                buf_x_INPUTBUS <= '1';
            elsif iopsetcount = 0 and iopclrcount = 7 then
                -- not doing any io, continuously clock in MB contents
                r_BAC <= '1';
                r_BMB <= '0';
                buf_x_INPUTBUS <= '1';
                oBMB  <= bPIOBUSF & bPIOBUSN & bPIOBUSM  &  bPIOBUSE & bPIOBUSC & bPIOBUSJ  &
                         bPIOBUSD & bPIOBUSL & bPIOBUSK  &  bPIOBUSB & bPIOBUSA & bPIOBUSH;
            elsif iopsetcount =  1 and iopclrcount = 0 then
                -- just started a long (500+ nS) io pulse, turn off passing MB onto PIOBUS
                -- ...and leave oBMB contents as they were (contains io opcode)
                r_BMB <= '1';
            elsif iopsetcount =  2 and iopclrcount = 0 then
                -- ... then turn on passing AC onto PIOBUS
                r_BAC <= '0';
            elsif iopsetcount = 14 and iopclrcount = 0 then
                r_BAC <= '1';
            elsif iopsetcount = 15 and iopclrcount = 0 then
                buf_x_INPUTBUS <= '0';
            elsif iopsetcount =  0 and iopclrcount = 4 then
                buf_x_INPUTBUS <= '1';
            end if;
        end if;
    end process;

    -- internal pio busses - wired-or from device to processor

    dev_i_AC_CLEAR <= '0' when ttacclr or tt40acclr or rkacclr else '1';
    dev_i_INT_RQST <= '0' when ttintrq or tt40intrq or rkintrq else '1';
    dev_i_IO_SKIP  <= '0' when ttioskp or tt40ioskp or rkioskp else '1';
    dev_iINPUTBUS  <= ttibus or tt40ibus or rkibus;

    -- teletype interfaces

    ttawrite   <= armwrite and writeaddr(11 downto 4) = b"00001010";    -- 00001010xx00
    tt40awrite <= armwrite and writeaddr(11 downto 4) = b"00001011";    -- 00001011xx00

    ttinst: pdp8ltty port map (
        CLOCK => CLOCK,
        RESET => ioreset,

        armwrite => ttawrite,
        armraddr => readaddr(3 downto 2),
        armwaddr => writeaddr(3 downto 2),
        armwdata => saxi_WDATA,
        armrdata => ttardata,

        iopstart => iopstart,
        iopstop  => iopstop,
        ioopcode => dev_oBMB,
        cputodev => dev_oBAC,

        devtocpu => ttibus,
        AC_CLEAR => ttacclr,
        IO_SKIP  => ttioskp,
        INT_RQST => ttintrq
    );

    tt40inst: pdp8ltty
        generic map (KBDEV => std_logic_vector (to_unsigned (8#40#, 6)))
        port map (
            CLOCK => CLOCK,
            RESET => ioreset,

            armwrite => tt40awrite,
            armraddr => readaddr(3 downto 2),
            armwaddr => writeaddr(3 downto 2),
            armwdata => saxi_WDATA,
            armrdata => tt40ardata,

            iopstart => iopstart,
            iopstop  => iopstop,
            ioopcode => dev_oBMB,
            cputodev => dev_oBAC,

            devtocpu => tt40ibus,
            AC_CLEAR => tt40acclr,
            IO_SKIP  => tt40ioskp,
            INT_RQST => tt40intrq
        );

    -- disk interface

    rkawrite <= armwrite and writeaddr(11 downto 5) = b"0000100";       -- 0000100xxx00

    rkinst: pdp8lrk8je port map (
        CLOCK => CLOCK,
        RESET => ioreset,

        armwrite => rkawrite,
        armraddr => readaddr(4 downto 2),
        armwaddr => writeaddr(4 downto 2),
        armwdata => saxi_WDATA,
        armrdata => rkardata,

        iopstart => iopstart,
        iopstop  => iopstop,
        ioopcode => dev_oBMB,
        cputodev => dev_oBAC,

        devtocpu => rkibus,
        AC_CLEAR => rkacclr,
        IO_SKIP  => rkioskp,
        INT_RQST => rkintrq
    );

end rtl;
