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

/**
 * Decode/Encode ABCD and GPIO connector pins to/from various signals.
 */

#include <string.h>

#include "abcd.h"

// - pin assignments from ../modules/cons.inc
#define A_acq_11      (1U << ( 2 - 1))
#define A__aluq_11    (1U << ( 3 - 1))
#define A__maq_11     (1U << ( 4 - 1))
#define A_maq_11      (1U << ( 5 - 1))
#define A_mq_11       (1U << ( 6 - 1))
#define A_pcq_11      (1U << ( 7 - 1))
#define A_irq_11      (1U << ( 8 - 1))
#define A_acq_10      (1U << ( 9 - 1))
#define A__aluq_10    (1U << (10 - 1))
#define A__maq_10     (1U << (11 - 1))
#define A_maq_10      (1U << (12 - 1))
#define A_mq_10       (1U << (13 - 1))
#define A_pcq_10      (1U << (14 - 1))
#define A_irq_10      (1U << (15 - 1))
#define A_acq_09      (1U << (16 - 1))
#define A__aluq_09    (1U << (17 - 1))
#define A__maq_09     (1U << (18 - 1))
#define A_maq_09      (1U << (19 - 1))
#define A_mq_09       (1U << (20 - 1))
#define A_pcq_09      (1U << (21 - 1))
#define A_irq_09      (1U << (22 - 1))
#define A_acq_08      (1U << (23 - 1))
#define A__aluq_08    (1U << (24 - 1))
#define A__maq_08     (1U << (25 - 1))
#define A_maq_08      (1U << (26 - 1))
#define A_mq_08       (1U << (27 - 1))
#define A_pcq_08      (1U << (28 - 1))
#define A_acq_07      (1U << (30 - 1))
#define A__aluq_07    (1U << (31 - 1))
#define A__maq_07     (1U << (32 - 1))

#define B_maq_07      (1U << ( 2 - 1))
#define B_mq_07       (1U << ( 3 - 1))
#define B_pcq_07      (1U << ( 4 - 1))
#define B_acq_06      (1U << ( 6 - 1))
#define B__aluq_06    (1U << ( 7 - 1))
#define B__maq_06     (1U << ( 8 - 1))
#define B_maq_06      (1U << ( 9 - 1))
#define B_mq_06       (1U << (10 - 1))
#define B_pcq_06      (1U << (11 - 1))
#define B__jump       (1U << (12 - 1))
#define B_acq_05      (1U << (13 - 1))
#define B__aluq_05    (1U << (14 - 1))
#define B__maq_05     (1U << (15 - 1))
#define B_maq_05      (1U << (16 - 1))
#define B_mq_05       (1U << (17 - 1))
#define B_pcq_05      (1U << (18 - 1))
#define B__alub_m1    (1U << (19 - 1))
#define B_acq_04      (1U << (20 - 1))
#define B__aluq_04    (1U << (21 - 1))
#define B__maq_04     (1U << (22 - 1))
#define B_maq_04      (1U << (23 - 1))
#define B_mq_04       (1U << (24 - 1))
#define B_pcq_04      (1U << (25 - 1))
#define B_acqzero     (1U << (26 - 1))
#define B_acq_03      (1U << (27 - 1))
#define B__aluq_03    (1U << (28 - 1))
#define B__maq_03     (1U << (29 - 1))
#define B_maq_03      (1U << (30 - 1))
#define B_mq_03       (1U << (31 - 1))
#define B_pcq_03      (1U << (32 - 1))

