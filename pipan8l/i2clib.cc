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

// Plugs Raspberry PI into PDP-8/L console slot via 5 MCP23017s connected to the RasPIs I2C bus

/*
  commands:
    i2cdetect

  enable /dev/i2c-1 with raspi-config
    should see /dev/i2c-1

  https://forums.raspberrypi.com/viewtopic.php?t=344789
    Normally on a Pi4 you have buses
    - i2c-1 on GPIOs 2&3. Requires dtparam=i2c_arm in config.txt
    - i2c-20 and i2c-21 on the HDMI ports for reading the EDID. Requires dtoverlay=vc4-kms-v3d in config.txt.
    - i2c-0 on GPIOs 0&1. Requires dtparam=i2c_vc in config.txt.
    - i2c-10 for the camera and display. Requires dtparam=i2c_vc in config.txt.
    - i2c-22 as the parent hardware block for i2c-0 and i2c-10, as they are just alternate pinmuxing options for the one block. This should be ignored as it has no defined GPIOs to expose it to the outside world, and will just use whichever of i2c-0 and i2c-10 that was last used.

  https://github.com/fivdi/i2c-bus/blob/master/doc/raspberry-pi-i2c.md
    add following to /boot/config.txt           "maybe this is hardware i2c"
      dtparam=i2c_arm=on
      dtparam=i2c_arm_baudrate=400000
    add following to /etc/modules
      i2c-dev

  https://github.com/fivdi/i2c-bus/blob/master/doc/raspberry-pi-software-i2c.md
    dtoverlay=i2c-gpio,bus=3    "software i2c", uses GPIO 23(SDA) & 24(SCL)
*/

#include <errno.h>
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

#include "padlib.h"
#include "pindefs.h"

#define GPIO_FSEL0 (0)          // set output current / disable (readwrite)
#define GPIO_SET0 (0x1C/4)      // set gpio output bits (writeonly)
#define GPIO_CLR0 (0x28/4)      // clear gpio output bits (writeonly)
#define GPIO_LEV0 (0x34/4)      // read gpio bit levels (readonly)
#define GPIO_PUDMOD (0x94/4)    // pullup/pulldown enable (0=disable; 1=enable pulldown; 2=enable pullup)
#define GPIO_PUDCLK (0x98/4)    // which pins to change pullup/pulldown mode

#define ABORT() do { fprintf (stderr, "abort() %s:%d\n", __FILE__, __LINE__); abort (); } while (0)
#define ASSERT(cond) do { if (__builtin_constant_p (cond)) { if (!(cond)) asm volatile ("assert failure line %c0" :: "i"(__LINE__)); } else { if (!(cond)) ABORT (); } } while (0)

#define I2CBA  0x20     // I2C address of MCP23017 chip 0

#define IODIRA   0x00   // pin direction: '1'=input ; '0'=output
#define IODIRB   0x01   //   IO<7> must be set to 'output'
#define IPOLA    0x02   // input polarity: '1'=flip ; '0'=normal
#define IPOLB    0x03
#define GPINTENA 0x04   // interrupt on difference bitmask
#define GPINTENB 0x05
#define DEFVALA  0x06   // comparison value for interrupt on difference
#define DEFVALB  0x07
#define INTCONA  0x08   // more interrupt comparison
#define INTCONB  0x09
#define IOCONA   0x0A   // various control bits
#define IOCONB   0x0B
#define GPPUA    0x0C   // pullup enable bits
#define GPPUB    0x0D   //   '0' = disabled ; '1' = enabled
#define INTFA    0x0E   // interrupt flag bits
#define INTFB    0x0F
#define INTCAPA  0x10   // interrupt captured value
#define INTCAPB  0x11
#define GPIOA    0x12   // reads pin state
#define GPIOB    0x13
#define OLATA    0x14   // output pin latch
#define OLATB    0x15

// which of the pins are outputs (switches)
static uint16_t const wrmsks[P_NU16S] = { P0_WMSK, P1_WMSK, P2_WMSK, P3_WMSK, P4_WMSK };
// active low outputs
static uint16_t const wrrevs[P_NU16S] = { P0_WREV, P1_WREV, P2_WREV, P3_WREV, P4_WREV };
// active low inputs + active low outputs (so we can read the outputs back)
static uint16_t const rdwrrevs[P_NU16S] = { P0_RREV | P0_WREV, P1_RREV | P1_WREV, P2_RREV | P2_WREV, P3_RREV | P3_WREV, P4_RREV | P4_WREV };



I2CLib::I2CLib ()
{
    i2cfd = -1;
}

