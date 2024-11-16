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
// It does not write to the zynq memory pages so does not alter zynq state

//  ./z8ldump.armv7l

#include <signal.h>
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

#define FIELD(index,mask) ((z8ls[index] & mask) / (mask & - mask))
#define BUS12(index,topbit) ((z8ls[index] / (topbit / 2048)) & 4095)

static char const *const majstatenames[] = { MS_NAMES };
static char const *const timestatenames[] = { TS_NAMES };

static bool volatile exitflag;
static char stdoutbuf[8000];

static void siginthand (int signum)
{
    exitflag = true;
}

int main (int argc, char **argv)
{
    setvbuf (stdout, stdoutbuf, _IOFBF, sizeof stdoutbuf);

    bool stepmode = false;
    if ((argc == 2) && (strcasecmp (argv[1], "-step") == 0)) {
        stepmode = true;
    } else if (argc > 1) {
        fprintf (stderr, "unknown argument %s\n", argv[1]);
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

    if (stepmode) {
        zynqpage[Z_RE] &= ~ (e_nanocontin | e_nanotrigger);
    } else {
        signal (SIGINT, siginthand);
    }

    while (! exitflag) {
        usleep (1000);

        uint32_t z8ls[1024];
        for (int i = 0; i < 1024; i ++) {
            z8ls[i] = zynqpage[i];
        }

        printf (ESC_HOMEC "VERSION=%08X 8L" EOL, z8ls[0]);

        printf ("  oBIOP1=%o             oBAC=%04o           brkwhenhltd=%o    ",   FIELD(Z_RF,f_oBIOP1),         FIELD(Z_RH,h_oBAC),           FIELD(Z_RE,e_brkwhenhltd));
        printf ("  iBEMA=%o              i_DMAADDR=%04o      majstate=%s" EOL,      FIELD(Z_RA,a_iBEMA),          FIELD(Z_RD,d_i_DMAADDR),  majstatenames[FIELD(Z_RK,k_majstate)]);

        printf ("  oBIOP2=%o             oBMB=%04o           nanocstep=%o      ",   FIELD(Z_RF,f_oBIOP2),         FIELD(Z_RH,h_oBMB),           FIELD(Z_RE,e_nanocstep));
        printf ("  iCA_INCREMENT=%o      i_DMADATA=%04o      nextmajst=%s" EOL,     FIELD(Z_RA,a_iCA_INCREMENT),  FIELD(Z_RD,d_i_DMADATA),  majstatenames[FIELD(Z_RK,k_nextmajst)]);

        printf ("  oBIOP4=%o             oMA=%04o            nanotrigger=%o    ",   FIELD(Z_RF,f_oBIOP4),         FIELD(Z_RI,i_oMA),            FIELD(Z_RE,e_nanotrigger));
        printf ("  iDATA_IN=%o           iINPUTBUS=%04o      timedelay=%o0" EOL,    FIELD(Z_RA,a_iDATA_IN),       FIELD(Z_RC,c_iINPUTBUS),  FIELD(Z_RK,k_timedelay));

        printf ("  oBTP2=%o                                  nanocontin=%o     ",   FIELD(Z_RF,f_oBTP2),                                        FIELD(Z_RE,e_nanocontin));
        printf ("  iMEMINCR=%o           iMEM=%04o           timestate=%s" EOL,     FIELD(Z_RA,a_iMEMINCR),       FIELD(Z_RC,c_iMEM),       timestatenames[FIELD(Z_RK,k_timestate)]);

        printf ("  oBTP3=%o              lbBRK=%o             softreset=%o      ",  FIELD(Z_RF,f_oBTP3),          FIELD(Z_RG,g_lbBRK),          FIELD(Z_RE,e_softreset));
        printf ("  iMEM_P=%o             swCONT=%o            cyclectr=%04o" EOL,   FIELD(Z_RA,a_iMEM_P),         FIELD(Z_RB,b_swCONT),     FIELD(Z_RK,k_cyclectr));

        printf ("  oBTS_1=%o             lbCA=%o              simit=%o          ",  FIELD(Z_RF,f_oBTS_1),         FIELD(Z_RG,g_lbCA),           FIELD(Z_RE,e_simit));
        printf ("  iTHREECYCLE=%o        swDEP=%o             memcycctr=%08X" EOL,  FIELD(Z_RA,a_iTHREECYCLE),    FIELD(Z_RB,b_swDEP),      z8ls[Z_RN]);

        printf ("  oBTS_3=%o             lbDEF=%o             bareit=%o         ",  FIELD(Z_RF,f_oBTS_3),         FIELD(Z_RG,g_lbDEF),          FIELD(Z_RE,e_bareit));
        printf ("  i_AC_CLEAR=%o         swDFLD=%o" EOL,                            FIELD(Z_RA,a_i_AC_CLEAR),     FIELD(Z_RB,b_swDFLD));

        printf ("  oBWC_OVERFLOW=%o      lbEA=%o                               ",   FIELD(Z_RF,f_oBWC_OVERFLOW),  FIELD(Z_RG,g_lbEA));
        printf ("  i_BRK_RQST=%o         swEXAM=%o            debounced=%o" EOL,    FIELD(Z_RA,a_i_BRK_RQST),     FIELD(Z_RB,b_swEXAM),     FIELD(Z_RG,g_debounced));

        printf ("  oB_BREAK=%o           lbEXE=%o             bDMABUS=%04o     ",   FIELD(Z_RF,f_oB_BREAK),       FIELD(Z_RG,g_lbEXE),          FIELD(Z_RO,o_bDMABUS));
        printf ("  i_EA=%o               swIFLD=%o            lastswLDAD=%o" EOL,   FIELD(Z_RA,a_i_EA),           FIELD(Z_RB,b_swIFLD),     FIELD(Z_RG,g_lastswLDAD));

        printf ("  oE_SET_F_SET=%o       lbFET=%o               x_DMAADDR=%o    ",  FIELD(Z_RF,f_oE_SET_F_SET),   FIELD(Z_RG,g_lbFET),          FIELD(Z_RO,o_x_DMAADDR));
        printf ("  i_EMA=%o              swLDAD=%o            lastswSTART=%o" EOL,  FIELD(Z_RA,a_i_EMA),          FIELD(Z_RB,b_swLDAD),     FIELD(Z_RG,g_lastswSTART));

        printf ("  oJMP_JMS=%o           lbION=%o               x_DMADATA=%o    ",  FIELD(Z_RF,f_oJMP_JMS),       FIELD(Z_RG,g_lbION),          FIELD(Z_RO,o_x_DMADATA));
        printf ("  i_INT_INHIBIT=%o      swMPRT=%o" EOL,                            FIELD(Z_RA,a_i_INT_INHIBIT),  FIELD(Z_RB,b_swMPRT));

        printf ("  oLINE_LOW=%o          lbLINK=%o                             ",   FIELD(Z_RF,f_oLINE_LOW),      FIELD(Z_RG,g_lbLINK));
        printf ("  i_INT_RQST=%o         swSTEP=%o" EOL,                            FIELD(Z_RA,a_i_INT_RQST),     FIELD(Z_RB,b_swSTEP));

        printf ("  oMEMSTART=%o          lbRUN=%o             bMEMBUS=%04o     ",  FIELD(Z_RF,f_oMEMSTART),      FIELD(Z_RG,g_lbRUN),          FIELD(Z_RP,p_bMEMBUS));
        printf ("  i_IO_SKIP=%o          swSTOP=%o" EOL,                            FIELD(Z_RA,a_i_IO_SKIP),      FIELD(Z_RB,b_swSTOP));

        printf ("  o_ADDR_ACCEPT=%o      lbWC=%o                r_MA=%o         ",  FIELD(Z_RF,f_o_ADDR_ACCEPT),  FIELD(Z_RG,g_lbWC),           FIELD(Z_RO,o_r_MA));
        printf ("  i_MEMDONE=%o          swSTART=%o" EOL,                           FIELD(Z_RA,a_i_MEMDONE),      FIELD(Z_RB,b_swSTART));

        printf ("  o_BF_ENABLE=%o        lbIR=%o                x_MEM=%o        ",  FIELD(Z_RF,f_o_BF_ENABLE),    FIELD(Z_RG,g_lbIR),           FIELD(Z_RO,o_x_MEM));
        printf ("  i_STROBE=%o           swSR=%04o" EOL,                            FIELD(Z_RA,a_i_STROBE),       FIELD(Z_RB,b_swSR));

        printf ("  o_BUSINIT=%o" EOL,                                               FIELD(Z_RF,f_o_BUSINIT));
        printf ("  o_B_RUN=%o            lbAC=%04o           bPIOBUS=%04o" EOL,     FIELD(Z_RF,f_o_B_RUN),        FIELD(Z_RI,i_lbAC),           FIELD(Z_RP,p_bPIOBUS));
        printf ("  o_DF_ENABLE=%o        lbMA=%04o             r_BAC=%o" EOL,       FIELD(Z_RF,f_o_DF_ENABLE),    FIELD(Z_RJ,j_lbMA),           FIELD(Z_RO,o_r_BAC));
        printf ("  o_KEY_CLEAR=%o        lbMB=%04o             r_BMB=%o" EOL,       FIELD(Z_RF,f_o_KEY_CLEAR),    FIELD(Z_RJ,j_lbMB),           FIELD(Z_RO,o_r_BMB));
        printf ("  o_KEY_DF=%o                                 x_INPUTBUS=%o" EOL,  FIELD(Z_RF,f_o_KEY_DF),                                     FIELD(Z_RO,o_x_INPUTBUS));
        printf ("  o_KEY_IF=%o           o_LOAD_SF=%o" EOL,                         FIELD(Z_RF,f_o_KEY_IF),       FIELD(Z_RF,f_o_LOAD_SF));
        printf ("  o_KEY_LOAD=%o         o_SP_CYC_NEXT=%o" EOL,                     FIELD(Z_RF,f_o_KEY_LOAD),     FIELD(Z_RF,f_o_SP_CYC_NEXT));

        for (int i = 0; i < 1024;) {
            uint32_t idver = z8ls[i];
            if (((idver >> 24) == 'X') && (((idver >> 16) & 255) == 'M')) {
                printf (EOL "VERSION=%08X XM  enlo4k=%o  enable=%o  ifld=%o  dfld=%o  field=%o  memdelay=%3u  _mwdone=%o  _mrdone=%o" EOL,
                    idver, FIELD (i+1,XM_ENLO4K), FIELD (i+1,XM_ENABLE), FIELD (i+2,XM2_IFLD), FIELD (i+2,XM2_DFLD), FIELD (i+2,XM2_FIELD),
                    FIELD (i+2,XM2_MEMDELAY), FIELD (i+2,XM2__MWDONE), FIELD (i+2,XM2__MRDONE));
            } else {
                if ((idver & 0xF000U) == 0x0000U) {
                    printf (EOL "VERSION=%08X %c%c %08X" EOL, idver, idver >> 24, idver >> 16, z8ls[i+1]);
                }
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

        printf (EOP);

        fflush (stdout);

        if (stepmode) {
            char temp[8];
            printf ("\n > ");
            fflush (stdout);
            if (fgets (temp, sizeof temp, stdin) == NULL) break;
            zynqpage[Z_RE] |= e_nanotrigger;
        }
    }
    printf ("\n");
    return 0;
}
