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

// Runs PIPan8L on a Zynq board programmed with 8/L emulation code

#include <fcntl.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include "z8ldefs.h"

#define ABORT() do { fprintf (stderr, "abort() %s:%d\n", __FILE__, __LINE__); abort (); } while (0)
#define ASSERT(cond) do { if (__builtin_constant_p (cond)) { if (!(cond)) asm volatile ("assert failure line %c0" :: "i"(__LINE__)); } else { if (!(cond)) ABORT (); } } while (0)

#define ESC_NORMV "\033[m"             /* go back to normal video */
#define ESC_REVER "\033[7m"            /* turn reverse video on */
#define ESC_UNDER "\033[4m"            /* turn underlining on */
#define ESC_BLINK "\033[5m"            /* turn blink on */
#define ESC_BOLDV "\033[1m"            /* turn bold on */
#define ESC_REDBG "\033[41m"           /* red background */
#define ESC_YELBG "\033[44m"           /* yellow background */
#define ESC_EREOL "\033[K"             /* erase to end of line */
#define ESC_EREOP "\033[J"             /* erase to end of page */
#define ESC_EREOL "\033[K"          // erase to end of line
#define ESC_EREOP "\033[J"          // erase to end of page
#define ESC_HOMEC "\033[H"          // home cursor

#define EOL ESC_EREOL "\n"
#define EOP ESC_EREOP

static char const *const memodifynames[4] = { "NONE", "DEPOS", "STATE", "???3" };
static char const *const memstatenames[8] = { "IDLE", "START", "LOCAL", "EXTERN", "STARTIO", "DOIO", "FINISH", "???7" };
static char const *const statenames[8] = { "START", "FETCH", "DEFER", "EXEC", "WC", "CA", "BRK", "INTAK" };
static char const *const timestatenames[16] = { "IDLE", "TS1BODY", "TP1BEG", "TP1END", "TS2BODY", "TP2BEG", "TP2END", "TS3BODY", "TP3BEG", "TP3END", "TS4BODY", "TP4BEG", "TP4END", "???13", "???14", "???15" };

#define FIELD(index,mask) ((z8ls[index] & mask) / (mask & - mask))

