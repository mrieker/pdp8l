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

// bits provided by pdp8l.v in the zynq

#ifndef _Z8LDEFS
#define _Z8LDEFS

#define Z_VER 0     // pdp8l.v version number
#define Z_RA 1      // contain the various bits
#define Z_RB 2
#define Z_RC 3
#define Z_RD 4
#define Z_RE 5
#define Z_RF 6
#define Z_RG 7
#define Z_RH 8
#define Z_RI 9
#define Z_RJ 10
#define Z_RK 11
#define Z_RL 12
#define Z_RM 13
#define Z_RN 14     // memcycctr
#define Z_RO 15
#define Z_RP 16

#define Z_N 17      // total number of register

#define ZZ_RA (a_i_CA_INCRMNT | a_i_DATA_IN | a_i_MEM_P | a_i_EA | a_i_MEMDONE | a_i_STROBE | a_i_B36V1 | a_i_D36B2)
#define ZZ_RC (c_i_MEM)
#define ZZ_RD 0

#define a_iBEMA        (1U <<  0)
#define a_i_CA_INCRMNT (1U <<  1)
#define a_i_DATA_IN    (1U <<  2)
#define a_iMEMINCR     (1U <<  3)
#define a_i_MEM_P      (1U <<  4)
#define a_i3CYCLE      (1U <<  5)
#define a_iAC_CLEAR    (1U <<  6)
#define a_iBRK_RQST    (1U <<  7)
#define a_i_EA         (1U <<  8)
#define a_iEMA         (1U <<  9)
#define a_iINT_INHIBIT (1U << 10)
#define a_iINT_RQST    (1U << 11)
#define a_iIO_SKIP     (1U << 12)
#define a_i_MEMDONE    (1U << 13)
#define a_i_STROBE     (1U << 14)
#define a_i_B36V1      (1U << 15)
#define a_i_D36B2      (1U << 16)

#define b_swCONT        (1U <<  0)
#define b_swDEP         (1U <<  1)
#define b_swDFLD        (1U <<  2)
#define b_swEXAM        (1U <<  3)
#define b_swIFLD        (1U <<  4)
#define b_swLDAD        (1U <<  5)
#define b_swMPRT        (1U <<  6)
#define b_swSTEP        (1U <<  7)
#define b_swSTOP        (1U <<  8)
#define b_swSTART       (1U <<  9)
#define b_swSR      (07777U << 20)

#define c_iINPUTBUS (07777U <<  0)
#define c_i_MEM     (07777U << 16)
#define d_iDMAADDR  (07777U <<  0)
#define d_iDMADATA  (07777U << 16)

#define e_simit         (1U <<  0)  //rw 0=real PDP-8/L; 1=pdp8lsim.v
#define e_softreset     (1U <<  1)  //rw 0=normal; 1=power-on-reset (sim only)
#define e_nanocontin    (1U <<  2)  //rw 0=single step mode (sim only); 1=normal continuous running
#define e_nanotrigger   (1U <<  3)  //rw 0=ignored; 1=trigger single step (sim only)
#define e_nanocstep     (1U <<  4)  //ro 0=stopped; 1=stepping
#define e_brkwhenhltd   (1U <<  5)  //rw 0=ignore brk when halted; 1=process brk when halted (sim only)
#define e_bareit        (1U <<  6)  //rw 0=pass device data to PDP-8/L & sim 'i' pins; 1=pass arm reg data to PDP-8/L & sim 'i' pins

