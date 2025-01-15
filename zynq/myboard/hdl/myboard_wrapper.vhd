--Copyright 1986-2018 Xilinx, Inc. All Rights Reserved.
----------------------------------------------------------------------------------
--Tool Version: Vivado v.2018.3 (lin64) Build 2405991 Thu Dec  6 23:36:41 MST 2018
--Date        : Tue Jan 14 20:23:17 2025
--Host        : homepc2 running 64-bit Ubuntu 16.04.7 LTS
--Command     : generate_target myboard_wrapper.bd
--Design      : myboard_wrapper
--Purpose     : IP block netlist
----------------------------------------------------------------------------------
library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
library UNISIM;
use UNISIM.VCOMPONENTS.ALL;
entity myboard_wrapper is
  port (
    DDR_addr : inout STD_LOGIC_VECTOR ( 14 downto 0 );
    DDR_ba : inout STD_LOGIC_VECTOR ( 2 downto 0 );
    DDR_cas_n : inout STD_LOGIC;
    DDR_ck_n : inout STD_LOGIC;
    DDR_ck_p : inout STD_LOGIC;
    DDR_cke : inout STD_LOGIC;
    DDR_cs_n : inout STD_LOGIC;
    DDR_dm : inout STD_LOGIC_VECTOR ( 3 downto 0 );
    DDR_dq : inout STD_LOGIC_VECTOR ( 31 downto 0 );
    DDR_dqs_n : inout STD_LOGIC_VECTOR ( 3 downto 0 );
    DDR_dqs_p : inout STD_LOGIC_VECTOR ( 3 downto 0 );
    DDR_odt : inout STD_LOGIC;
    DDR_ras_n : inout STD_LOGIC;
    DDR_reset_n : inout STD_LOGIC;
    DDR_we_n : inout STD_LOGIC;
    FIXED_IO_ddr_vrn : inout STD_LOGIC;
    FIXED_IO_ddr_vrp : inout STD_LOGIC;
    FIXED_IO_mio : inout STD_LOGIC_VECTOR ( 53 downto 0 );
    FIXED_IO_ps_clk : inout STD_LOGIC;
    FIXED_IO_ps_porb : inout STD_LOGIC;
    FIXED_IO_ps_srstb : inout STD_LOGIC;
    LEDoutB_0 : out STD_LOGIC;
    LEDoutG_0 : out STD_LOGIC;
    LEDoutR_0 : out STD_LOGIC;
    bDMABUSA_0 : inout STD_LOGIC;
    bDMABUSB_0 : inout STD_LOGIC;
    bDMABUSC_0 : inout STD_LOGIC;
    bDMABUSD_0 : inout STD_LOGIC;
    bDMABUSE_0 : inout STD_LOGIC;
    bDMABUSF_0 : inout STD_LOGIC;
    bDMABUSH_0 : inout STD_LOGIC;
    bDMABUSJ_0 : inout STD_LOGIC;
    bDMABUSK_0 : inout STD_LOGIC;
    bDMABUSL_0 : inout STD_LOGIC;
    bDMABUSM_0 : inout STD_LOGIC;
    bDMABUSN_0 : inout STD_LOGIC;
    bFPI2CDATA_0 : inout STD_LOGIC;
    bMEMBUSA_0 : inout STD_LOGIC;
    bMEMBUSB_0 : inout STD_LOGIC;
    bMEMBUSC_0 : inout STD_LOGIC;
    bMEMBUSD_0 : inout STD_LOGIC;
    bMEMBUSE_0 : inout STD_LOGIC;
    bMEMBUSF_0 : inout STD_LOGIC;
    bMEMBUSH_0 : inout STD_LOGIC;
    bMEMBUSJ_0 : inout STD_LOGIC;
    bMEMBUSK_0 : inout STD_LOGIC;
    bMEMBUSL_0 : inout STD_LOGIC;
    bMEMBUSM_0 : inout STD_LOGIC;
    bMEMBUSN_0 : inout STD_LOGIC;
    bPIOBUSA_0 : inout STD_LOGIC;
    bPIOBUSB_0 : inout STD_LOGIC;
    bPIOBUSC_0 : inout STD_LOGIC;
    bPIOBUSD_0 : inout STD_LOGIC;
    bPIOBUSE_0 : inout STD_LOGIC;
    bPIOBUSF_0 : inout STD_LOGIC;
    bPIOBUSH_0 : inout STD_LOGIC;
    bPIOBUSJ_0 : inout STD_LOGIC;
    bPIOBUSK_0 : inout STD_LOGIC;
    bPIOBUSL_0 : inout STD_LOGIC;
    bPIOBUSM_0 : inout STD_LOGIC;
    bPIOBUSN_0 : inout STD_LOGIC;
    i3CYCLE_0 : out STD_LOGIC;
    iAC_CLEAR_0 : out STD_LOGIC;
    iBEMA_0 : out STD_LOGIC;
    iBRK_RQST_0 : out STD_LOGIC;
    iEMA_0 : out STD_LOGIC;
    iEXTDMAADD_12_0 : out STD_LOGIC;
    iFPI2CCLK_0 : out STD_LOGIC;
    iFPI2CDDIR_0 : out STD_LOGIC;
    iINT_INHIBIT_0 : out STD_LOGIC;
    iINT_RQST_0 : out STD_LOGIC;
    iIO_SKIP_0 : out STD_LOGIC;
    iMEMINCR_0 : out STD_LOGIC;
    i_B36V1_0 : out STD_LOGIC;
    i_CA_INCRMNT_0 : out STD_LOGIC;
    i_D36B2_0 : out STD_LOGIC;
    i_DATA_IN_0 : out STD_LOGIC;
    i_EA_0 : out STD_LOGIC;
    i_FPI2CDENA_0 : out STD_LOGIC;
    i_MEMDONE_0 : out STD_LOGIC;
    i_MEM_07_0 : out STD_LOGIC;
    i_STROBE_0 : out STD_LOGIC;
    oBIOP1_0 : in STD_LOGIC;
    oBIOP2_0 : in STD_LOGIC;
    oBIOP4_0 : in STD_LOGIC;
    oBTP2_0 : in STD_LOGIC;
    oBTP3_0 : in STD_LOGIC;
    oBTS_1_0 : in STD_LOGIC;
    oBTS_3_0 : in STD_LOGIC;
    oBUSINIT_0 : in STD_LOGIC;
    oB_RUN_0 : in STD_LOGIC;
    oC36B2_0 : in STD_LOGIC;
    oD35B2_0 : in STD_LOGIC;
    oE_SET_F_SET_0 : in STD_LOGIC;
    oJMP_JMS_0 : in STD_LOGIC;
    oLINE_LOW_0 : in STD_LOGIC;
    oMEMSTART_0 : in STD_LOGIC;
    o_ADDR_ACCEPT_0 : in STD_LOGIC;
    o_BF_ENABLE_0 : in STD_LOGIC;
    o_BWC_OVERFLOW_0 : in STD_LOGIC;
    o_B_BREAK_0 : in STD_LOGIC;
    o_DF_ENABLE_0 : in STD_LOGIC;
    o_KEY_CLEAR_0 : in STD_LOGIC;
    o_KEY_DF_0 : in STD_LOGIC;
    o_KEY_IF_0 : in STD_LOGIC;
    o_KEY_LOAD_0 : in STD_LOGIC;
    o_LOAD_SF_0 : in STD_LOGIC;
    o_SP_CYC_NEXT_0 : in STD_LOGIC;
    r_BAC_0 : out STD_LOGIC;
    r_BMB_0 : out STD_LOGIC;
    r_MA_0 : out STD_LOGIC;
    x_DMAADDR_0 : out STD_LOGIC;
    x_DMADATA_0 : out STD_LOGIC;
    x_INPUTBUS_0 : out STD_LOGIC;
    x_MEM_0 : out STD_LOGIC
  );
