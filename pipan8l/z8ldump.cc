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

//  ./z8ldump.armv7l [-nano]
//    -nano - press enter to do one fpga cycle at a time

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "z8ldefs.h"
#include "z8lutil.h"

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

static char const *const majstatenames[8] =
    { "START", "FETCH", "DEFER", "EXEC", "WC", "CA", "BRK", "DEPOS" };
static char const *const timestatenames[32] =
    { "IDLE", "TS1INIT", "TS1BODY", "TP1BEG", "TP1END", "TS2BODY", "TP2BEG", "TP2END", "TS3BODY",
        "TP3BEG", "TP3END", "BEGIOP1", "DOIOP1", "BEGIOP2", "DOIOP2", "BEGIOP4", "DOIOP4",
        "TS4BODY", "TP4BEG", "TP4END", "???20", "???21", "???22", "???23",
        "???24", "???25", "???26", "???27", "???28", "???29", "???30", "???31" };

#define FIELD(index,mask) ((z8ls[index] & mask) / (mask & - mask))

int main (int argc, char **argv)
{
    bool rawflag = false;
    bool nanocycle = false;
    for (int i = 0; ++ i < argc;) {
        if (strcasecmp (argv[i], "-nano") == 0) {
            nanocycle = true;
            continue;
        }
        if (strcasecmp (argv[i], "-raw") == 0) {
            rawflag = true;
            continue;
        }
        fprintf (stderr, "unknown argument %s\n", argv[i]);
        return 1;
    }

    Z8LPage z8p;
    uint32_t volatile *zynqpage = z8p.findev (NULL, NULL, NULL, false);
    uint32_t ver = zynqpage[Z_VER];
    fprintf (stderr, "Z8LLib::openpads: zynq version %08X\n", ver);
    if ((ver & 0xFFFF0000U) != (('8' << 24) | ('L' << 16))) {
        fprintf (stderr, "Z8LLib::openpads: bad magic number\n");
        ABORT ();
    }

    // make sure nanostep (the clocking pulse) is clear
    // set nanocycle (enables clocking via nanostep) only if -nano option given
    uint32_t rega = zynqpage[Z_RA];
    zynqpage[Z_RA] = (rega & ~ a_nanocycle & ~ a_nanostep) | (nanocycle ? a_nanocycle : 0);

    while (true) {
        usleep (1000);

        uint32_t z8ls[1024];
        for (int i = 0; i < 1024; i ++) {
            z8ls[i] = zynqpage[i];
        }

        printf (ESC_HOMEC);

        if (rawflag) {
            for (int i = 0; i < 1024; i += 16) {
                printf ("%03X:", i);
                for (int j = 0; j < 16; j ++) {
                    printf (" %08X", zynqpage[i+j]);
                }
                printf (EOL);
            }
        } else {

            printf ("VERSION=%08X", z8ls[Z_VER]);
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
            printf ("  brkwhenhltd=%o", FIELD (Z_RA, a_brkwhenhltd));
            printf ("  simit=%o", FIELD (Z_RA, a_simit));
            printf ("  softreset=%o", FIELD (Z_RA, a_softreset));
            printf ("  nanostep=%o", FIELD (Z_RA, a_nanostep));
            printf ("  nanocycle=%o", FIELD (Z_RA, a_nanocycle));

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
            printf ("  debounced=%o", FIELD (Z_RG, (1U << 10)));
            printf ("  lastswLDAD=%o", FIELD (Z_RG, (1U << 11)));
            printf ("  lastswSTART=%o", FIELD (Z_RG, (1U << 12)));

            printf ("  oBAC=%04o", FIELD (Z_RH, h_oBAC));
            printf ("  oBMB=%04o", FIELD (Z_RH, h_oBMB));
            printf ("  oMA=%04o", FIELD (Z_RI, i_oMA));
            printf ("  lbAC=%04o", FIELD (Z_RI, i_lbAC));
            printf ("  lbMA=%04o", FIELD (Z_RJ, j_lbMA));
            printf ("  lbMB=%04o", FIELD (Z_RJ, j_lbMB));

            printf ("  majstate=%-5s", majstatenames[FIELD(Z_RK,k_majstate)]);
            printf ("  timedelay=%02u", FIELD (Z_RK, k_timedelay));
            printf ("  timestate=%-7s", timestatenames[FIELD(Z_RK,k_timestate)]);
            printf ("  cyclectr=%04u" EOL, FIELD (Z_RK, k_cyclectr));

            for (int i = 0; i < 1024;) {
                uint32_t idver = z8ls[i];
                if (((idver >> 24) == 'X') && (((idver >> 16) & 255) == 'M')) {
                    printf (EOL "VERSION=%08X XM  enlo4k=%o  enable=%o  ifld=%o  dfld=%o  field=%o  memdelay=%3u  _mwdone=%o  _mrdone=%o" EOL,
                        idver, FIELD (i+1,XM_ENLO4K), FIELD (i+1,XM_ENABLE), FIELD (i+2,XM2_IFLD), FIELD (i+2,XM2_DFLD), FIELD (i+2,XM2_FIELD),
                        FIELD (i+2,XM2_MEMDELAY), FIELD (i+2,XM2__MWDONE), FIELD (i+2,XM2__MRDONE));
                } else {
                    if ((idver & 0xF000U) == 0x1000U) {
                        printf (EOL "VERSION=%08X %c%c %08X %08X %08X" EOL, idver, idver >> 24, idver >> 16, z8ls[i+1], z8ls[i+2], z8ls[i+3]);
                    }
                    if ((idver & 0xF000U) == 0x2000U) {
                        printf (EOL "VERSION=%08X %c%c %08X %08X %08X %08X %08X %08X %08X" EOL, idver, idver >> 24, idver >> 16,
                            z8ls[i+1], z8ls[i+2], z8ls[i+3], z8ls[i+4], z8ls[i+5], z8ls[i+6], z8ls[i+7]);
                    }
                }
                i += 2 << ((idver >> 12) & 15);
            }
        }

        printf (EOP);

        if (nanocycle) {
            char temp[8];
            printf ("\n > ");
            fflush (stdout);
            if (fgets (temp, sizeof temp, stdin) == NULL) break;
            uint32_t startcount = zynqpage[Z_RK] & k_cyclectr;
            zynqpage[Z_RA] &= ~ a_nanostep;
            zynqpage[Z_RA] |=   a_nanostep;
            for (int i = 0; (zynqpage[Z_RK] & k_cyclectr) == startcount; i ++) {
                if (i > 1000) {
                    fprintf (stderr, "timed out waiting for cycle count increment\n");
                    break;
                }
            }
            zynqpage[Z_RA] &= ~ a_nanostep;
        }
    }
    printf ("\n");
    return 0;
}