#define f_oBIOP1         (1U <<  0)
#define f_oBIOP2         (1U <<  1)
#define f_oBIOP4         (1U <<  2)
#define f_oBTP2          (1U <<  3)
#define f_oBTP3          (1U <<  4)
#define f_oBTS_1         (1U <<  5)
#define f_oBTS_3         (1U <<  6)
#define f_oC36B2         (1U <<  7)
#define f_o_BWC_OVERFLOW (1U <<  8)
#define f_o_B_BREAK      (1U <<  9)
#define f_oE_SET_F_SET   (1U << 10)
#define f_oJMP_JMS       (1U << 11)
#define f_oLINE_LOW      (1U << 12)
#define f_oMEMSTART      (1U << 13)
#define f_o_ADDR_ACCEPT  (1U << 14)
#define f_o_BF_ENABLE    (1U << 15)
#define f_o_BUSINIT      (1U << 16)
#define f_oB_RUN         (1U << 17)
#define f_o_DF_ENABLE    (1U << 18)
#define f_o_KEY_CLEAR    (1U << 19)
#define f_o_KEY_DF       (1U << 20)
#define f_o_KEY_IF       (1U << 21)
#define f_o_KEY_LOAD     (1U << 22)
#define f_o_LOAD_SF      (1U << 23)
#define f_o_SP_CYC_NEXT  (1U << 24)
#define f_oD35B2         (1U << 25)
#define f_didio          (1U << 26)

#define g_lbBRK         (1U <<  0)
#define g_lbCA          (1U <<  1)
#define g_lbDEF         (1U <<  2)
#define g_lbEA          (1U <<  3)
#define g_lbEXE         (1U <<  4)
#define g_lbFET         (1U <<  5)
#define g_lbION         (1U <<  6)
#define g_lbLINK        (1U <<  7)
#define g_lbRUN         (1U <<  8)
#define g_lbWC          (1U <<  9)
#define g_debounced     (1U << 10)
#define g_lastswLDAD    (1U << 11)
#define g_lastswSTART   (1U << 12)
#define g_simmemen      (1U << 13)
#define g_lbIR          (7U << 25)

#define h_oBAC      (07777U <<  0)
#define h_oBMB      (07777U << 16)
#define i_oMA       (07777U <<  0)
#define i_lbAC      (07777U << 16)
#define j_lbMA      (07777U <<  0)
#define j_lbMB      (07777U << 16)

#define k_majstate  (  017U <<  0)
#define k_timedelay (  077U <<  4)
#define k_timestate (  037U << 10)
#define k_cyclectr  (01777U << 15)
#define k_nextmajst (  017U << 25)

#define l_xbraddr  (077777U <<  0)
#define l_xbrwena  (     1U << 16)
#define l_xbrenab  (     1U << 20)
#define l_meminprog (01777U << 22)
#define m_xbrrdat  ( 07777U <<  0)
#define m_xbrwdat  ( 07777U << 16)

#define b_swSR0       (b_swSR       & - b_swSR)
#define c_iINPUTBUS0  (c_iINPUTBUS  & - c_iINPUTBUS)
#define c_i_MEM0      (c_i_MEM      & - c_i_MEM)
#define d_iDMAADDR0   (d_iDMAADDR   & - d_iDMAADDR)
#define d_iDMADATA0   (d_iDMADATA   & - d_iDMADATA)
#define g_lbIR0       (g_lbIR       & - g_lbIR)
#define h_oBAC0       (h_oBAC       & - h_oBAC)
#define h_oBMB0       (h_oBMB       & - h_oBMB)
#define i_oMA0        (i_oMA        & - i_oMA)
#define i_lbAC0       (i_lbAC       & - i_lbAC)
#define j_lbMA0       (j_lbMA       & - j_lbMA)
#define j_lbMB0       (j_lbMB       & - j_lbMB)
#define k_majstate0   (k_majstate   & - k_majstate)
#define k_timedelay0  (k_timedelay  & - k_timedelay)
#define k_timestate0  (k_timestate  & - k_timestate)
#define l_xbraddr0    (l_xbraddr    & - l_xbraddr)
#define l_xbrwena0    (l_xbrwena    & - l_xbrwena)
#define l_xbrenab0    (l_xbrenab    & - l_xbrenab)
#define l_meminprog0  (l_meminprog  & - l_meminprog)
#define m_xbrrdat0    (m_xbrrdat    & - m_xbrrdat)
#define m_xbrwdat0    (m_xbrwdat    & - m_xbrwdat)