int main ()
{
    int zynqfd = open ("/proc/zynqgpio", O_RDWR);
    if (zynqfd < 0) {
        fprintf (stderr, "Z8LLib::openpads: error opening /proc/zynqgpio: %m\n");
        ABORT ();
    }

    void *zynqptr = mmap (NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, zynqfd, 0);
    if (zynqptr == MAP_FAILED) {
        fprintf (stderr, "Z8LLib::openpads: error mmapping /proc/zynqgpio: %m\n");
        ABORT ();
    }

    uint32_t volatile *zynqpage = (uint32_t volatile *) zynqptr;
    uint32_t ver = zynqpage[Z_VER];
    fprintf (stderr, "Z8LLib::openpads: zynq version %08X\n", ver);
    if ((ver & 0xFFFF0000U) != (('8' << 24) | ('L' << 16))) {
        fprintf (stderr, "Z8LLib::openpads: bad magic number\n");
        ABORT ();
    }

    while (true) {
        usleep (1000);

        uint32_t z8ls[Z_N];
        for (int i = 0; i < Z_N; i ++) {
            z8ls[i] = zynqpage[i];
        }

        printf (ESC_HOMEC);
        printf ("  iBEMA=%o", FIELD (Z_RA, a_iBEMA));
        printf ("  iCA_INCREMENT=%o", FIELD (Z_RA, a_iCA_INCREMENT));
        printf ("  iDATA_IN=%o", FIELD (Z_RA, a_iDATA_IN));
        printf ("  iMEMINCR=%o", FIELD (Z_RA, a_iMEMINCR));
        printf ("  iMEM_P=%o", FIELD (Z_RA, a_iMEM_P));
        printf ("  iTHREECYCLE=%o", FIELD (Z_RA, a_iTHREECYCLE));
        printf ("  i_AC_CLEAR=%o", FIELD (Z_RA, a_i_AC_CLEAR));
        printf ("  i_BRK_RQST=%o", FIELD (Z_RA, a_i_BRK_RQST));
        printf ("  i_EA=%o", FIELD (Z_RA, a_i_EA));
        printf ("  i_EMA=%o", FIELD (Z_RA, a_i_EMA));
        printf ("  i_INT_INHIBIT=%o", FIELD (Z_RA, a_i_INT_INHIBIT));
        printf ("  i_INT_RQST=%o", FIELD (Z_RA, a_i_INT_RQST));
        printf ("  i_IO_SKIP=%o", FIELD (Z_RA, a_i_IO_SKIP));
        printf ("  i_MEMDONE=%o", FIELD (Z_RA, a_i_MEMDONE));
        printf ("  i_STROBE=%o", FIELD (Z_RA, a_i_STROBE));
        printf ("  softreset=%o", FIELD (Z_RA, a_softreset));
        printf ("  softclock=%o", FIELD (Z_RA, a_softclock));
        printf ("  softenab=%o", FIELD (Z_RA, a_softenab));

        printf ("  swCONT=%o", FIELD (Z_RB, b_swCONT));
        printf ("  swDEP=%o", FIELD (Z_RB, b_swDEP));
        printf ("  swDFLD=%o", FIELD (Z_RB, b_swDFLD));
        printf ("  swEXAM=%o", FIELD (Z_RB, b_swEXAM));
        printf ("  swIFLD=%o", FIELD (Z_RB, b_swIFLD));
        printf ("  swLDAD=%o", FIELD (Z_RB, b_swLDAD));
        printf ("  swMPRT=%o", FIELD (Z_RB, b_swMPRT));
        printf ("  swSTEP=%o", FIELD (Z_RB, b_swSTEP));
        printf ("  swSTOP=%o", FIELD (Z_RB, b_swSTOP));
        printf ("  swSTART=%o", FIELD (Z_RB, b_swSTART));

        printf ("  iINPUTBUS=%04o", FIELD (Z_RC, c_iINPUTBUS));
        printf ("  iMEM=%04o", FIELD (Z_RC, c_iMEM));
        printf ("  i_DMAADDR=%04o", FIELD (Z_RD, d_i_DMAADDR));
        printf ("  i_DMADATA=%04o", FIELD (Z_RD, d_i_DMADATA));
        printf ("  swSR=%04o", FIELD (Z_RE, e_swSR));

        printf ("  oBIOP1=%o", FIELD (Z_RF, f_oBIOP1));
        printf ("  oBIOP2=%o", FIELD (Z_RF, f_oBIOP2));
        printf ("  oBIOP4=%o", FIELD (Z_RF, f_oBIOP4));
        printf ("  oBTP2=%o", FIELD (Z_RF, f_oBTP2));
        printf ("  oBTP3=%o", FIELD (Z_RF, f_oBTP3));
        printf ("  oBTS_1=%o", FIELD (Z_RF, f_oBTS_1));
        printf ("  oBTS_3=%o", FIELD (Z_RF, f_oBTS_3));
        printf ("  oBUSINIT=%o", FIELD (Z_RF, f_oBUSINIT));
        printf ("  oBWC_OVERFLOW=%o", FIELD (Z_RF, f_oBWC_OVERFLOW));
        printf ("  oB_BREAK=%o", FIELD (Z_RF, f_oB_BREAK));
        printf ("  oE_SET_F_SET=%o", FIELD (Z_RF, f_oE_SET_F_SET));
        printf ("  oJMP_JMS=%o", FIELD (Z_RF, f_oJMP_JMS));
        printf ("  oLINE_LOW=%o", FIELD (Z_RF, f_oLINE_LOW));
        printf ("  oMEMSTART=%o", FIELD (Z_RF, f_oMEMSTART));
        printf ("  o_ADDR_ACCEPT=%o", FIELD (Z_RF, f_o_ADDR_ACCEPT));
        printf ("  o_BF_ENABLE=%o", FIELD (Z_RF, f_o_BF_ENABLE));
        printf ("  o_BUSINIT=%o", FIELD (Z_RF, f_o_BUSINIT));
        printf ("  o_B_RUN=%o", FIELD (Z_RF, f_o_B_RUN));
        printf ("  o_DF_ENABLE=%o", FIELD (Z_RF, f_o_DF_ENABLE));
        printf ("  o_KEY_CLEAR=%o", FIELD (Z_RF, f_o_KEY_CLEAR));
        printf ("  o_KEY_DF=%o", FIELD (Z_RF, f_o_KEY_DF));
        printf ("  o_KEY_IF=%o", FIELD (Z_RF, f_o_KEY_IF));
        printf ("  o_KEY_LOAD=%o", FIELD (Z_RF, f_o_KEY_LOAD));
        printf ("  o_LOAD_SF=%o", FIELD (Z_RF, f_o_LOAD_SF));
        printf ("  o_SP_CYC_NEXT=%o", FIELD (Z_RF, f_o_SP_CYC_NEXT));

        printf ("  lbBRK=%o", FIELD (Z_RG, g_lbBRK));
        printf ("  lbCA=%o", FIELD (Z_RG, g_lbCA));
        printf ("  lbDEF=%o", FIELD (Z_RG, g_lbDEF));
        printf ("  lbEA=%o", FIELD (Z_RG, g_lbEA));
        printf ("  lbEXE=%o", FIELD (Z_RG, g_lbEXE));
        printf ("  lbFET=%o", FIELD (Z_RG, g_lbFET));
        printf ("  lbION=%o", FIELD (Z_RG, g_lbION));
        printf ("  lbLINK=%o", FIELD (Z_RG, g_lbLINK));
        printf ("  lbRUN=%o", FIELD (Z_RG, g_lbRUN));
        printf ("  lbWC=%o", FIELD (Z_RG, g_lbWC));
        printf ("  lbIR=%o", FIELD (Z_RG, g_lbIR));

        printf ("  oBAC=%04o", FIELD (Z_RH, h_oBAC));
        printf ("  oBMB=%04o", FIELD (Z_RH, h_oBMB));
        printf ("  oMA=%04o", FIELD (Z_RI, i_oMA));
        printf ("  lbAC=%04o", FIELD (Z_RI, i_lbAC));
        printf ("  lbMA=%04o", FIELD (Z_RJ, j_lbMA));
        printf ("  lbMB=%04o", FIELD (Z_RJ, j_lbMB));

        printf ("  state=%-5s", statenames[FIELD(Z_RK,k_state)]);
        printf ("  memodify=%-5s", memodifynames[FIELD(Z_RK,k_memodify)]);
        printf ("  memstate=%-7s", memstatenames[FIELD(Z_RK,k_memstate)]);
        printf ("  timedelay=%u", FIELD (Z_RK, k_timedelay));
        printf ("  timestate=%-7s", timestatenames[FIELD(Z_RK,k_timestate)]);
        printf ("  cyclectr=%05u", FIELD (Z_RK, k_cyclectr));

        printf (EOL EOP);
    }
}
