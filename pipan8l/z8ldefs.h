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
#define Z_N 12      // total number of registers

#define a_iBEMA         (1U <<  0)
#define a_iCA_INCREMENT (1U <<  1)
#define a_iDATA_IN      (1U <<  2)
#define a_iMEMINCR      (1U <<  3)
#define a_iMEM_P        (1U <<  4)
#define a_iTHREECYCLE   (1U <<  5)
#define a_i_AC_CLEAR    (1U <<  6)
#define a_i_BRK_RQST    (1U <<  7)
#define a_i_EA          (1U <<  8)
#define a_i_EMA         (1U <<  9)
#define a_i_INT_INHIBIT (1U << 10)
#define a_i_INT_RQST    (1U << 11)
#define a_i_IO_SKIP     (1U << 12)
#define a_i_MEMDONE     (1U << 13)
#define a_i_STROBE      (1U << 14)
#define a_softreset     (1U << 29)
#define a_softclock     (1U << 30)
#define a_softenab      (1U << 31)

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

#define c_iINPUTBUS (07777U <<  0)
#define c_iMEM      (07777U << 16)
#define d_i_DMAADDR (07777U <<  0)
#define d_i_DMADATA (07777U << 16)
#define e_swSR      (07777U <<  0)

#define f_oBIOP1        (1U <<  0)
#define f_oBIOP2        (1U <<  1)
#define f_oBIOP4        (1U <<  2)
#define f_oBTP2         (1U <<  3)
#define f_oBTP3         (1U <<  4)
#define f_oBTS_1        (1U <<  5)
#define f_oBTS_3        (1U <<  6)
#define f_oBUSINIT      (1U <<  7)
#define f_oBWC_OVERFLOW (1U <<  8)
#define f_oB_BREAK      (1U <<  9)
#define f_oE_SET_F_SET  (1U << 10)
#define f_oJMP_JMS      (1U << 11)
#define f_oLINE_LOW     (1U << 12)
#define f_oMEMSTART     (1U << 13)
#define f_o_ADDR_ACCEPT (1U << 14)
#define f_o_BF_ENABLE   (1U << 15)
#define f_o_BUSINIT     (1U << 16)
#define f_o_B_RUN       (1U << 17)
#define f_o_DF_ENABLE   (1U << 18)
#define f_o_KEY_CLEAR   (1U << 19)
#define f_o_KEY_DF      (1U << 20)
#define f_o_KEY_IF      (1U << 21)
#define f_o_KEY_LOAD    (1U << 22)
#define f_o_LOAD_SF     (1U << 23)
#define f_o_SP_CYC_NEXT (1U << 24)

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
#define g_lbIR          (7U << 25)

#define h_oBAC      (07777U <<  0)
#define h_oBMB      (07777U << 16)
#define i_oMA       (07777U <<  0)
#define i_lbAC      (07777U << 16)
#define j_lbMA      (07777U <<  0)
#define j_lbMB      (07777U << 16)

#define k_state     (   7U <<  0)
#define k_memodify  (   3U <<  3)
#define k_memstate  (   7U <<  5)
#define k_timedelay (   7U <<  8)
#define k_timestate (  15U << 11)
#define k_cyclectr  (1023U << 15)

#define c_iINPUTBUS0 (c_iINPUTBUS & - c_iINPUTBUS)
#define c_iMEM0      (c_iMEM      & - c_iMEM)
#define d_i_DMAADDR0 (d_i_DMAADDR & - d_i_DMAADDR)
#define d_i_DMADATA0 (d_i_DMADATA & - d_i_DMADATA)
#define e_swSR0      (e_swSR      & - e_swSR)
#define g_lbIR0      (g_lbIR      & - g_lbIR)
#define h_oBAC0      (h_oBAC      & - h_oBAC)
#define h_oBMB0      (h_oBMB      & - h_oBMB)
#define i_oMA0       (i_oMA       & - i_oMA)
#define i_lbAC0      (i_lbAC      & - i_lbAC)
#define j_lbMA0      (j_lbMA      & - j_lbMA)
#define j_lbMB0      (j_lbMB      & - j_lbMB)

#endif