#define C_acq_02      (1U << ( 2 - 1))
#define C__aluq_02    (1U << ( 3 - 1))
#define C__maq_02     (1U << ( 4 - 1))
#define C_maq_02      (1U << ( 5 - 1))
#define C_mq_02       (1U << ( 6 - 1))
#define C_pcq_02      (1U << ( 7 - 1))
#define C__ac_sc      (1U << ( 8 - 1))
#define C_acq_01      (1U << ( 9 - 1))
#define C__aluq_01    (1U << (10 - 1))
#define C__maq_01     (1U << (11 - 1))
#define C_maq_01      (1U << (12 - 1))
#define C_mq_01       (1U << (13 - 1))
#define C_pcq_01      (1U << (14 - 1))
#define C_intak1q     (1U << (15 - 1))
#define C_acq_00      (1U << (16 - 1))
#define C__aluq_00    (1U << (17 - 1))
#define C__maq_00     (1U << (18 - 1))
#define C_maq_00      (1U << (19 - 1))
#define C_mq_00       (1U << (20 - 1))
#define C_pcq_00      (1U << (21 - 1))
#define C_fetch1q     (1U << (22 - 1))
#define C__ac_aluq    (1U << (23 - 1))
#define C__alu_add    (1U << (24 - 1))
#define C__alu_and    (1U << (25 - 1))
#define C__alua_m1    (1U << (26 - 1))
#define C__alucout    (1U << (27 - 1))
#define C__alua_ma    (1U << (28 - 1))
#define C_alua_mq0600 (1U << (29 - 1))
#define C_alua_mq1107 (1U << (30 - 1))
#define C_alua_pc0600 (1U << (31 - 1))
#define C_alua_pc1107 (1U << (32 - 1))

#define D_alub_1      (1U << ( 2 - 1))
#define D__alub_ac    (1U << ( 3 - 1))
#define D_clok2       (1U << ( 4 - 1))
#define D_fetch2q     (1U << ( 5 - 1))
#define D__grpa1q     (1U << ( 6 - 1))
#define D_defer1q     (1U << ( 7 - 1))
#define D_defer2q     (1U << ( 8 - 1))
#define D_defer3q     (1U << ( 9 - 1))
#define D_exec1q      (1U << (10 - 1))
#define D_grpb_skip   (1U << (11 - 1))
#define D_exec2q      (1U << (12 - 1))
#define D__dfrm       (1U << (13 - 1))
#define D_inc_axb     (1U << (14 - 1))
#define D__intak      (1U << (15 - 1))
#define D_intrq       (1U << (16 - 1))
#define D_exec3q      (1U << (17 - 1))
#define D_ioinst      (1U << (18 - 1))
#define D_ioskp       (1U << (19 - 1))
#define D_iot2q       (1U << (20 - 1))
#define D__ln_wrt     (1U << (21 - 1))
#define D__lnq        (1U << (22 - 1))
#define D_lnq         (1U << (23 - 1))
#define D__ma_aluq    (1U << (24 - 1))
#define D_mql         (1U << (25 - 1))
#define D__mread      (1U << (26 - 1))
#define D__mwrite     (1U << (27 - 1))
#define D__pc_aluq    (1U << (28 - 1))
#define D__pc_inc     (1U << (29 - 1))
#define D_reset       (1U << (30 - 1))
#define D__newlink    (1U << (31 - 1))
#define D_tad3q       (1U << (32 - 1))

void ABCD::zeroit ()
{
    memset (cons, 0, sizeof cons);
    decode ();
}

