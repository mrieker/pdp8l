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

#ifndef _I2CZLIB_H
#define _I2CZLIB_H

#include <netinet/in.h>
#include <stdint.h>

#include "z8lutil.h"

// toggle switches
struct Z8LToggles {
    uint16_t sr;
    bool mprt;
    bool dfld;
    bool ifld;
    bool step;
};

// button (momentary) switches
struct Z8LButtons {
    bool stop;
    bool cont;
    bool start;
    bool ldad;
    bool exam;
    bool dep;
};

// light bulbs
struct Z8LLights {
    uint16_t ma;
    uint16_t mb;
    uint16_t ac;
    uint16_t ir;
    bool link;
    bool ema;
    bool fet;
    bool ion;
    bool pare;
    bool exe;
    bool def;
    bool prte;
    bool run;
    bool wct;
    bool cad;
    bool brk;
};

// all switches and lights
struct Z8LPanel {
    Z8LToggles togovr;  // which toggles are being overridden
    Z8LToggles togval;  // toggle switch values
    Z8LButtons button;  // button switch values
    Z8LLights  light;   // light bulb values
};

#define I2CZUDPPORT 25747
struct I2CZUDPPkt {
    uint64_t seq;
    uint16_t cmd;
    uint16_t dirs[4];
    uint16_t vals[4];
};

struct I2CZLib {
    I2CZLib ();
    ~I2CZLib ();
    void openpads (bool dislo4k, bool enlo4k, bool real, bool sim);
    void relall ();
    void readpads (Z8LPanel *pads);
    void writepads (Z8LPanel const *pads);

    void readi2c (uint16_t *dirs, uint16_t *vals, bool latch);
    void writei2c (uint16_t *dirs, uint16_t *vals);
private:
    int udpfd;
    struct sockaddr_in servaddr;
    uint32_t volatile *pdpat;
    uint32_t volatile *fpat;
    uint64_t udpseq;
    Z8LPage *z8p;

    void writebut (uint16_t *dirs, uint16_t *vals, bool val, int validx, int valbit);
    void writetog (uint16_t *dirs, uint16_t *vals, bool val, bool ovr, int collidx, int collbit, int baseidx, int basebit);
    void doudpio (I2CZUDPPkt *udppkt);
    uint16_t read16 (uint8_t addr, uint8_t reg);
    void write16 (uint8_t addr, uint8_t reg, uint16_t word);
    uint64_t doi2ccycle (uint64_t cmd, int i2cus);
};

#endif
