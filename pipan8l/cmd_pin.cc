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

// tcl command to give direct access to z8l fpga signals

#include <string.h>

#include "cmd_pin.h"
#include "tclmain.h"
#include "z8ldefs.h"
#include "z8lutil.h"

// pin definitions
struct PinDef {
    char name[16];
    int dev;
    int reg;
    uint32_t mask;
    bool writ;
};

#define DEV_8L 0
#define DEV_CM 1
#define DEV_TT 2
#define DEV_XM 3
#define DEV_SH 4
#define DEV_FP 5

static uint32_t volatile *devs[6];

static PinDef const pindefs[] = {
    { "iBEMA",           DEV_8L, Z_RA, a_iBEMA,            true  },
    { "i_CA_INCRMNT",    DEV_8L, Z_RA, a_i_CA_INCRMNT,     true  },
    { "i_DATA_IN",       DEV_8L, Z_RA, a_i_DATA_IN,        true  },
    { "iMEMINCR",        DEV_8L, Z_RA, a_iMEMINCR,         true  },
    { "i_MEM_P",         DEV_8L, Z_RA, a_i_MEM_P,          true  },
    { "i3CYCLE",         DEV_8L, Z_RA, a_i3CYCLE,          true  },
    { "iAC_CLEAR",       DEV_8L, Z_RA, a_iAC_CLEAR,        true  },
    { "iBRK_RQST",       DEV_8L, Z_RA, a_iBRK_RQST,        true  },
    { "i_EA",            DEV_8L, Z_RA, a_i_EA,             true  },
    { "iEMA",            DEV_8L, Z_RA, a_iEMA,             true  },
    { "iINT_INHIBIT",    DEV_8L, Z_RA, a_iINT_INHIBIT,     true  },
    { "iINT_RQST",       DEV_8L, Z_RA, a_iINT_RQST,        true  },
    { "iIO_SKIP",        DEV_8L, Z_RA, a_iIO_SKIP,         true  },
    { "i_MEMDONE",       DEV_8L, Z_RA, a_i_MEMDONE,        true  },
    { "i_STROBE",        DEV_8L, Z_RA, a_i_STROBE,         true  },
    { "i_B36V1",         DEV_8L, Z_RA, a_i_B36V1,          true  },
    { "i_D36B2",         DEV_8L, Z_RA, a_i_D36B2,          true  },
    { "swCONT",          DEV_8L, Z_RB, b_swCONT,           true  },
    { "swDEP",           DEV_8L, Z_RB, b_swDEP,            true  },
    { "swDFLD",          DEV_8L, Z_RB, b_swDFLD,           true  },
    { "swEXAM",          DEV_8L, Z_RB, b_swEXAM,           true  },
    { "swIFLD",          DEV_8L, Z_RB, b_swIFLD,           true  },
    { "swLDAD",          DEV_8L, Z_RB, b_swLDAD,           true  },
    { "swMPRT",          DEV_8L, Z_RB, b_swMPRT,           true  },
    { "swSTEP",          DEV_8L, Z_RB, b_swSTEP,           true  },
    { "swSTOP",          DEV_8L, Z_RB, b_swSTOP,           true  },
    { "swSTART",         DEV_8L, Z_RB, b_swSTART,          true  },
    { "swSR",            DEV_8L, Z_RB, b_swSR,             true  },
    { "iINPUTBUS",       DEV_8L, Z_RC, c_iINPUTBUS,        true  },
    { "i_MEM",           DEV_8L, Z_RC, c_i_MEM,            true  },
    { "i_MEM_00",        DEV_8L, Z_RC, c_i_MEM0 << 11,     true  },
    { "i_MEM_01",        DEV_8L, Z_RC, c_i_MEM0 << 10,     true  },
    { "i_MEM_02",        DEV_8L, Z_RC, c_i_MEM0 <<  9,     true  },
    { "i_MEM_03",        DEV_8L, Z_RC, c_i_MEM0 <<  8,     true  },
    { "i_MEM_04",        DEV_8L, Z_RC, c_i_MEM0 <<  7,     true  },
    { "i_MEM_05",        DEV_8L, Z_RC, c_i_MEM0 <<  6,     true  },
    { "i_MEM_06",        DEV_8L, Z_RC, c_i_MEM0 <<  5,     true  },
    { "i_MEM_07",        DEV_8L, Z_RC, c_i_MEM0 <<  4,     true  },
    { "i_MEM_08",        DEV_8L, Z_RC, c_i_MEM0 <<  3,     true  },
    { "i_MEM_09",        DEV_8L, Z_RC, c_i_MEM0 <<  2,     true  },
    { "i_MEM_10",        DEV_8L, Z_RC, c_i_MEM0 <<  1,     true  },
    { "i_MEM_11",        DEV_8L, Z_RC, c_i_MEM0 <<  0,     true  },
    { "iDMAADDR",        DEV_8L, Z_RD, d_iDMAADDR,         true  },
    { "iDMAADDR_00",     DEV_8L, Z_RD, d_iDMAADDR0 << 11,  true  },
    { "iDMAADDR_01",     DEV_8L, Z_RD, d_iDMAADDR0 << 10,  true  },
    { "iDMAADDR_02",     DEV_8L, Z_RD, d_iDMAADDR0 <<  9,  true  },
    { "iDMAADDR_03",     DEV_8L, Z_RD, d_iDMAADDR0 <<  8,  true  },
    { "iDMAADDR_04",     DEV_8L, Z_RD, d_iDMAADDR0 <<  7,  true  },
    { "iDMAADDR_05",     DEV_8L, Z_RD, d_iDMAADDR0 <<  6,  true  },
    { "iDMAADDR_06",     DEV_8L, Z_RD, d_iDMAADDR0 <<  5,  true  },
    { "iDMAADDR_07",     DEV_8L, Z_RD, d_iDMAADDR0 <<  4,  true  },
    { "iDMAADDR_08",     DEV_8L, Z_RD, d_iDMAADDR0 <<  3,  true  },
    { "iDMAADDR_09",     DEV_8L, Z_RD, d_iDMAADDR0 <<  2,  true  },
    { "iDMAADDR_10",     DEV_8L, Z_RD, d_iDMAADDR0 <<  1,  true  },
    { "iDMAADDR_11",     DEV_8L, Z_RD, d_iDMAADDR0 <<  0,  true  },
    { "iDMADATA",        DEV_8L, Z_RD, d_iDMADATA,         true  },
    { "iDMADATA_00",     DEV_8L, Z_RD, d_iDMADATA0 << 11,  true  },
    { "iDMADATA_01",     DEV_8L, Z_RD, d_iDMADATA0 << 10,  true  },
    { "iDMADATA_02",     DEV_8L, Z_RD, d_iDMADATA0 <<  9,  true  },
    { "iDMADATA_03",     DEV_8L, Z_RD, d_iDMADATA0 <<  8,  true  },
    { "iDMADATA_04",     DEV_8L, Z_RD, d_iDMADATA0 <<  7,  true  },
    { "iDMADATA_05",     DEV_8L, Z_RD, d_iDMADATA0 <<  6,  true  },
    { "iDMADATA_06",     DEV_8L, Z_RD, d_iDMADATA0 <<  5,  true  },
    { "iDMADATA_07",     DEV_8L, Z_RD, d_iDMADATA0 <<  4,  true  },
    { "iDMADATA_08",     DEV_8L, Z_RD, d_iDMADATA0 <<  3,  true  },
    { "iDMADATA_09",     DEV_8L, Z_RD, d_iDMADATA0 <<  2,  true  },
    { "iDMADATA_10",     DEV_8L, Z_RD, d_iDMADATA0 <<  1,  true  },
    { "iDMADATA_11",     DEV_8L, Z_RD, d_iDMADATA0 <<  0,  true  },
    { "simit",           DEV_8L, Z_RE, e_simit,            true  },
    { "fpgareset",       DEV_8L, Z_RE, e_fpgareset,        true  },
    { "nanocontin",      DEV_8L, Z_RE, e_nanocontin,       true  },
    { "nanotrigger",     DEV_8L, Z_RE, e_nanotrigger,      true  },
    { "nanocstep",       DEV_8L, Z_RE, e_nanocstep,        false },
    { "bareit",          DEV_8L, Z_RE, e_bareit,           true  },
    { "oBIOP1",          DEV_8L, Z_RF, f_oBIOP1,           false },
    { "oBIOP2",          DEV_8L, Z_RF, f_oBIOP2,           false },
    { "oBIOP4",          DEV_8L, Z_RF, f_oBIOP4,           false },
    { "oBTP2",           DEV_8L, Z_RF, f_oBTP2,            false },
    { "oBTP3",           DEV_8L, Z_RF, f_oBTP3,            false },
    { "oBTS_1",          DEV_8L, Z_RF, f_oBTS_1,           false },
    { "oBTS_3",          DEV_8L, Z_RF, f_oBTS_3,           false },
    { "oC36B2",          DEV_8L, Z_RF, f_oC36B2,           false },
    { "o_BWC_OVERFLOW",  DEV_8L, Z_RF, f_o_BWC_OVERFLOW,   false },
    { "o_B_BREAK",       DEV_8L, Z_RF, f_o_B_BREAK,        false },
    { "oE_SET_F_SET",    DEV_8L, Z_RF, f_oE_SET_F_SET,     false },
    { "oJMP_JMS",        DEV_8L, Z_RF, f_oJMP_JMS,         false },
    { "oLINE_LOW",       DEV_8L, Z_RF, f_oLINE_LOW,        false },
    { "oMEMSTART",       DEV_8L, Z_RF, f_oMEMSTART,        false },
    { "o_ADDR_ACCEPT",   DEV_8L, Z_RF, f_o_ADDR_ACCEPT,    false },
    { "o_BF_ENABLE",     DEV_8L, Z_RF, f_o_BF_ENABLE,      false },
    { "o_BUSINIT",       DEV_8L, Z_RF, f_o_BUSINIT,        false },
    { "oB_RUN",          DEV_8L, Z_RF, f_oB_RUN,           false },
    { "o_DF_ENABLE",     DEV_8L, Z_RF, f_o_DF_ENABLE,      false },
    { "o_KEY_CLEAR",     DEV_8L, Z_RF, f_o_KEY_CLEAR,      false },
    { "o_KEY_DF",        DEV_8L, Z_RF, f_o_KEY_DF,         false },
    { "o_KEY_IF",        DEV_8L, Z_RF, f_o_KEY_IF,         false },
    { "o_KEY_LOAD",      DEV_8L, Z_RF, f_o_KEY_LOAD,       false },
    { "o_LOAD_SF",       DEV_8L, Z_RF, f_o_LOAD_SF,        false },
    { "o_SP_CYC_NEXT",   DEV_8L, Z_RF, f_o_SP_CYC_NEXT,    false },
    { "oD35B2",          DEV_8L, Z_RF, f_oD35B2,           false },
    { "didio",           DEV_8L, Z_RF, f_didio,            false },
    { "lbBRK",           DEV_8L, Z_RG, g_lbBRK,            false },
    { "lbCA",            DEV_8L, Z_RG, g_lbCA,             false },
    { "lbDEF",           DEV_8L, Z_RG, g_lbDEF,            false },
    { "lbEA",            DEV_8L, Z_RG, g_lbEA,             false },
    { "lbEXE",           DEV_8L, Z_RG, g_lbEXE,            false },
    { "lbFET",           DEV_8L, Z_RG, g_lbFET,            false },
    { "lbION",           DEV_8L, Z_RG, g_lbION,            false },
    { "lbLINK",          DEV_8L, Z_RG, g_lbLINK,           false },
    { "lbPRTE",          DEV_8L, Z_RG, g_lbPRTE,           false },
    { "lbRUN",           DEV_8L, Z_RG, g_lbRUN,            false },
    { "lbWC",            DEV_8L, Z_RG, g_lbWC,             false },
    { "debounced",       DEV_8L, Z_RG, g_debounced,        false },
    { "lastswLDAD",      DEV_8L, Z_RG, g_lastswLDAD,       false },
    { "lastswSTART",     DEV_8L, Z_RG, g_lastswSTART,      false },
    { "lbIR",            DEV_8L, Z_RG, g_lbIR,             false },
    { "oBAC",            DEV_8L, Z_RH, h_oBAC,             false },
    { "oBAC_00",         DEV_8L, Z_RH, h_oBAC0 << 11,      false },
    { "oBAC_01",         DEV_8L, Z_RH, h_oBAC0 << 10,      false },
    { "oBAC_02",         DEV_8L, Z_RH, h_oBAC0 <<  9,      false },
    { "oBAC_03",         DEV_8L, Z_RH, h_oBAC0 <<  8,      false },
    { "oBAC_04",         DEV_8L, Z_RH, h_oBAC0 <<  7,      false },
    { "oBAC_05",         DEV_8L, Z_RH, h_oBAC0 <<  6,      false },
    { "oBAC_06",         DEV_8L, Z_RH, h_oBAC0 <<  5,      false },
    { "oBAC_07",         DEV_8L, Z_RH, h_oBAC0 <<  4,      false },
    { "oBAC_08",         DEV_8L, Z_RH, h_oBAC0 <<  3,      false },
    { "oBAC_09",         DEV_8L, Z_RH, h_oBAC0 <<  2,      false },
    { "oBAC_10",         DEV_8L, Z_RH, h_oBAC0 <<  1,      false },
    { "oBAC_11",         DEV_8L, Z_RH, h_oBAC0 <<  0,      false },
    { "oBMB",            DEV_8L, Z_RH, h_oBMB,             false },
    { "oBMB_00",         DEV_8L, Z_RH, h_oBMB0 << 11,      false },
    { "oBMB_01",         DEV_8L, Z_RH, h_oBMB0 << 10,      false },
    { "oBMB_02",         DEV_8L, Z_RH, h_oBMB0 <<  9,      false },
    { "oBMB_03",         DEV_8L, Z_RH, h_oBMB0 <<  8,      false },
    { "oBMB_04",         DEV_8L, Z_RH, h_oBMB0 <<  7,      false },
    { "oBMB_05",         DEV_8L, Z_RH, h_oBMB0 <<  6,      false },
    { "oBMB_06",         DEV_8L, Z_RH, h_oBMB0 <<  5,      false },
    { "oBMB_07",         DEV_8L, Z_RH, h_oBMB0 <<  4,      false },
    { "oBMB_08",         DEV_8L, Z_RH, h_oBMB0 <<  3,      false },
    { "oBMB_09",         DEV_8L, Z_RH, h_oBMB0 <<  2,      false },
    { "oBMB_10",         DEV_8L, Z_RH, h_oBMB0 <<  1,      false },
    { "oBMB_11",         DEV_8L, Z_RH, h_oBMB0 <<  0,      false },
    { "oMA",             DEV_8L, Z_RI, i_oMA,              false },
    { "oMA_00",          DEV_8L, Z_RI, i_oMA0 << 11,       false },
    { "oMA_01",          DEV_8L, Z_RI, i_oMA0 << 10,       false },
    { "oMA_02",          DEV_8L, Z_RI, i_oMA0 <<  9,       false },
    { "oMA_03",          DEV_8L, Z_RI, i_oMA0 <<  8,       false },
    { "oMA_04",          DEV_8L, Z_RI, i_oMA0 <<  7,       false },
    { "oMA_05",          DEV_8L, Z_RI, i_oMA0 <<  6,       false },
    { "oMA_06",          DEV_8L, Z_RI, i_oMA0 <<  5,       false },
    { "oMA_07",          DEV_8L, Z_RI, i_oMA0 <<  4,       false },
    { "oMA_08",          DEV_8L, Z_RI, i_oMA0 <<  3,       false },
    { "oMA_09",          DEV_8L, Z_RI, i_oMA0 <<  2,       false },
    { "oMA_10",          DEV_8L, Z_RI, i_oMA0 <<  1,       false },
    { "oMA_11",          DEV_8L, Z_RI, i_oMA0 <<  0,       false },
    { "lbAC",            DEV_8L, Z_RI, i_lbAC,             false },
    { "lbMA",            DEV_8L, Z_RJ, j_lbMA,             false },
    { "lbMB",            DEV_8L, Z_RJ, j_lbMB,             false },
    { "majstate",        DEV_8L, Z_RK, k_majstate,         false },
    { "timedelay",       DEV_8L, Z_RK, k_timedelay,        false },
    { "timestate",       DEV_8L, Z_RK, k_timestate,        false },
    { "cyclectr",        DEV_8L, Z_RK, k_cyclectr,         false },
    { "nextmajst",       DEV_8L, Z_RK, k_nextmajst,        false },
    { "bDMABUS",         DEV_8L, Z_RO, o_bDMABUS,          false },
    { "hizmembus",       DEV_8L, Z_RO, o_hizmembus,        true  },
    { "r_BAC",           DEV_8L, Z_RO, o_r_BAC,            true  },
    { "r_BMB",           DEV_8L, Z_RO, o_r_BMB,            true  },
    { "r_MA",            DEV_8L, Z_RO, o_r_MA,             true  },
    { "x_DMAADDR",       DEV_8L, Z_RO, o_x_DMAADDR,        true  },
    { "x_DMADATA",       DEV_8L, Z_RO, o_x_DMADATA,        true  },
    { "x_INPUTBUS",      DEV_8L, Z_RO, o_x_INPUTBUS,       true  },
    { "x_MEM",           DEV_8L, Z_RO, o_x_MEM,            true  },
    { "bMEMBUS",         DEV_8L, Z_RP, p_bMEMBUS,          false },
    { "bPIOBUS",         DEV_8L, Z_RP, p_bPIOBUS,          false },
    { "ilaarmed",        DEV_8L, Z_RQ, q_ilaarmed,         true  },
    { "ilaafter",        DEV_8L, Z_RQ, q_ilaafter,         true  },
    { "ilaindex",        DEV_8L, Z_RQ, q_ilaindex,         true  },
    { "CM_ADDR",         DEV_CM, 1, CM_ADDR,               true  },
    { "CM_WRITE",        DEV_CM, 1, CM_WRITE,              true  },
    { "CM_DATA",         DEV_CM, 1, CM_DATA,               true  },
    { "CM_BUSY",         DEV_CM, 1, CM_BUSY,               false },
    { "CM_DONE",         DEV_CM, 1, CM_DONE,               false },
    { "CM_WCOVF",        DEV_CM, 1, CM_WCOVF,              false },
    { "CM_ENAB",         DEV_CM, 1, CM_ENAB,               true  },
    { "CM_NOBRK",        DEV_CM, 2, CM2_NOBRK,             true  },
    { "CM_CAINC",        DEV_CM, 2, CM2_CAINC,             true  },
    { "CM_3CYCL",        DEV_CM, 2, CM2_3CYCL,             true  },
    { "CM3",             DEV_CM, 3, CM3,                   true  },
    { "KB_FLAG",         DEV_TT, Z_TTYKB, KB_FLAG,         true  },
    { "KB_ENAB",         DEV_TT, Z_TTYKB, KB_ENAB,         true  },
    { "KB_CHAR",         DEV_TT, Z_TTYKB, 07777,           true  },
    { "PR_FLAG",         DEV_TT, Z_TTYPR, PR_FLAG,         true  },
    { "PR_FULL",         DEV_TT, Z_TTYPR, PR_FULL,         true  },
    { "PR_CHAR",         DEV_TT, Z_TTYPR, 07777,           true  },
    { "XM_OS8ZAP",       DEV_XM, 1, XM_OS8ZAP,             true  },
    { "XM_MWSTEP",       DEV_XM, 1, XM_MWSTEP,             true  },
    { "XM_MWHOLD",       DEV_XM, 1, XM_MWHOLD,             true  },
    { "XM_MRSTEP",       DEV_XM, 1, XM_MRSTEP,             true  },
    { "XM_MRHOLD",       DEV_XM, 1, XM_MRHOLD,             true  },
    { "XM_ENLO4K",       DEV_XM, 1, XM_ENLO4K,             true  },
    { "XM_ENABLE",       DEV_XM, 1, XM_ENABLE,             true  },
    { "XM_XMSTATE",      DEV_XM, 2, XM2_XMSTATE,           false },
    { "XM_SAVEDIFLD",    DEV_XM, 2, XM2_SAVEDIFLD,         false },
    { "XM_SAVEDDFLD",    DEV_XM, 2, XM2_SAVEDDFLD,         false },
    { "XM_IFLDAFJMP",    DEV_XM, 2, XM2_IFLDAFJMP,         false },
    { "XM_IFLD",         DEV_XM, 2, XM2_IFLD,              false },
    { "XM_DFLD",         DEV_XM, 2, XM2_DFLD,              false },
    { "XM_FIELD",        DEV_XM, 2, XM2_FIELD,             false },
    { "XM__MWDONE",      DEV_XM, 2, XM2__MWDONE,           false },
    { "XM__MRDONE",      DEV_XM, 2, XM2__MRDONE,           false },
    { "XM_OS8STEP",      DEV_XM, 3, XM3_OS8STEP,           false },
    { "ADDRLATCHWID",    DEV_XM, 4, XM4_ADDRLATCHWID,      true  },
    { "READSTROBEDEL",   DEV_XM, 4, XM4_READSTROBEDEL,     true  },
    { "READSTROBEWID",   DEV_XM, 4, XM4_READSTROBEWID,     true  },
    { "WRITEENABDEL",    DEV_XM, 4, XM4_WRITEENABDEL,      true  },
    { "WRITEENABWID",    DEV_XM, 5, XM5_WRITEENABWID,      true  },
    { "WRITEDONEWID",    DEV_XM, 5, XM5_WRITEDONEWID,      true  },
    { "XM_MEMCYCCTRLO",  DEV_XM, 6, XM6_MEMCYCCTRLO,       true  },
    { "XM_MEMCYCCTRHI",  DEV_XM, 7, XM7_MEMCYCCTRHI,       true  },
    { "SH_ACKNOWN",      DEV_SH, 1, SH_ACKNOWN,            false },
    { "SH_EAKNOWN",      DEV_SH, 1, SH_EAKNOWN,            false },
    { "SH_IRKNOWN",      DEV_SH, 1, SH_IRKNOWN,            false },
    { "SH_LNKNOWN",      DEV_SH, 1, SH_LNKNOWN,            false },
    { "SH_MAKNOWN",      DEV_SH, 1, SH_MAKNOWN,            false },
    { "SH_MBKNOWN",      DEV_SH, 1, SH_MBKNOWN,            false },
    { "SH_PCKNOWN",      DEV_SH, 1, SH_PCKNOWN,            false },
    { "SH_FRZONERR",     DEV_SH, 1, SH_FRZONERR,           true  },
    { "SH_CLEARIT",      DEV_SH, 1, SH_CLEARIT,            true  },
    { "SH_ERROR",        DEV_SH, 1, SH_ERROR,              true  },
    { "SH_MAJSTATE",     DEV_SH, 2, SH2_MAJSTATE,          false },
    { "SH_TIMESTATE",    DEV_SH, 2, SH2_TIMESTATE,         false },
    { "SH_IREG",         DEV_SH, 2, SH2_IREG,              false },
    { "SH_PCTR",         DEV_SH, 2, SH2_PCTR,              false },
    { "SH_TIMEDELAY",    DEV_SH, 3, SH3_TIMEDELAY,         false },
    { "SH_MBUF",         DEV_SH, 3, SH3_MBUF,              false },
    { "SH_MADR",         DEV_SH, 3, SH3_MADR,              false },
    { "SH_LINK",         DEV_SH, 4, SH4_LINK,              false },
    { "SH_ACUM",         DEV_SH, 4, SH4_ACUM,              false },
    { "SH_EADR",         DEV_SH, 4, SH4_EADR,              false },
    { "FP_CMDLO",        DEV_FP, 1, FP1_CMDLO,             true  },
    { "FP_CMDHI",        DEV_FP, 2, FP2_CMDHI,             true  },
    { "FP_STSLO",        DEV_FP, 3, FP3_STSLO,             false },
    { "FP_STSHI",        DEV_FP, 4, FP4_STSHI,             false },
    { "FP_STEPON",       DEV_FP, 5, FP5_STEPON,            true  },
    { "FP_CLEAR",        DEV_FP, 5, FP5_CLEAR,             true  },
    { "FP_MANUAL",       DEV_FP, 5, FP5_MANUAL,            true  },
    { "FP_I2CCOUNT",     DEV_FP, 5, FP5_I2CCOUNT,          false },
    { "FP_I2CSTATE",     DEV_FP, 5, FP5_I2CSTATE,          false },
    { "FP_I2CCLI",       DEV_FP, 5, FP5_I2CCLI,            false },
    { "FP_I2CDAI",       DEV_FP, 5, FP5_I2CDAI,            false },
    { "FP_I2CDAO",       DEV_FP, 5, FP5_I2CDAO,            true  },
    { "FP_I2CCLO",       DEV_FP, 5, FP5_I2CCLO,            true  },
    { "FP_LOCK",         DEV_FP, 6, FP6_LOCK,              true  },
    { "", 0, 0, 0, false }
};

