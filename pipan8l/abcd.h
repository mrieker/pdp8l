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

#ifndef _ABCD_H
#define _ABCD_H

#include <stdint.h>
#include <string>

struct ABCD {
    union {
        uint32_t cons[4];
        struct {
            uint32_t acon;
            uint32_t bcon;
            uint32_t ccon;
            uint32_t dcon;
        };
    };

    bool acqzero;

    bool _jump;
    bool _ac_sc;
    bool intak1q;
    bool fetch1q;
    bool _ac_aluq;
    bool _alu_add;
    bool _alu_and;
    bool _alua_m1;
    bool _alucout;
    bool _alua_ma;
    bool alua_mq0600;
    bool alua_mq1107;
    bool alua_pc0600;
    bool alua_pc1107;

    bool _alub_m1;
    bool alub_1;
    bool _alub_ac;
    bool clok2;
    bool fetch2q;
    bool _grpa1q;
    bool defer1q;
    bool defer2q;
    bool defer3q;
    bool exec1q;
    bool grpb_skip;
    bool exec2q;
    bool _dfrm;
    bool inc_axb;
    bool _intak;
    bool intrq;
    bool exec3q;
    bool ioinst;
    bool ioskp;
    bool iot2q;
    bool _ln_wrt;
    bool _lnq;
    bool lnq;
    bool _ma_aluq;
    bool mql;
    bool _mread;
    bool _mwrite;
    bool _pc_aluq;
    bool _pc_inc;
    bool reset;
    bool _newlink;
    bool tad3q;

    uint16_t acq;
    uint16_t _aluq;
    uint16_t _maq;
    uint16_t maq;
    uint16_t mq;
    uint16_t pcq;
    uint16_t irq;

    void zeroit ();
    void decode ();
    void encode ();
    std::string states ();
};

#endif