#define o_bDMABUS (07777U << 20)
#define o_bDMABUSA (1U << 31)
#define o_bDMABUSB (1U << 30)
#define o_bDMABUSC (1U << 29)
#define o_bDMABUSD (1U << 28)
#define o_bDMABUSE (1U << 27)
#define o_bDMABUSF (1U << 26)
#define o_bDMABUSH (1U << 25)
#define o_bDMABUSJ (1U << 24)
#define o_bDMABUSK (1U << 23)
#define o_bDMABUSL (1U << 22)
#define o_bDMABUSM (1U << 21)
#define o_bDMABUSN (1U << 20)
#define o_hizmembus  (1U << 7)
#define o_r_BAC      (1U << 6)
#define o_r_BMB      (1U << 5)
#define o_r_MA       (1U << 4)
#define o_x_DMAADDR  (1U << 3)
#define o_x_DMADATA  (1U << 2)
#define o_x_INPUTBUS (1U << 1)
#define o_x_MEM      (1U << 0)

#define p_bMEMBUS (07777U << 20)
#define p_bMEMBUSA (1U << 31)
#define p_bMEMBUSB (1U << 30)
#define p_bMEMBUSC (1U << 29)
#define p_bMEMBUSD (1U << 28)
#define p_bMEMBUSE (1U << 27)
#define p_bMEMBUSF (1U << 26)
#define p_bMEMBUSH (1U << 25)
#define p_bMEMBUSJ (1U << 24)
#define p_bMEMBUSK (1U << 23)
#define p_bMEMBUSL (1U << 22)
#define p_bMEMBUSM (1U << 21)
#define p_bMEMBUSN (1U << 20)
#define p_bPIOBUS (07777U <<  8)
#define p_bPIOBUSA (1U << 19)
#define p_bPIOBUSB (1U << 18)
#define p_bPIOBUSC (1U << 17)
#define p_bPIOBUSD (1U << 16)
#define p_bPIOBUSE (1U << 15)
#define p_bPIOBUSF (1U << 14)
#define p_bPIOBUSH (1U << 13)
#define p_bPIOBUSJ (1U << 12)
#define p_bPIOBUSK (1U << 11)
#define p_bPIOBUSL (1U << 10)
#define p_bPIOBUSM (1U <<  9)
#define p_bPIOBUSN (1U <<  8)

#define MS_HALT   0     // figure out what to do next (also for exam & ldad switches)
#define MS_FETCH  1     // memory cycle is fetching instruction
#define MS_DEFER  2     // memory cycle is reading pointer
#define MS_EXEC   3     // memory cycle is for executing instruction
#define MS_WC     4     // memory cycle is for incrementing dma word count
#define MS_CA     5     // memory cycle is for reading dma address
#define MS_BRK    6     // memory cycle is for dma data word transfer
#define MS_INTAK  7     // memory cycle is for interrupt acknowledge
#define MS_DEPOS  8     // memory cycle is for deposit switch
#define MS_EXAM   9     // memory cycle is for examine switch
#define MS_BRKWH 10     // memory cycle is for break when halted

#define MS_NAMES "HALT","FETCH","DEFER","EXEC","WC","CA","BRK","INTAK","DEPOS","EXAM","BRKWH","???11","???12","???13","???14","???15"

#define TS_TSIDLE   0     // figure out what to do next, does console switch processing if not running
#define TS_TS1BODY  1     // tell memory to start reading location addressed by MA
#define TS_TP1BEG   2
#define TS_TP1END   3
#define TS_TS2BODY  4     // get contents of memory into MB and modify according to majstate S_...
#define TS_TP2BEG   5
#define TS_TP2END   6
#define TS_TS3BODY  7     // write contents of MB back to memory
#define TS_TP3BEG   8
#define TS_TP3END   9
#define TS_BEGIOP1 10
#define TS_DOIOP1  11     // maybe output IOP1
#define TS_BEGIOP2 12
#define TS_DOIOP2  13     // maybe output IOP2
#define TS_BEGIOP4 14
#define TS_DOIOP4  15     // maybe output IOP4
#define TS_TS4BODY 16     // finish up instruction (modify ac, link, pc, etc)
#define TS_TP4BEG  17
#define TS_TP4END  18