I2CLib::~I2CLib ()
{
    close (i2cfd);
    i2cfd = -1;
}

void I2CLib::openpads ()
{
    close (i2cfd);
    i2cfd = -1;

    char *i2cenv = getenv ("i2clibdev");
    if (i2cenv == NULL) {
        fprintf (stderr, "I2CLib::openpads: envar i2clibdev undefined\n");
        ABORT ();
    }

    // pulse GPIO<16> = PIN[36] to reset the MCP23017s
    int gpiofd = open ("/dev/gpiomem", O_RDWR);
    if ((gpiofd >= 0) || (errno != ENOENT)) {
        if (gpiofd < 0) {
            fprintf (stderr, "I2CLib::openpads: error opening /dev/gpiomem: %m\n");
            ABORT ();
        }
        void *gpioptr = mmap (NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, gpiofd, 0);
        if (gpioptr == MAP_FAILED) {
            fprintf (stderr, "I2CLib::openpads: error mmapping /dev/gpiomem: %m\n");
            ABORT ();
        }
        uint32_t volatile *gpiopage = (uint32_t volatile *) gpioptr;

        // set GPIO<16> (pin[36]) to output mode
        gpiopage[GPIO_FSEL0+1] = (gpiopage[GPIO_FSEL0+1] & 037770777777) | 000001000000;

        // turn bit<16> on
        gpiopage[GPIO_SET0] = 1U << 16;

        // let it soak in
        usleep (10);

        // turn bit<16> off
        gpiopage[GPIO_CLR0] = 1U << 16;

        // let it soak in
        usleep (10);

        // all done with GPIO
        munmap (gpioptr, 4096);
        close (gpiofd);
    }

    // open I2C bus #1 which is on GPIO connector : SDA=GPIO<02>=PIN[03] SCL=GPIO<03>=PIN[05]
    i2cfd = open (i2cenv, O_RDWR);
    if (i2cfd < 0) {
        fprintf (stderr, "I2CLib::openpads: error opening %s: %m\n", i2cenv);
        ABORT ();
    }

    // set output mode for output pins
    // also enable weak pullups (ignored for output pins)
    for (int pad = 0; pad < P_NU16S; pad ++) {
        // bit<7> of both ports must be configured as outputs
        uint16_t abdir = ~ wrmsks[pad];
        ASSERT ((abdir & 0x8080) == 0);
        write16 (I2CBA + pad, IODIRA, abdir);
        write16 (I2CBA + pad, GPPUA, 0xFFFF);
    }
}

// read values from all 5 MCP23017s
// flip the pins we consider to be active low so caller only sees active high
void I2CLib::readpads (uint16_t *pads)
{
    for (int pad = 0; pad < P_NU16S; pad ++) {
        uint16_t raw = read16 (I2CBA + pad, GPIOA);
        pads[pad] = raw ^ rdwrrevs[pad];
    }
}

int I2CLib::readpin (int pinnum)
{
    if (pinnum < 0) return -1;
    if (pinnum >= P_NU16S * 16) return -1;
    uint16_t raw = read16 (I2CBA + (pinnum / 16), GPIOA);
    return (raw >> (pinnum % 16)) & 1;
}

// write values to all 5 MCP23017s
// flip the pins we consider to be active low so caller can pass active high
void I2CLib::writepads (uint16_t const *pads)
{
    for (int pad = 0; pad < P_NU16S; pad ++) {
        write16 (I2CBA + pad, OLATA, pads[pad] ^ wrrevs[pad]);
    }
}

int I2CLib::writepin (int pinnum, int pinval)
{
    if (pinnum < 0) return -1;
    if (pinnum >= P_NU16S * 16) return -1;
    if (pinval < 0) return -1;
    if (pinval > 1) return -1;
    uint16_t raw = read16 (I2CBA + (pinnum / 16), OLATA);
    if (pinval) raw |= (1U << (pinnum % 16));
         else raw &= ~ (1U << (pinnum % 16));
    write16 (I2CBA + (pinnum / 16), OLATA, raw);
    return pinval;
}



// https://github.com/ve3wwg/raspberry_pi/blob/master/mcp23017/i2c_funcs.c