end myboard_wrapper;

architecture STRUCTURE of myboard_wrapper is
  component myboard is
  port (
    LEDoutB_0 : out STD_LOGIC;
    LEDoutG_0 : out STD_LOGIC;
    LEDoutR_0 : out STD_LOGIC;
    bDMABUSA_0 : inout STD_LOGIC;
    bDMABUSB_0 : inout STD_LOGIC;
    bDMABUSC_0 : inout STD_LOGIC;
    bDMABUSD_0 : inout STD_LOGIC;
    bDMABUSE_0 : inout STD_LOGIC;
    bDMABUSF_0 : inout STD_LOGIC;
    bDMABUSH_0 : inout STD_LOGIC;
    bDMABUSJ_0 : inout STD_LOGIC;
    bDMABUSK_0 : inout STD_LOGIC;
    bDMABUSL_0 : inout STD_LOGIC;
    bDMABUSM_0 : inout STD_LOGIC;
    bDMABUSN_0 : inout STD_LOGIC;
    bFPI2CDATA_0 : inout STD_LOGIC;
    bMEMBUSA_0 : inout STD_LOGIC;
    bMEMBUSB_0 : inout STD_LOGIC;
    bMEMBUSC_0 : inout STD_LOGIC;
    bMEMBUSD_0 : inout STD_LOGIC;
    bMEMBUSE_0 : inout STD_LOGIC;
    bMEMBUSF_0 : inout STD_LOGIC;
    bMEMBUSH_0 : inout STD_LOGIC;
    bMEMBUSJ_0 : inout STD_LOGIC;
    bMEMBUSK_0 : inout STD_LOGIC;
    bMEMBUSL_0 : inout STD_LOGIC;
    bMEMBUSM_0 : inout STD_LOGIC;
    bMEMBUSN_0 : inout STD_LOGIC;
    bPIOBUSA_0 : inout STD_LOGIC;
    bPIOBUSB_0 : inout STD_LOGIC;
    bPIOBUSC_0 : inout STD_LOGIC;
    bPIOBUSD_0 : inout STD_LOGIC;
    bPIOBUSE_0 : inout STD_LOGIC;
    bPIOBUSF_0 : inout STD_LOGIC;
    bPIOBUSH_0 : inout STD_LOGIC;
    bPIOBUSJ_0 : inout STD_LOGIC;
    bPIOBUSK_0 : inout STD_LOGIC;
    bPIOBUSL_0 : inout STD_LOGIC;
    bPIOBUSM_0 : inout STD_LOGIC;
    bPIOBUSN_0 : inout STD_LOGIC;
    i3CYCLE_0 : out STD_LOGIC;
    iAC_CLEAR_0 : out STD_LOGIC;
    iBEMA_0 : out STD_LOGIC;
    iBRK_RQST_0 : out STD_LOGIC;
    iEMA_0 : out STD_LOGIC;
    iEXTDMAADD_12_0 : out STD_LOGIC;
    iFPI2CCLK_0 : out STD_LOGIC;
    iFPI2CDDIR_0 : out STD_LOGIC;
    iINT_INHIBIT_0 : out STD_LOGIC;
    iINT_RQST_0 : out STD_LOGIC;
    iIO_SKIP_0 : out STD_LOGIC;
    i_B36V1_0 : out STD_LOGIC;
    i_CA_INCRMNT_0 : out STD_LOGIC;
    i_D36B2_0 : out STD_LOGIC;
    i_DATA_IN_0 : out STD_LOGIC;
    i_EA_0 : out STD_LOGIC;
    i_FPI2CDENA_0 : out STD_LOGIC;
    i_MEMDONE_0 : out STD_LOGIC;
    iMEMINCR_0 : out STD_LOGIC;
    i_MEM_07_0 : out STD_LOGIC;
    i_STROBE_0 : out STD_LOGIC;
    oBIOP1_0 : in STD_LOGIC;
    oBIOP2_0 : in STD_LOGIC;
    oBIOP4_0 : in STD_LOGIC;
    oBTP2_0 : in STD_LOGIC;
    oBTP3_0 : in STD_LOGIC;
    oBTS_1_0 : in STD_LOGIC;
    oBTS_3_0 : in STD_LOGIC;
    oBUSINIT_0 : in STD_LOGIC;
    oB_RUN_0 : in STD_LOGIC;
    oC36B2_0 : in STD_LOGIC;
    oD35B2_0 : in STD_LOGIC;
    oE_SET_F_SET_0 : in STD_LOGIC;
    oJMP_JMS_0 : in STD_LOGIC;
    oLINE_LOW_0 : in STD_LOGIC;
    oMEMSTART_0 : in STD_LOGIC;
    o_ADDR_ACCEPT_0 : in STD_LOGIC;
    o_BF_ENABLE_0 : in STD_LOGIC;
    o_BWC_OVERFLOW_0 : in STD_LOGIC;
    o_B_BREAK_0 : in STD_LOGIC;
    o_DF_ENABLE_0 : in STD_LOGIC;
    o_KEY_CLEAR_0 : in STD_LOGIC;
    o_KEY_DF_0 : in STD_LOGIC;
    o_KEY_IF_0 : in STD_LOGIC;
    o_KEY_LOAD_0 : in STD_LOGIC;
    o_LOAD_SF_0 : in STD_LOGIC;
    o_SP_CYC_NEXT_0 : in STD_LOGIC;
    r_BAC_0 : out STD_LOGIC;
    r_BMB_0 : out STD_LOGIC;
    r_MA_0 : out STD_LOGIC;
    x_DMAADDR_0 : out STD_LOGIC;
    x_DMADATA_0 : out STD_LOGIC;
    x_INPUTBUS_0 : out STD_LOGIC;
    x_MEM_0 : out STD_LOGIC;
    FIXED_IO_mio : inout STD_LOGIC_VECTOR ( 53 downto 0 );
    FIXED_IO_ddr_vrn : inout STD_LOGIC;
    FIXED_IO_ddr_vrp : inout STD_LOGIC;
    FIXED_IO_ps_srstb : inout STD_LOGIC;
    FIXED_IO_ps_clk : inout STD_LOGIC;
    FIXED_IO_ps_porb : inout STD_LOGIC;
    DDR_cas_n : inout STD_LOGIC;
    DDR_cke : inout STD_LOGIC;
    DDR_ck_n : inout STD_LOGIC;
    DDR_ck_p : inout STD_LOGIC;
    DDR_cs_n : inout STD_LOGIC;
    DDR_reset_n : inout STD_LOGIC;
    DDR_odt : inout STD_LOGIC;
    DDR_ras_n : inout STD_LOGIC;
    DDR_we_n : inout STD_LOGIC;
    DDR_ba : inout STD_LOGIC_VECTOR ( 2 downto 0 );
    DDR_addr : inout STD_LOGIC_VECTOR ( 14 downto 0 );
    DDR_dm : inout STD_LOGIC_VECTOR ( 3 downto 0 );
    DDR_dq : inout STD_LOGIC_VECTOR ( 31 downto 0 );
    DDR_dqs_n : inout STD_LOGIC_VECTOR ( 3 downto 0 );
    DDR_dqs_p : inout STD_LOGIC_VECTOR ( 3 downto 0 )
  );
  end component myboard;