// decode {a,b,c,d}con => signals
void ABCD::decode ()
{
    acqzero     = (bcon & B_acqzero    ) != 0;
    _jump       = (bcon & B__jump      ) != 0;
    _alub_m1    = (bcon & B__alub_m1   ) != 0;

    _ac_sc      = (ccon & C__ac_sc     ) != 0;
    intak1q     = (ccon & C_intak1q    ) != 0;
    fetch1q     = (ccon & C_fetch1q    ) != 0;
    _ac_aluq    = (ccon & C__ac_aluq   ) != 0;
    _alu_add    = (ccon & C__alu_add   ) != 0;
    _alu_and    = (ccon & C__alu_and   ) != 0;
    _alua_m1    = (ccon & C__alua_m1   ) != 0;
    _alucout    = (ccon & C__alucout   ) != 0;
    _alua_ma    = (ccon & C__alua_ma   ) != 0;
    alua_mq0600 = (ccon & C_alua_mq0600) != 0;
    alua_mq1107 = (ccon & C_alua_mq1107) != 0;
    alua_pc0600 = (ccon & C_alua_pc0600) != 0;
    alua_pc1107 = (ccon & C_alua_pc1107) != 0;

    alub_1      = (dcon & D_alub_1     ) != 0;
    _alub_ac    = (dcon & D__alub_ac   ) != 0;
    clok2       = (dcon & D_clok2      ) != 0;
    fetch2q     = (dcon & D_fetch2q    ) != 0;
    _grpa1q     = (dcon & D__grpa1q    ) != 0;
    defer1q     = (dcon & D_defer1q    ) != 0;
    defer2q     = (dcon & D_defer2q    ) != 0;
    defer3q     = (dcon & D_defer3q    ) != 0;
    exec1q      = (dcon & D_exec1q     ) != 0;
    grpb_skip   = (dcon & D_grpb_skip  ) != 0;
    exec2q      = (dcon & D_exec2q     ) != 0;
    _dfrm       = (dcon & D__dfrm      ) != 0;
    inc_axb     = (dcon & D_inc_axb    ) != 0;
    _intak      = (dcon & D__intak     ) != 0;
    intrq       = (dcon & D_intrq      ) != 0;
    exec3q      = (dcon & D_exec3q     ) != 0;
    ioinst      = (dcon & D_ioinst     ) != 0;
    ioskp       = (dcon & D_ioskp      ) != 0;
    iot2q       = (dcon & D_iot2q      ) != 0;
    _ln_wrt     = (dcon & D__ln_wrt    ) != 0;
    _lnq        = (dcon & D__lnq       ) != 0;
    lnq         = (dcon & D_lnq        ) != 0;
    _ma_aluq    = (dcon & D__ma_aluq   ) != 0;
    mql         = (dcon & D_mql        ) != 0;
    _mread      = (dcon & D__mread     ) != 0;
    _mwrite     = (dcon & D__mwrite    ) != 0;
    _pc_aluq    = (dcon & D__pc_aluq   ) != 0;
    _pc_inc     = (dcon & D__pc_inc    ) != 0;
    reset       = (dcon & D_reset      ) != 0;
    _newlink    = (dcon & D__newlink   ) != 0;
    tad3q       = (dcon & D_tad3q      ) != 0;

    acq   = 0;
    _aluq = 0;
    _maq  = 0;
    maq   = 0;
    mq    = 0;
    pcq   = 0;
    irq   = 0;

    if (acon & A_acq_11     ) acq      |= (1U << 11);
    if (acon & A__aluq_11   ) _aluq    |= (1U << 11);
    if (acon & A__maq_11    ) _maq     |= (1U << 11);
    if (acon & A_maq_11     ) maq      |= (1U << 11);
    if (acon & A_mq_11      ) mq       |= (1U << 11);
    if (acon & A_pcq_11     ) pcq      |= (1U << 11);
    if (acon & A_irq_11     ) irq      |= (1U << 11);
    if (acon & A_acq_10     ) acq      |= (1U << 10);
    if (acon & A__aluq_10   ) _aluq    |= (1U << 10);
    if (acon & A__maq_10    ) _maq     |= (1U << 10);
    if (acon & A_maq_10     ) maq      |= (1U << 10);
    if (acon & A_mq_10      ) mq       |= (1U << 10);
    if (acon & A_pcq_10     ) pcq      |= (1U << 10);
    if (acon & A_irq_10     ) irq      |= (1U << 10);
    if (acon & A_acq_09     ) acq      |= (1U <<  9);
    if (acon & A__aluq_09   ) _aluq    |= (1U <<  9);
    if (acon & A__maq_09    ) _maq     |= (1U <<  9);
    if (acon & A_maq_09     ) maq      |= (1U <<  9);
    if (acon & A_mq_09      ) mq       |= (1U <<  9);
    if (acon & A_pcq_09     ) pcq      |= (1U <<  9);
    if (acon & A_irq_09     ) irq      |= (1U <<  9);
    if (acon & A_acq_08     ) acq      |= (1U <<  8);
    if (acon & A__aluq_08   ) _aluq    |= (1U <<  8);
    if (acon & A__maq_08    ) _maq     |= (1U <<  8);
    if (acon & A_maq_08     ) maq      |= (1U <<  8);
    if (acon & A_mq_08      ) mq       |= (1U <<  8);
    if (acon & A_pcq_08     ) pcq      |= (1U <<  8);
    if (acon & A_acq_07     ) acq      |= (1U <<  7);
    if (acon & A__aluq_07   ) _aluq    |= (1U <<  7);
    if (acon & A__maq_07    ) _maq     |= (1U <<  7);

    if (bcon & B_maq_07     ) maq      |= (1U <<  7);
    if (bcon & B_mq_07      ) mq       |= (1U <<  7);
    if (bcon & B_pcq_07     ) pcq      |= (1U <<  7);
    if (bcon & B_acq_06     ) acq      |= (1U <<  6);
    if (bcon & B__aluq_06   ) _aluq    |= (1U <<  6);
    if (bcon & B__maq_06    ) _maq     |= (1U <<  6);
    if (bcon & B_maq_06     ) maq      |= (1U <<  6);
    if (bcon & B_mq_06      ) mq       |= (1U <<  6);
    if (bcon & B_pcq_06     ) pcq      |= (1U <<  6);
    if (bcon & B_acq_05     ) acq      |= (1U <<  5);
    if (bcon & B__aluq_05   ) _aluq    |= (1U <<  5);
    if (bcon & B__maq_05    ) _maq     |= (1U <<  5);
    if (bcon & B_maq_05     ) maq      |= (1U <<  5);
    if (bcon & B_mq_05      ) mq       |= (1U <<  5);
    if (bcon & B_pcq_05     ) pcq      |= (1U <<  5);
    if (bcon & B_acq_04     ) acq      |= (1U <<  4);
    if (bcon & B__aluq_04   ) _aluq    |= (1U <<  4);
    if (bcon & B__maq_04    ) _maq     |= (1U <<  4);
    if (bcon & B_maq_04     ) maq      |= (1U <<  4);
    if (bcon & B_mq_04      ) mq       |= (1U <<  4);
    if (bcon & B_pcq_04     ) pcq      |= (1U <<  4);
    if (bcon & B_acq_03     ) acq      |= (1U <<  3);
    if (bcon & B__aluq_03   ) _aluq    |= (1U <<  3);
    if (bcon & B__maq_03    ) _maq     |= (1U <<  3);
    if (bcon & B_maq_03     ) maq      |= (1U <<  3);
    if (bcon & B_mq_03      ) mq       |= (1U <<  3);
    if (bcon & B_pcq_03     ) pcq      |= (1U <<  3);

    if (ccon & C_acq_02     ) acq      |= (1U <<  2);
    if (ccon & C__aluq_02   ) _aluq    |= (1U <<  2);
    if (ccon & C__maq_02    ) _maq     |= (1U <<  2);
    if (ccon & C_maq_02     ) maq      |= (1U <<  2);
    if (ccon & C_mq_02      ) mq       |= (1U <<  2);
    if (ccon & C_pcq_02     ) pcq      |= (1U <<  2);
    if (ccon & C_acq_01     ) acq      |= (1U <<  1);
    if (ccon & C__aluq_01   ) _aluq    |= (1U <<  1);
    if (ccon & C__maq_01    ) _maq     |= (1U <<  1);
    if (ccon & C_maq_01     ) maq      |= (1U <<  1);
    if (ccon & C_mq_01      ) mq       |= (1U <<  1);
    if (ccon & C_pcq_01     ) pcq      |= (1U <<  1);
    if (ccon & C_acq_00     ) acq      |= (1U <<  0);
    if (ccon & C__aluq_00   ) _aluq    |= (1U <<  0);
    if (ccon & C__maq_00    ) _maq     |= (1U <<  0);
    if (ccon & C_maq_00     ) maq      |= (1U <<  0);
    if (ccon & C_mq_00      ) mq       |= (1U <<  0);
    if (ccon & C_pcq_00     ) pcq      |= (1U <<  0);
}

