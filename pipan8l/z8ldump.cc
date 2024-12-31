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

//  ./z8ldump.armv7l [-once | -step] [-xmem <lo>..<hi>]...

#include <alloca.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "z8ldefs.h"
#include "z8lutil.h"

#define ESC_NORMV "\033[m"          // go back to normal video
#define ESC_REVER "\033[7m"         // turn reverse video on
#define ESC_UNDER "\033[4m"         // turn underlining on
#define ESC_BLINK "\033[5m"         // turn blink on
#define ESC_BOLDV "\033[1m"         // turn bold on
#define ESC_REDBG "\033[41m"        // red background
#define ESC_YELBG "\033[44m"        // yellow background
#define ESC_EREOL "\033[K"          // erase to end of line
#define ESC_EREOP "\033[J"          // erase to end of page
#define ESC_HOMEC "\033[H"          // home cursor

#define EOL ESC_EREOL "\n"
#define EOP ESC_EREOP

#define FIELD(index,mask) ((z8ls[index] & mask) / (mask & - mask))
#define BUS12(index,topbit) ((z8ls[index] / (topbit / 2048)) & 4095)

struct XMemRange {
    XMemRange *next;
    uint16_t loaddr;
    uint16_t hiaddr;
};

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

    bool oncemode = false;
    bool pagemode = false;
    bool stepmode = false;
    char const *eol = EOL;
    XMemRange **lxmemrange, *xmemrange, *xmemranges;
    lxmemrange = &xmemranges;
    for (int i = 0; ++ i < argc;) {
        if (strcmp (argv[i], "-?") == 0) {
            puts ("");
            puts ("     Dump ZTurn FPGA state");
            puts ("     Does not alter the state");
            puts ("");
            puts ("  ./z8ldump.armv7l [-once | -page | -step] [-xmem <lo>..<hi>]...");
            puts ("     -once : just print the state once, else update continually");
            puts ("     -page : just dump the raw page then exit");
            puts ("     -step : prompt between updates");
            puts ("     -xmem : dump the given extended memory range instead of register state");
            puts ("             may be given more than once");
            puts ("             Does not dump PDP-8/L core memory.  If need to dump low 4K memory,");
            puts ("             restart everything with z8lreal giving it the -enlo4k option.");
            puts ("");
            return 0;
        }
        if (strcasecmp (argv[i], "-once") == 0) {
            eol = "\n";
            oncemode = true;
            continue;
        }
        if (strcasecmp (argv[i], "-page") == 0) {
            pagemode = true;
            continue;
        }
        if (strcasecmp (argv[i], "-step") == 0) {
            stepmode = true;
            continue;
        }
        if (strcasecmp (argv[i], "-xmem") == 0) {
            if ((++ i >= argc) || (argv[i][0] == '-')) {
                fprintf (stderr, "missing lo..hi address range after -xmem\n");
                return 1;
            }
            char *p;
            unsigned long hiaddr, loaddr;
            hiaddr = loaddr = strtoul (argv[i], &p, 8);
            if ((p[0] == '.') && (p[1] == '.')) {
                hiaddr = strtoul (p + 2, &p, 8);
            }
            if ((*p != 0) || (loaddr > hiaddr) || (hiaddr > 077777)) {
                fprintf (stderr, "bad address range %s must be <loaddr>..<hiaddr>\n", argv[i]);
                return 1;
            }
            xmemrange = (XMemRange *) alloca (sizeof *xmemrange);
            xmemrange->loaddr = loaddr;
            xmemrange->hiaddr = hiaddr;
            *lxmemrange = xmemrange;
            lxmemrange = &xmemrange->next;
            continue;
        }
        fprintf (stderr, "unknown argument %s\n", argv[i]);
        return 1;
    }
    *lxmemrange = NULL;

    Z8LPage z8p;
    uint32_t volatile *pdpat = z8p.findev ("8L", NULL, NULL, false);

    // maybe just dump the page and exit
    if (pagemode) {
        uint32_t words[1024];
        for (int i = 0; i < 1024; i ++) words[i] = pdpat[i];
        int k;
        for (k = 1024; words[--k] == 0xDEADBEEF;) { }
        for (int i = 0; i <= k; i += 8) {
            printf ("%03X:", i);
            for (int j = 0; j < 8; j ++) {
                printf (" %08X", words[i+j]);
            }
            printf ("\n");
        }
        return 0;
    }

    uint32_t volatile *extmem = (xmemranges == NULL) ? NULL : z8p.extmem ();

    if (stepmode) {
        pdpat[Z_RE] &= ~ (e_nanocontin | e_nanotrigger);
    } else {
        signal (SIGINT, siginthand);
    }

    while (! exitflag) {
        usleep (1000);

        uint32_t z8ls[1024];
        if (xmemranges == NULL) {
            for (int i = 0; i < 1024; i ++) {
                z8ls[i] = pdpat[i];
            }
        }

        if (! oncemode) printf (ESC_HOMEC);

        if (xmemranges == NULL) {

            // zynq.v register dump
            printf ("VERSION=%08X 8L%s", z8ls[0], eol);

            printf ("  oBIOP1=%o             oBAC=%04o           brkwhenhltd=%o      iBEMA=%o              iDMAADDR=%04o       majstate=%s%s",      FIELD(Z_RF,f_oBIOP1),         FIELD(Z_RH,h_oBAC),           FIELD(Z_RE,e_brkwhenhltd), FIELD(Z_RA,a_iBEMA),         FIELD(Z_RD,d_iDMAADDR),   majstatenames[FIELD(Z_RK,k_majstate)],   eol);
            printf ("  oBIOP2=%o             oBMB=%04o           nanocstep=%o        i_CA_INCRMNT=%o       iDMADATA=%04o       nextmajst=%s%s",     FIELD(Z_RF,f_oBIOP2),         FIELD(Z_RH,h_oBMB),           FIELD(Z_RE,e_nanocstep),   FIELD(Z_RA,a_i_CA_INCRMNT),  FIELD(Z_RD,d_iDMADATA),   majstatenames[FIELD(Z_RK,k_nextmajst)],  eol);
            printf ("  oBIOP4=%o             oMA=%04o            nanotrigger=%o      i_DATA_IN=%o          iINPUTBUS=%04o      timedelay=%o%s",     FIELD(Z_RF,f_oBIOP4),         FIELD(Z_RI,i_oMA),            FIELD(Z_RE,e_nanotrigger), FIELD(Z_RA,a_i_DATA_IN),     FIELD(Z_RC,c_iINPUTBUS),  FIELD(Z_RK,k_timedelay),                 eol);
            printf ("  oBTP2=%o                                  nanocontin=%o       i_MEMINCR=%o          i_MEM=%04o          timestate=%s%s",     FIELD(Z_RF,f_oBTP2),                                        FIELD(Z_RE,e_nanocontin),  FIELD(Z_RA,a_i_MEMINCR),     FIELD(Z_RC,c_i_MEM),      timestatenames[FIELD(Z_RK,k_timestate)], eol);
            printf ("  oBTP3=%o              lbBRK=%o             softreset=%o        i_MEM_P=%o            swCONT=%o            cyclectr=%04o%s",  FIELD(Z_RF,f_oBTP3),          FIELD(Z_RG,g_lbBRK),          FIELD(Z_RE,e_softreset),   FIELD(Z_RA,a_i_MEM_P),       FIELD(Z_RB,b_swCONT),     FIELD(Z_RK,k_cyclectr),                  eol);
            printf ("  oBTS_1=%o             lbCA=%o              simit=%o            i3CYCLE=%o            swDEP=%o             simmemen=%o%s",    FIELD(Z_RF,f_oBTS_1),         FIELD(Z_RG,g_lbCA),           FIELD(Z_RE,e_simit),       FIELD(Z_RA,a_i3CYCLE),       FIELD(Z_RB,b_swDEP),      FIELD(Z_RG,g_simmemen),                  eol);
            printf ("  oBTS_3=%o             lbDEF=%o             bareit=%o           iAC_CLEAR=%o          swDFLD=%o%s",                           FIELD(Z_RF,f_oBTS_3),         FIELD(Z_RG,g_lbDEF),          FIELD(Z_RE,e_bareit),      FIELD(Z_RA,a_iAC_CLEAR),     FIELD(Z_RB,b_swDFLD),                                              eol);
            printf ("  o_BWC_OVERFLOW=%o     lbEA=%o                                 iBRK_RQST=%o          swEXAM=%o            debounced=%o%s",    FIELD(Z_RF,f_o_BWC_OVERFLOW), FIELD(Z_RG,g_lbEA),                                      FIELD(Z_RA,a_iBRK_RQST),     FIELD(Z_RB,b_swEXAM),     FIELD(Z_RG,g_debounced),                 eol);
            printf ("  o_B_BREAK=%o          lbEXE=%o             bDMABUS=%04o       i_EA=%o               swIFLD=%o            lastswLDAD=%o%s",   FIELD(Z_RF,f_o_B_BREAK),      FIELD(Z_RG,g_lbEXE),          FIELD(Z_RO,o_bDMABUS),     FIELD(Z_RA,a_i_EA),          FIELD(Z_RB,b_swIFLD),     FIELD(Z_RG,g_lastswLDAD),                eol);
            printf ("  oE_SET_F_SET=%o       lbFET=%o             x_DMAADDR=%o        iEMA=%o               swLDAD=%o            lastswSTART=%o%s", FIELD(Z_RF,f_oE_SET_F_SET),   FIELD(Z_RG,g_lbFET),          FIELD(Z_RO,o_x_DMAADDR),   FIELD(Z_RA,a_iEMA),          FIELD(Z_RB,b_swLDAD),     FIELD(Z_RG,g_lastswSTART),               eol);
            printf ("  oJMP_JMS=%o           lbION=%o             x_DMADATA=%o        iINT_INHIBIT=%o       swMPRT=%o%s",                           FIELD(Z_RF,f_oJMP_JMS),       FIELD(Z_RG,g_lbION),          FIELD(Z_RO,o_x_DMADATA),   FIELD(Z_RA,a_iINT_INHIBIT),  FIELD(Z_RB,b_swMPRT),                                              eol);
            printf ("  oLINE_LOW=%o          lbLINK=%o                               iINT_RQST=%o          swSTEP=%o%s",                            FIELD(Z_RF,f_oLINE_LOW),      FIELD(Z_RG,g_lbLINK),                                    FIELD(Z_RA,a_iINT_RQST),     FIELD(Z_RB,b_swSTEP),                                              eol);
            printf ("  oMEMSTART=%o          lbRUN=%o             bMEMBUS=%04o       iIO_SKIP=%o           swSTOP=%o%s",                            FIELD(Z_RF,f_oMEMSTART),      FIELD(Z_RG,g_lbRUN),          FIELD(Z_RP,p_bMEMBUS),     FIELD(Z_RA,a_iIO_SKIP),      FIELD(Z_RB,b_swSTOP),                                              eol);
            printf ("  o_ADDR_ACCEPT=%o      lbWC=%o              r_MA=%o             i_MEMDONE=%o          swSTART=%o%s",                          FIELD(Z_RF,f_o_ADDR_ACCEPT),  FIELD(Z_RG,g_lbWC),           FIELD(Z_RO,o_r_MA),        FIELD(Z_RA,a_i_MEMDONE),     FIELD(Z_RB,b_swSTART),                                             eol);
            printf ("  o_BF_ENABLE=%o        lbIR=%o              x_MEM=%o            i_STROBE=%o           swSR=%04o%s",                           FIELD(Z_RF,f_o_BF_ENABLE),    FIELD(Z_RG,g_lbIR),           FIELD(Z_RO,o_x_MEM),       FIELD(Z_RA,a_i_STROBE),      FIELD(Z_RB,b_swSR),                                                eol);
            printf ("  o_BUSINIT=%o%s",                                                                                                             FIELD(Z_RF,f_o_BUSINIT),                                                                                                                                                               eol);
            printf ("  oB_RUN=%o             lbAC=%04o           bPIOBUS=%04o       memcycctr=%08X   xbraddr=%05o%s",                               FIELD(Z_RF,f_oB_RUN),         FIELD(Z_RI,i_lbAC),           FIELD(Z_RP,p_bPIOBUS),     z8ls[Z_RN],                  FIELD(Z_RL,l_xbraddr),                                             eol);
            printf ("  o_DF_ENABLE=%o        lbMA=%04o           r_BAC=%o            didio=%o              xbrwena=%o%s",                           FIELD(Z_RF,f_o_DF_ENABLE),    FIELD(Z_RJ,j_lbMA),           FIELD(Z_RO,o_r_BAC),       FIELD(Z_RF,f_didio),         FIELD(Z_RL,l_xbrwena),                                             eol);
            printf ("  o_KEY_CLEAR=%o        lbMB=%04o           r_BMB=%o            hizmembus=%o          xbrenab=%o%s",                           FIELD(Z_RF,f_o_KEY_CLEAR),    FIELD(Z_RJ,j_lbMB),           FIELD(Z_RO,o_r_BMB),       FIELD(Z_RO,o_hizmembus),     FIELD(Z_RL,l_xbrenab),                                             eol);
            printf ("  o_KEY_DF=%o                               x_INPUTBUS=%o       meminprog=%4u       xbrrdat=%04o%s",                           FIELD(Z_RF,f_o_KEY_DF),                                     FIELD(Z_RO,o_x_INPUTBUS),  FIELD(Z_RL,l_meminprog),     FIELD(Z_RM,m_xbrrdat),                                             eol);
            printf ("  o_KEY_IF=%o           o_LOAD_SF=%o                                                 xbrwdat=%04o%s",                          FIELD(Z_RF,f_o_KEY_IF),       FIELD(Z_RF,f_o_LOAD_SF),                                                              FIELD(Z_RM,m_xbrwdat),                                             eol);
            printf ("  o_KEY_LOAD=%o         o_SP_CYC_NEXT=%o%s",                                                                                   FIELD(Z_RF,f_o_KEY_LOAD),     FIELD(Z_RF,f_o_SP_CYC_NEXT),                                                                                                                             eol);

            for (int i = 0; i < 1024;) {
                uint32_t idver = z8ls[i];
                if (((idver >> 24) == 'X') && (((idver >> 16) & 255) == 'M')) {
                    printf ("%sVERSION=%08X XM  enlo4k=%o  enable=%o  ifld=%o  dfld=%o  field=%o  _mwdone=%o  _mrdone=%o  os8zap=%o,step=%o%s",
                        eol,
                        idver, FIELD (i+1,XM_ENLO4K), FIELD (i+1,XM_ENABLE), FIELD (i+2,XM2_IFLD), FIELD (i+2,XM2_DFLD), FIELD (i+2,XM2_FIELD),
                        FIELD (i+2,XM2__MWDONE), FIELD (i+2,XM2__MRDONE), FIELD (i+1,XM_OS8ZAP), FIELD (i+3,XM3_OS8STEP), eol);
                    printf ("                     mdhold=%o  mdstep=%o  xmstate=%2u  xmmemenab=%o%s",
                        FIELD(i+1,XM_MDHOLD), FIELD(i+1,XM_MDSTEP), FIELD(i+2,XM2_XMSTATE), FIELD(i+2,XM2_XMMEMENAB), eol);
                } else {
                    if ((idver & 0xF000U) == 0x0000U) {
                        printf ("%sVERSION=%08X %c%c %08X%s", eol, idver, idver >> 24, idver >> 16, z8ls[i+1], eol);
                    }
                    if ((idver & 0xF000U) == 0x1000U) {
                        printf ("%sVERSION=%08X %c%c %08X %08X %08X%s", eol, idver, idver >> 24, idver >> 16, z8ls[i+1], z8ls[i+2], z8ls[i+3], eol);
                    }
                    if ((idver & 0xF000U) == 0x2000U) {
                        printf ("%sVERSION=%08X %c%c %08X %08X %08X %08X %08X %08X %08X%s", eol, idver, idver >> 24, idver >> 16,
                            z8ls[i+1], z8ls[i+2], z8ls[i+3], z8ls[i+4], z8ls[i+5], z8ls[i+6], z8ls[i+7], eol);
                    }
                }
                i += 2 << ((idver >> 12) & 15);
            }
        } else {

            // memory dump
            for (xmemrange = xmemranges; xmemrange != NULL; xmemrange = xmemrange->next) {
                uint16_t loaddr = xmemrange->loaddr;
                uint16_t hiaddr = xmemrange->hiaddr;
                fputs (eol, stdout);
                for (uint16_t lnaddr = loaddr & -16; lnaddr <= hiaddr; lnaddr += 16) {
                    printf (" %05o :", lnaddr);
                    for (uint16_t i = 0; i < 16; i ++) {
                        uint16_t addr = lnaddr + i;
                        if ((addr < loaddr) || (addr > hiaddr)) {
                            printf ("     ");
                        } else {
                            printf (" %04o", extmem[addr]);
                        }
                    }
                    fputs (eol, stdout);
                }
            }
        }

        if (oncemode) break;

        printf (EOP);
        fflush (stdout);

        if (stepmode) {
            char temp[8];
            printf ("\n > ");
            fflush (stdout);
            if (fgets (temp, sizeof temp, stdin) == NULL) break;
            pdpat[Z_RE] |= e_nanotrigger;
        }
    }
    printf ("\n");
    return 0;
}
