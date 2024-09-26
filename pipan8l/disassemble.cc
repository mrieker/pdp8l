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

#include <stdio.h>
#include <stdlib.h>

#include "disassemble.h"

static char const *const skips[] = {
    "",                 //  -   -   -   -
    "SKP ",             //  -   -   -  rev
    "SNL ",             //  -   -  SNL  -
    "SZL ",             //  -   -  SNL rev
    "SZA ",             //  -  SZA  -   -
    "SNA ",             //  -  SZA  -  rev
    "SZA+SNL ",         //  -  SZA SNL  -
    "SNA*SZL ",         //  -  SZA SNL rev
    "SMA ",             // SMA  -   -   -
    "SPA ",             // SMA  -   -  rev
    "SMA+SNL ",         // SMA  -  SNL  -
    "SPA*SZL ",         // SMA  -  SNL rev
    "SMA+SZA ",         // SMA SZA  -   -
    "SPA*SNA ",         // SMA SZA  -  rev
    "SMA+SZA+SNL ",     // SMA SZA SNL  -
    "SPA*SNA*SZL " };   // SMA SZA SNL rev

static void appendoctal (std::string *str, uint16_t val);

// disassemble the given opcode
//  input:
//   opc = opcode to disassemble
//   pc  = address of instruction (not after it)
//  output:
//   returns string giving disassembly
std::string disassemble (uint16_t opc, uint16_t pc)
{
    std::string str;

    switch (opc >> 9) {
        case 0: str.append ("AND"); break;
        case 1: str.append ("TAD"); break;
        case 2: str.append ("ISZ"); break;
        case 3: str.append ("DCA"); break;
        case 4: str.append ("JMS"); break;
        case 5: str.append ("JMP"); break;
        case 6: {
            str.append ("IOT   ");
            appendoctal (&str, opc);
            return str;
        }
        case 7: {
            if (! (opc & 256)) {
                if (opc & 128) str.append ("CLA ");
                if (opc &  64) str.append ("CLL ");
                if (opc &  32) str.append ("CMA ");
                if (opc &  16) str.append ("CML ");
                if (opc &   1) str.append ("IAC ");
                switch ((opc >> 1) & 7) {
                    case 1: str.append ("BSW "); break;
                    case 2: str.append ("RAL "); break;
                    case 3: str.append ("RTL "); break;
                    case 4: str.append ("RAR "); break;
                    case 5: str.append ("RTR "); break;
                    case 6: str.append ("??6 "); break;
                    case 7: str.append ("??7 "); break;
                }
            } else if (! (opc & 1)) {
                str.append (skips[(opc>>3)&15]);
                if (opc & 128) str.append ("CLA ");
                if (opc &   4) str.append ("OSR ");
                if (opc &   2) str.append ("HLT ");
            } else {
                if (opc & 00200) str.append ("CLA ");
                if ((opc & 07577) == 07431) str.append ("SWAB ");  // MQL NMI
                else {
                    switch (opc & 00120) {
                        case 00020: str.append ("MQL "); break;
                        case 00100: str.append ("MQA "); break;
                        case 00120: str.append ("SWP "); break;
                    }
                    if (opc & 00056) {
                        switch ((opc >> 1) & 027) {
                            case 001: if (opc & 00320) str.append (": "); str.append ("SCL ? ACS "); break;
                            case 002: str.append ("MUY "); break;
                            case 003: str.append ("DVI "); break;
                            case 004: str.append ("NMI "); break;
                            case 005: str.append ("SHL "); break;
                            case 006: str.append ("ASR "); break;
                            case 007: str.append ("LSR "); break;
                            case 020: str.append ("SCA "); break;
                            case 021: if (opc & 00320) str.append (": "); str.append ("SCA SCL ? DAD "); break;
                            case 022: if (opc & 00320) str.append (": "); str.append ("SCA MUY ? DST "); break;
                            case 023: str.append ("SWBA "); break;
                            case 024: if (opc & 00320) str.append (": "); str.append ("SCA NMI ? DPSZ "); break;
                            case 025: if (opc & 00320) str.append (": "); str.append ("SCA SHL ? DPIC "); break;
                            case 026: if (opc & 00320) str.append (": "); str.append ("SCA ASR ? DCM "); break;
                            case 027: if (opc & 00320) str.append (": "); str.append ("SCA LSR ? SAM "); break;
                        }
                    }
                }
            }
            if (str.length () == 0) str.append ("NOP");
                 else str.erase (str.length () - 1, 1);
            return str;
        }
        default: abort ();
    }
    str.append ((opc & 00400) ? "I  " : "   ");
    opc &= 00377;
    if (opc & 00200) opc += (pc & 07600) - 00200;
    appendoctal (&str, opc);
    return str;
}

static void appendoctal (std::string *str, uint16_t val)
{
    for (int b = 9; b >= 0; b -=3) {
        str->push_back ((char) (((val >> b) & 7) + '0'));
    }
}