begin
myboard_i: component myboard
     port map (
      DDR_addr(14 downto 0) => DDR_addr(14 downto 0),
      DDR_ba(2 downto 0) => DDR_ba(2 downto 0),
      DDR_cas_n => DDR_cas_n,
      DDR_ck_n => DDR_ck_n,
      DDR_ck_p => DDR_ck_p,
      DDR_cke => DDR_cke,
      DDR_cs_n => DDR_cs_n,
      DDR_dm(3 downto 0) => DDR_dm(3 downto 0),
      DDR_dq(31 downto 0) => DDR_dq(31 downto 0),
      DDR_dqs_n(3 downto 0) => DDR_dqs_n(3 downto 0),
      DDR_dqs_p(3 downto 0) => DDR_dqs_p(3 downto 0),
      DDR_odt => DDR_odt,
      DDR_ras_n => DDR_ras_n,
      DDR_reset_n => DDR_reset_n,
      DDR_we_n => DDR_we_n,
      FIXED_IO_ddr_vrn => FIXED_IO_ddr_vrn,
      FIXED_IO_ddr_vrp => FIXED_IO_ddr_vrp,
      FIXED_IO_mio(53 downto 0) => FIXED_IO_mio(53 downto 0),
      FIXED_IO_ps_clk => FIXED_IO_ps_clk,
      FIXED_IO_ps_porb => FIXED_IO_ps_porb,
      FIXED_IO_ps_srstb => FIXED_IO_ps_srstb,
      LEDoutB_0 => LEDoutB_0,
      LEDoutG_0 => LEDoutG_0,
      LEDoutR_0 => LEDoutR_0,
      bDMABUSA_0 => bDMABUSA_0,
      bDMABUSB_0 => bDMABUSB_0,
      bDMABUSC_0 => bDMABUSC_0,
      bDMABUSD_0 => bDMABUSD_0,
      bDMABUSE_0 => bDMABUSE_0,
      bDMABUSF_0 => bDMABUSF_0,
      bDMABUSH_0 => bDMABUSH_0,
      bDMABUSJ_0 => bDMABUSJ_0,
      bDMABUSK_0 => bDMABUSK_0,
      bDMABUSL_0 => bDMABUSL_0,
      bDMABUSM_0 => bDMABUSM_0,
      bDMABUSN_0 => bDMABUSN_0,
      bFPI2CDATA_0 => bFPI2CDATA_0,
      bMEMBUSA_0 => bMEMBUSA_0,
      bMEMBUSB_0 => bMEMBUSB_0,
      bMEMBUSC_0 => bMEMBUSC_0,
      bMEMBUSD_0 => bMEMBUSD_0,
      bMEMBUSE_0 => bMEMBUSE_0,
      bMEMBUSF_0 => bMEMBUSF_0,
      bMEMBUSH_0 => bMEMBUSH_0,
      bMEMBUSJ_0 => bMEMBUSJ_0,
      bMEMBUSK_0 => bMEMBUSK_0,
      bMEMBUSL_0 => bMEMBUSL_0,
      bMEMBUSM_0 => bMEMBUSM_0,
      bMEMBUSN_0 => bMEMBUSN_0,
      bPIOBUSA_0 => bPIOBUSA_0,
      bPIOBUSB_0 => bPIOBUSB_0,
      bPIOBUSC_0 => bPIOBUSC_0,
      bPIOBUSD_0 => bPIOBUSD_0,
      bPIOBUSE_0 => bPIOBUSE_0,
      bPIOBUSF_0 => bPIOBUSF_0,
      bPIOBUSH_0 => bPIOBUSH_0,
      bPIOBUSJ_0 => bPIOBUSJ_0,
      bPIOBUSK_0 => bPIOBUSK_0,
      bPIOBUSL_0 => bPIOBUSL_0,
      bPIOBUSM_0 => bPIOBUSM_0,
      bPIOBUSN_0 => bPIOBUSN_0,
      i3CYCLE_0 => i3CYCLE_0,
      iAC_CLEAR_0 => iAC_CLEAR_0,
      iBEMA_0 => iBEMA_0,
      iBRK_RQST_0 => iBRK_RQST_0,
      iEMA_0 => iEMA_0,
      iEXTDMAADD_12_0 => iEXTDMAADD_12_0,
      iFPI2CCLK_0 => iFPI2CCLK_0,
      iFPI2CDDIR_0 => iFPI2CDDIR_0,
      iINT_INHIBIT_0 => iINT_INHIBIT_0,
      iINT_RQST_0 => iINT_RQST_0,
      iIO_SKIP_0 => iIO_SKIP_0,
      iMEMINCR_0 => iMEMINCR_0,
      i_B36V1_0 => i_B36V1_0,
      i_CA_INCRMNT_0 => i_CA_INCRMNT_0,
      i_D36B2_0 => i_D36B2_0,
      i_DATA_IN_0 => i_DATA_IN_0,
      i_EA_0 => i_EA_0,
      i_FPI2CDENA_0 => i_FPI2CDENA_0,
      i_MEMDONE_0 => i_MEMDONE_0,
      i_MEM_07_0 => i_MEM_07_0,
      i_STROBE_0 => i_STROBE_0,
      oBIOP1_0 => oBIOP1_0,
      oBIOP2_0 => oBIOP2_0,
      oBIOP4_0 => oBIOP4_0,
      oBTP2_0 => oBTP2_0,
      oBTP3_0 => oBTP3_0,
      oBTS_1_0 => oBTS_1_0,
      oBTS_3_0 => oBTS_3_0,
      oBUSINIT_0 => oBUSINIT_0,
      oB_RUN_0 => oB_RUN_0,
      oC36B2_0 => oC36B2_0,
      oD35B2_0 => oD35B2_0,
      oE_SET_F_SET_0 => oE_SET_F_SET_0,
      oJMP_JMS_0 => oJMP_JMS_0,
      oLINE_LOW_0 => oLINE_LOW_0,
      oMEMSTART_0 => oMEMSTART_0,
      o_ADDR_ACCEPT_0 => o_ADDR_ACCEPT_0,
      o_BF_ENABLE_0 => o_BF_ENABLE_0,
      o_BWC_OVERFLOW_0 => o_BWC_OVERFLOW_0,
      o_B_BREAK_0 => o_B_BREAK_0,
      o_DF_ENABLE_0 => o_DF_ENABLE_0,
      o_KEY_CLEAR_0 => o_KEY_CLEAR_0,
      o_KEY_DF_0 => o_KEY_DF_0,
      o_KEY_IF_0 => o_KEY_IF_0,
      o_KEY_LOAD_0 => o_KEY_LOAD_0,
      o_LOAD_SF_0 => o_LOAD_SF_0,
      o_SP_CYC_NEXT_0 => o_SP_CYC_NEXT_0,
      r_BAC_0 => r_BAC_0,
      r_BMB_0 => r_BMB_0,
      r_MA_0 => r_MA_0,
      x_DMAADDR_0 => x_DMAADDR_0,
      x_DMADATA_0 => x_DMADATA_0,
      x_INPUTBUS_0 => x_INPUTBUS_0,
      x_MEM_0 => x_MEM_0
    );
end STRUCTURE;