// encode signals => {a,b,c,d}con
void ABCD::encode ()
{
    acon = 0;
    bcon = 0;
    ccon = 0;
    dcon = 0;

    if (acqzero    ) bcon |= B_acqzero    ;
    if (_jump      ) bcon |= B__jump      ;
    if (_alub_m1   ) bcon |= B__alub_m1   ;

    if (_ac_sc     ) ccon |= C__ac_sc     ;
    if (intak1q    ) ccon |= C_intak1q    ;
    if (fetch1q    ) ccon |= C_fetch1q    ;
    if (_ac_aluq   ) ccon |= C__ac_aluq   ;
    if (_alu_add   ) ccon |= C__alu_add   ;
    if (_alu_and   ) ccon |= C__alu_and   ;
    if (_alua_m1   ) ccon |= C__alua_m1   ;
    if (_alucout   ) ccon |= C__alucout   ;
    if (_alua_ma   ) ccon |= C__alua_ma   ;
    if (alua_mq0600) ccon |= C_alua_mq0600;
    if (alua_mq1107) ccon |= C_alua_mq1107;
    if (alua_pc0600) ccon |= C_alua_pc0600;
    if (alua_pc1107) ccon |= C_alua_pc1107;

    if (alub_1     ) dcon |= D_alub_1     ;
    if (_alub_ac   ) dcon |= D__alub_ac   ;
    if (clok2      ) dcon |= D_clok2      ;
    if (fetch2q    ) dcon |= D_fetch2q    ;
    if (_grpa1q    ) dcon |= D__grpa1q    ;
    if (defer1q    ) dcon |= D_defer1q    ;
    if (defer2q    ) dcon |= D_defer2q    ;
    if (defer3q    ) dcon |= D_defer3q    ;
    if (exec1q     ) dcon |= D_exec1q     ;
    if (grpb_skip  ) dcon |= D_grpb_skip  ;
    if (exec2q     ) dcon |= D_exec2q     ;
    if (_dfrm      ) dcon |= D__dfrm      ;
    if (inc_axb    ) dcon |= D_inc_axb    ;
    if (_intak     ) dcon |= D__intak     ;
    if (intrq      ) dcon |= D_intrq      ;
    if (exec3q     ) dcon |= D_exec3q     ;
    if (ioinst     ) dcon |= D_ioinst     ;
    if (ioskp      ) dcon |= D_ioskp      ;
    if (iot2q      ) dcon |= D_iot2q      ;
    if (_ln_wrt    ) dcon |= D__ln_wrt    ;
    if (_lnq       ) dcon |= D__lnq       ;
    if (lnq        ) dcon |= D_lnq        ;
    if (_ma_aluq   ) dcon |= D__ma_aluq   ;
    if (mql        ) dcon |= D_mql        ;
    if (_mread     ) dcon |= D__mread     ;
    if (_mwrite    ) dcon |= D__mwrite    ;
    if (_pc_aluq   ) dcon |= D__pc_aluq   ;
    if (_pc_inc    ) dcon |= D__pc_inc    ;
    if (reset      ) dcon |= D_reset      ;
    if (_newlink   ) dcon |= D__newlink   ;
    if (tad3q      ) dcon |= D_tad3q      ;

    if (acq      & (1U << 11)) acon |= A_acq_11     ;
    if (_aluq    & (1U << 11)) acon |= A__aluq_11   ;
    if (_maq     & (1U << 11)) acon |= A__maq_11    ;
    if (maq      & (1U << 11)) acon |= A_maq_11     ;
    if (mq       & (1U << 11)) acon |= A_mq_11      ;
    if (pcq      & (1U << 11)) acon |= A_pcq_11     ;
    if (irq      & (1U << 11)) acon |= A_irq_11     ;
    if (acq      & (1U << 10)) acon |= A_acq_10     ;
    if (_aluq    & (1U << 10)) acon |= A__aluq_10   ;
    if (_maq     & (1U << 10)) acon |= A__maq_10    ;
    if (maq      & (1U << 10)) acon |= A_maq_10     ;
    if (mq       & (1U << 10)) acon |= A_mq_10      ;
    if (pcq      & (1U << 10)) acon |= A_pcq_10     ;
    if (irq      & (1U << 10)) acon |= A_irq_10     ;
    if (acq      & (1U <<  9)) acon |= A_acq_09     ;
    if (_aluq    & (1U <<  9)) acon |= A__aluq_09   ;
    if (_maq     & (1U <<  9)) acon |= A__maq_09    ;
    if (maq      & (1U <<  9)) acon |= A_maq_09     ;
    if (mq       & (1U <<  9)) acon |= A_mq_09      ;
    if (pcq      & (1U <<  9)) acon |= A_pcq_09     ;
    if (irq      & (1U <<  9)) acon |= A_irq_09     ;
    if (acq      & (1U <<  8)) acon |= A_acq_08     ;
    if (_aluq    & (1U <<  8)) acon |= A__aluq_08   ;
    if (_maq     & (1U <<  8)) acon |= A__maq_08    ;
    if (maq      & (1U <<  8)) acon |= A_maq_08     ;
    if (mq       & (1U <<  8)) acon |= A_mq_08      ;
    if (pcq      & (1U <<  8)) acon |= A_pcq_08     ;
    if (acq      & (1U <<  7)) acon |= A_acq_07     ;
    if (_aluq    & (1U <<  7)) acon |= A__aluq_07   ;
    if (_maq     & (1U <<  7)) acon |= A__maq_07    ;

    if (maq      & (1U <<  7)) bcon |= B_maq_07     ;
    if (mq       & (1U <<  7)) bcon |= B_mq_07      ;
    if (pcq      & (1U <<  7)) bcon |= B_pcq_07     ;
    if (acq      & (1U <<  6)) bcon |= B_acq_06     ;
    if (_aluq    & (1U <<  6)) bcon |= B__aluq_06   ;
    if (_maq     & (1U <<  6)) bcon |= B__maq_06    ;
    if (maq      & (1U <<  6)) bcon |= B_maq_06     ;
    if (mq       & (1U <<  6)) bcon |= B_mq_06      ;
    if (pcq      & (1U <<  6)) bcon |= B_pcq_06     ;
    if (acq      & (1U <<  5)) bcon |= B_acq_05     ;
    if (_aluq    & (1U <<  5)) bcon |= B__aluq_05   ;
    if (_maq     & (1U <<  5)) bcon |= B__maq_05    ;
    if (maq      & (1U <<  5)) bcon |= B_maq_05     ;
    if (mq       & (1U <<  5)) bcon |= B_mq_05      ;
    if (pcq      & (1U <<  5)) bcon |= B_pcq_05     ;
    if (acq      & (1U <<  4)) bcon |= B_acq_04     ;
    if (_aluq    & (1U <<  4)) bcon |= B__aluq_04   ;
    if (_maq     & (1U <<  4)) bcon |= B__maq_04    ;
    if (maq      & (1U <<  4)) bcon |= B_maq_04     ;
    if (mq       & (1U <<  4)) bcon |= B_mq_04      ;
    if (pcq      & (1U <<  4)) bcon |= B_pcq_04     ;
    if (acq      & (1U <<  3)) bcon |= B_acq_03     ;
    if (_aluq    & (1U <<  3)) bcon |= B__aluq_03   ;
    if (_maq     & (1U <<  3)) bcon |= B__maq_03    ;
    if (maq      & (1U <<  3)) bcon |= B_maq_03     ;
    if (mq       & (1U <<  3)) bcon |= B_mq_03      ;
    if (pcq      & (1U <<  3)) bcon |= B_pcq_03     ;

    if (acq      & (1U <<  2)) ccon |= C_acq_02     ;
    if (_aluq    & (1U <<  2)) ccon |= C__aluq_02   ;
    if (_maq     & (1U <<  2)) ccon |= C__maq_02    ;
    if (maq      & (1U <<  2)) ccon |= C_maq_02     ;
    if (mq       & (1U <<  2)) ccon |= C_mq_02      ;
    if (pcq      & (1U <<  2)) ccon |= C_pcq_02     ;
    if (acq      & (1U <<  1)) ccon |= C_acq_01     ;
    if (_aluq    & (1U <<  1)) ccon |= C__aluq_01   ;
    if (_maq     & (1U <<  1)) ccon |= C__maq_01    ;
    if (maq      & (1U <<  1)) ccon |= C_maq_01     ;
    if (mq       & (1U <<  1)) ccon |= C_mq_01      ;
    if (pcq      & (1U <<  1)) ccon |= C_pcq_01     ;
    if (acq      & (1U <<  0)) ccon |= C_acq_00     ;
    if (_aluq    & (1U <<  0)) ccon |= C__aluq_00   ;
    if (_maq     & (1U <<  0)) ccon |= C__maq_00    ;
    if (maq      & (1U <<  0)) ccon |= C_maq_00     ;
    if (mq       & (1U <<  0)) ccon |= C_mq_00      ;
    if (pcq      & (1U <<  0)) ccon |= C_pcq_00     ;
}

std::string ABCD::states ()
{
    std::string str;
    if (fetch1q) str.append ("FETCH1 ");
    if (fetch2q) str.append ("FETCH2 ");
    if (defer1q) str.append ("DEFER1 ");
    if (defer2q) str.append ("DEFER2 ");
    if (defer3q) str.append ("DEFER3 ");
    if (exec1q)  str.append ("EXEC1 ");
    if (exec2q)  str.append ("EXEC2 ");
    if (exec3q)  str.append ("EXEC3 ");
    if (intak1q) str.append ("INTAK1 ");
    if (! str.empty ()) str.pop_back ();
    return str;
}
