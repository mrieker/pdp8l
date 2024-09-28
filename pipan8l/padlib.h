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

#ifndef _PADLIB_H
#define _PADLIB_H

#include "pindefs.h"

struct PadLib {
    virtual ~PadLib () { }
    virtual void openpads () = 0;
    virtual void readpads (uint16_t *pads) = 0;
    virtual void writepads (uint16_t const *pads) = 0;
    virtual int readpin (int pinnum) { return -1; }
    virtual int writepin (int pinnum, int pinval) { return -1; }
};

struct I2CLib : PadLib {
    I2CLib ();
    virtual ~I2CLib ();
    virtual void openpads ();
    virtual void readpads (uint16_t *pads);
    virtual void writepads (uint16_t const *pads);
    virtual int readpin (int pinnum);
    virtual int writepin (int pinnum, int pinval);

private:
    int i2cfd;
    uint32_t volatile *zynqpage;

    uint8_t read8 (uint8_t addr, uint8_t reg);
    void write8 (uint8_t addr, uint8_t reg, uint8_t byte);
    uint16_t read16 (uint8_t addr, uint8_t reg);
    void write16 (uint8_t addr, uint8_t reg, uint16_t word);
};

struct SimLib : PadLib {
    SimLib ();
    virtual void openpads ();
    virtual void readpads (uint16_t *pads);
    virtual void writepads (uint16_t const *pads);

private:
    enum State { NUL, FET, EXE, DEF, WCT, CAD, BRK };

    bool dfldsw, dfreg, eareg, ifldsw, ifreg, ionreg, lnreg, mprtsw, stepsw;
    State state;
    uint16_t acreg, irtop, mareg, mbreg, pcreg, swreg;
    uint16_t memarray[8192];
    uint16_t wrpads[P_NU16S];

    bool volatile runreg;       // false: thread is not running or just about to exit
                                //   runtid = 0: thread is not running
                                //         else: thread is just about to exit
                                // - cleared by main thread or run thread
                                // true: thread is running or is being started up
                                //   runtid = 0: thread is being started up
                                //         else: thread is running
                                // - set only by main thread

    pthread_t runtid;           // - set and cleared only by main thread

    void spreadreg (uint16_t reg, uint16_t *pads, int npins, uint8_t const *pins);
    void spreadpin (bool val, uint16_t *pads, uint8_t pin);
    uint16_t gatherreg (uint16_t const *pads, int npins, uint8_t const *pins);
    bool gatherpin (uint16_t const *pads, uint8_t pin);

    void contswitch ();
    void stopswitch ();
    static void *runthreadwrap (void *zhis);
    void runthread ();
    void singlestep ();
    void dofetch ();
    void domemref ();
    void dooperate ();
    void doioinst ();
    char const *ststr ();
};

#endif
