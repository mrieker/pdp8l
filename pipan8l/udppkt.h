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

#ifndef _UDPPKT_H
#define _UDPPKT_H

#include <stdint.h>

#define UDPPORT 23456

struct UDPPkt {
    uint64_t seq;
    uint16_t ma;
    uint16_t ir;
    uint16_t mb;
    uint16_t ac;
    uint16_t sr;
    bool ea;
    bool stf;
    bool ste;
    bool std;
    bool stwc;
    bool stca;
    bool stb;
    bool link;
    bool ion;
    bool per;
    bool prot;
    bool run;
    bool mprt;
    bool dfld;
    bool ifld;
    bool ldad;
    bool start;
    bool cont;
    bool stop;
    bool step;
    bool exam;
    bool dep;
};

#endif