#define TS_NAMES \
    "TSIDLE",   "TS1BODY",  "TP1BEG",   "TP1END",   "TS2BODY",  "TP2BEG",   "TP2END",   "TS3BODY",  \
    "TP3BEG",   "TP3END",   "BEGIOP1",  "DOIOP1",   "BEGIOP2",  "DOIOP2",   "BEGIOP4",  "DOIOP4",   \
    "TS4BODY",  "TP4BEG",   "TP4END",   "???19",    "???20",    "???21",    "???22",    "???23",    \
    "???24",    "???25",    "???26",    "???27",    "???28",    "???29",    "???30",    "???31"

// pdp8lcmem.v registers
#define CM_ADDR  (077777U << 0)
#define CM_WRITE (1U << 15)
#define CM_DATA  (07777U << 16)
#define CM_BUSY  (1U << 28)
#define CM_DONE  (1U << 29)
#define CM_WCOVF (1U << 30)
#define CM_ENAB  (1U << 31)
#define CM_ADDR0 (1U <<  0)
#define CM_DATA0 (1U << 16)

#define CM2_CAINC (1U << 30)
#define CM2_3CYCL (1U << 31)

#define CM3 (0xFFFFFFFFU)

// pdp8ltty.v registers
#define Z_TTYVER 0
#define Z_TTYKB 1
#define Z_TTYPR 2
#define Z_TTYPN 3

#define KB_FLAG 0x80000000
#define KB_ENAB 0x40000000
#define PR_FLAG 0x80000000
#define PR_FULL 0x40000000

// pdp8lxmem.v registers
#define XM_OS8ZAP (1U <<  0)
#define XM_MWSTEP (1U <<  1)
#define XM_MWHOLD (1U <<  2)
#define XM_MRSTEP (1U <<  3)
#define XM_MRHOLD (1U <<  4)
#define XM_ENLO4K (1U << 30)
#define XM_ENABLE (1U << 31)

#define XM2_XMSTATE   (077U <<  0)
#define XM2_XMMEMENAB   (1U <<  6)
#define XM2_SAVEDIFLD   (7U <<  8)
#define XM2_SAVEDDFLD   (7U << 11)
#define XM2_IFLDAFJMP   (7U << 14)
#define XM2_IFLD        (7U << 17)
#define XM2_DFLD        (7U << 20)
#define XM2_FIELD       (7U << 27)
#define XM2__MWDONE     (1U << 30)
#define XM2__MRDONE     (1U << 31)
#define XM2_XMSTATE0    (1U <<  0)
#define XM2_SAVEDIFLD0  (1U <<  8)
#define XM2_SAVEDDFLD0  (1U << 11)
#define XM2_IFLDAFJMP0  (1U << 14)
#define XM2_IFLD0       (1U << 17)
#define XM2_DFLD0       (1U << 20)
#define XM2_BUSYONARM0  (1U << 24)
#define XM2_FIELD0      (1U << 27)

#define XM3_OS8STEP     (3U <<  0)
#define XM3_OS8STEP0    (1U <<  0)

// pdp8lvc8.v registers
#define VC1_REMOVE   (077777U << 17)
#define VC1_INSERT   (077777U <<  0)
#define VC2_TYPEE         (1U << 31)
#define VC2_INTENS        (3U << 28)
#define VC2_EFLAGS    (07777U << 16)
#define VC3_DEQUEUE       (1U << 31)
#define VC3_READADDR (077777U <<  0)
#define VC4_READBUSY      (3U << 30)
#define VC4_EMPTY         (1U << 29)
#define VC4_TCOORD        (3U << 20)
#define VC4_YCOORD    (01777U << 10)
#define VC4_XCOORD    (01777U <<  0)

#define VC1_REMOVE0       (1U << 17)
#define VC1_INSERT0       (1U <<  0)
#define VC2_INTENS0       (1U << 28)
#define VC2_EFLAGS0       (1U << 16)
#define VC3_READADDR0     (1U <<  0)
#define VC4_READBUSY0     (1U << 30)
#define VC4_TCOORD0       (1U << 20)
#define VC4_YCOORD0       (1U << 10)
#define VC4_XCOORD0       (1U <<  0)

#endif