// read 8-bit value from mcp23017 reg at addr
uint8_t I2CLib::read8 (uint8_t addr, uint8_t reg)
{
    struct i2c_rdwr_ioctl_data msgset;
    struct i2c_msg iomsgs[2];
    uint8_t wbuf[1], rbuf[1];

    memset (&msgset, 0, sizeof msgset);
    memset (iomsgs, 0, sizeof iomsgs);

    wbuf[0] = reg;              // MCP23017 register number
    iomsgs[0].addr  = addr;
    iomsgs[0].flags = 0;        // write
    iomsgs[0].buf   = wbuf;
    iomsgs[0].len   = 1;

    iomsgs[1].addr  = addr;
    iomsgs[1].flags = I2C_M_RD; // read
    iomsgs[1].buf   = rbuf;
    iomsgs[1].len   = 1;

    msgset.msgs = iomsgs;
    msgset.nmsgs = 2;

    int rc = ioctl (i2cfd, I2C_RDWR, &msgset);
    if (rc < 0) {
        fprintf (stderr, "I2CLib::read8: error reading from %02X.%02X: %m\n", addr, reg);
        ABORT ();
    }
    return rbuf[0];
}

// write 8-bit value to mcp23017 reg at addr
void I2CLib::write8 (uint8_t addr, uint8_t reg, uint8_t byte)
{
    struct i2c_rdwr_ioctl_data msgset;
    struct i2c_msg iomsgs[1];
    uint8_t buf[2];

    memset (&msgset, 0, sizeof msgset);
    memset (iomsgs, 0, sizeof iomsgs);

    buf[0] = reg;               // MCP23017 register number
    buf[1] = byte;              // byte for reg+0

    iomsgs[0].addr  = addr;
    iomsgs[0].flags = 0;        // write
    iomsgs[0].buf   = buf;
    iomsgs[0].len   = 2;

    msgset.msgs  = iomsgs;
    msgset.nmsgs = 1;

    int rc = ioctl (i2cfd, I2C_RDWR, &msgset);
    if (rc < 0) {
        fprintf (stderr, "I2CLib::write8: error writing to %02X.%02X: %m\n", addr, reg);
        ABORT ();
    }
}

// read 16-bit value from MCP23017 on I2C bus
//  input:
//   addr = MCP23017 address
//   reg = starting register within MCP23017
//  output:
//   returns [07:00] = reg+0 contents
//           [15:08] = reg+1 contents
uint16_t I2CLib::read16 (uint8_t addr, uint8_t reg)
{
    struct i2c_rdwr_ioctl_data msgset;
    struct i2c_msg iomsgs[2];
    uint8_t wbuf[1], rbuf[2];

    memset (&msgset, 0, sizeof msgset);
    memset (iomsgs, 0, sizeof iomsgs);

    for (int retry = 0; retry < 3; retry ++) {
        wbuf[0] = reg;              // MCP23017 register number
        iomsgs[0].addr  = addr;
        iomsgs[0].flags = 0;        // write
        iomsgs[0].buf   = wbuf;
        iomsgs[0].len   = 1;

        iomsgs[1].addr  = addr;
        iomsgs[1].flags = I2C_M_RD; // read
        iomsgs[1].buf   = rbuf;
        iomsgs[1].len   = 2;

        msgset.msgs = iomsgs;
        msgset.nmsgs = 2;

        int rc = ioctl (i2cfd, I2C_RDWR, &msgset);
        if (rc >= 0) {
            if (retry > 0) fprintf (stderr, "I2CLib::read16: reading recovered\n");
            // reg+1 in upper byte; reg+0 in lower byte
            return (rbuf[1] << 8) | rbuf[0];
        }

        fprintf (stderr, "I2CLib::read16: error reading from %02X.%02X: %m\n", addr, reg);
    }
    ABORT ();
}

// write 16-bit value to MCP23017 registers on I2C bus
//  input:
//   addr = MCP23017 address
//   reg = starting register within MCP23017
//   word = value to write
//          word[07:00] => reg+0
//          word[15:08] => reg+1
void I2CLib::write16 (uint8_t addr, uint8_t reg, uint16_t word)
{
    struct i2c_rdwr_ioctl_data msgset;
    struct i2c_msg iomsgs[1];
    uint8_t buf[3];

    memset (&msgset, 0, sizeof msgset);
    memset (iomsgs, 0, sizeof iomsgs);

    buf[0] = reg;               // MCP23017 register number
    buf[1] = word;              // byte for reg+0
    buf[2] = word >> 8;         // byte for reg+1

    iomsgs[0].addr  = addr;
    iomsgs[0].flags = 0;        // write
    iomsgs[0].buf   = buf;
    iomsgs[0].len   = 3;

    msgset.msgs  = iomsgs;
    msgset.nmsgs = 1;

    int rc = ioctl (i2cfd, I2C_RDWR, &msgset);
    if (rc < 0) {
        fprintf (stderr, "I2CLib::write16: error writing to %02X.%02X: %m\n", addr, reg);
        ABORT ();
    }
}