static Z8LPage *z8p;
static uint32_t volatile *extmemptr;

int cmd_pin (ClientData clientdata, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    if ((objc == 2) && (strcasecmp (Tcl_GetString (objv[1]), "help") == 0)) {
        puts ("");
        puts ("  pin list - list all the pins");
        puts ("");
        puts ("  pin {get pin ...} | {set pin val ...} | {test pin ...} ...");
        puts ("    defaults to get");
        puts ("    get returns integer value");
        puts ("    test returns -1: undefined; 0: read-only; 1: read/write");
        puts ("");
        puts ("  'o' pins : simit=0 : read PDP-8/L output pins");
        puts ("             simit=1 : read simulator output pins");
        puts ("");
        puts ("  'i' pins : simit=0 : drives PDP-8/L inputs from device & arm register");
        puts ("             simit=1 : drives simulator inputs from device & arm register");
        puts ("            bareit=0 : logical OR of device and arm register");
        puts ("            bareit=1 : just value of arm register");
        puts ("");
        return TCL_OK;
    }

    if ((objc == 2) && (strcasecmp (Tcl_GetString (objv[1]), "list") == 0)) {
        for (PinDef const *pte = pindefs; pte->name[0] != 0; pte ++) {
            uint32_t volatile *ptr = devs[pte->dev];
            printf ("  %-16s  %c%c[%2u]  %08X\n",
                pte->name, (ptr[0] >> 24) & 0xFFU, (ptr[0] >> 16) & 0xFFU, pte->reg, pte->mask);
        }
        return TCL_OK;
    }

    if (z8p == NULL) {

        // access the zynq io page and find devices thereon
        z8p = new Z8LPage ();
        devs[DEV_8L] = z8p->findev ("8L", NULL, NULL, false);
        devs[DEV_CM] = z8p->findev ("CM", NULL, NULL, false);
        devs[DEV_TT] = z8p->findev ("TT", NULL, NULL, false);
        devs[DEV_XM] = z8p->findev ("XM", NULL, NULL, false);
        devs[DEV_SH] = z8p->findev ("SH", NULL, NULL, false);
        devs[DEV_FP] = z8p->findev ("FP", NULL, NULL, false);

        // get pointer to the 32K-word ram
        // maps each 12-bit word into low 12 bits of 32-bit word
        // upper 20 bits discarded on write, readback as zeroes
        extmemptr = z8p->extmem ();
    }

    bool setmode = false;
    bool testmode = false;
    int ngotvals = 0;
    Tcl_Obj *gotvals[objc];

    for (int i = 0; ++ i < objc;) {
        char const *name = Tcl_GetString (objv[i]);

        if (strcasecmp (name, "get") == 0) {
            setmode = false;
            testmode = false;
            continue;
        }
        if (strcasecmp (name, "set") == 0) {
            setmode = true;
            testmode = false;
            continue;
        }
        if (strcasecmp (name, "test") == 0) {
            setmode = false;
            testmode = true;
            continue;
        }

        bool writeable;
        int width;
        uint32_t mask;
        uint32_t volatile *ptr;

        // em:address
        if (strncasecmp (name, "em:", 3) == 0) {
            char *p;
            mask = strtoul (name + 3, &p, 0);
            if ((*p != 0) || (mask > 077777)) {
                if (! testmode) {
                    Tcl_SetResultF (interp, "extended memory address %s must be integer in range 000000..077777", name + 3);
                    return TCL_ERROR;
                }
                gotvals[ngotvals++] = Tcl_NewIntObj (-1);
                continue;
            }
            ptr = extmemptr + mask;
            mask = 07777;
            width = 12;
            writeable = true;
        }

        // signalname
        else {
            PinDef const *pte;
            for (pte = pindefs; pte->name[0] != 0; pte ++) {
                if (strcasecmp (pte->name, name) == 0) goto gotit;
            }
            if (! testmode) {
                Tcl_SetResultF (interp, "bad pin name %s", name);
                return TCL_ERROR;
            }
            gotvals[ngotvals++] = Tcl_NewIntObj (-1);
            continue;
        gotit:;
            mask = pte->mask;
            width = 32;
            if (mask != 0xFFFFFFFFU) {
                width = 0;
                while (1U << ++ width <= mask / (mask & - mask)) { }
            }
            ptr = devs[pte->dev] + pte->reg;
            writeable = pte->writ;
        }
        if (testmode) {
            gotvals[ngotvals++] = Tcl_NewIntObj (writeable);
            continue;
        }

        if (setmode) {
            if (! writeable) {
                Tcl_SetResultF (interp, "pin %s not settable", name);
                return TCL_ERROR;
            }
            if (++ i >= objc) {
                Tcl_SetResultF (interp, "missing pin value for set %s", name);
                return TCL_ERROR;
            }
            int val;
            int rc  = Tcl_GetIntFromObj (interp, objv[i], &val);
            if (rc != TCL_OK) return rc;
            if ((val < 0) || ((uint32_t) val >= 1ULL << width)) {
                Tcl_SetResultF (interp, "value 0%o too big for %s", val, name);
                return TCL_ERROR;
            }
            *ptr = (*ptr & ~ mask) | val * (mask & - mask);
        } else {
            uint32_t val = (*ptr & mask) / (mask & - mask);
            gotvals[ngotvals++] = Tcl_NewIntObj (val);
        }
    }

    if (ngotvals > 0) {
        if (ngotvals < 2) {
            Tcl_SetObjResult (interp, gotvals[0]);
        } else {
            Tcl_SetObjResult (interp, Tcl_NewListObj (ngotvals, gotvals));
        }
    }
    return TCL_OK;
}
