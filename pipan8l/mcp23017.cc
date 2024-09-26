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

// Test MCP23017

//  ./mcp23017 /dev/i2c-1 <chipnumber>
//  top 4 bits of both ports are outputs
//  bottom 4 bits of both ports are inputs

/*
  assuming single mcp23017 plugged into address 4:
    $ i2cdetect -y 1
         0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f
    00:                         -- -- -- -- -- -- -- -- 
    10: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- 
    20: -- -- -- -- 24 -- -- -- -- -- -- -- -- -- -- -- 
    30: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- 
    40: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- 
    50: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- 
    60: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- 
    70: -- -- -- -- -- -- -- --                         
    $
*/

#include <fcntl.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

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

static int i2cfd;

uint16_t read16 (uint8_t addr, uint8_t reg);
void write16 (uint8_t addr, uint8_t reg, uint16_t word);

int main (int argc, char **argv)
{
    if (argc != 3) {
        fprintf (stderr, "usage: ./mcp23017 /dev/i2c-1 n\n");
        return -1;
    }

    i2cfd = open (argv[1], O_RDWR);
    if (i2cfd < 0) {
        fprintf (stderr, "main: error opening %s: %m\n", argv[1]);
        abort ();
    }

    int chipno = atoi (argv[2]);

    write16 (I2CBA + chipno, IODIRA, 0x0F0F);

    while (true) {
        uint16_t word = read16 (I2CBA + chipno, GPIOA);
        printf ("%d  %04X  =>  ", chipno, word);
        char inbuf[30];
        if (fgets (inbuf, sizeof inbuf, stdin) == NULL) break;
        word = strtoul (inbuf, NULL, 16);
        write16 (I2CBA + chipno, OLATA, word);
    }
}

// read 16-bit value from MCP23017 on I2C bus
//  input:
//   addr = MCP23017 address
//   reg = starting register within MCP23017
//  output:
//   returns [07:00] = reg+0 contents
//           [15:08] = reg+1 contents
uint16_t read16 (uint8_t addr, uint8_t reg)
{
    struct i2c_rdwr_ioctl_data msgset;
    struct i2c_msg iomsgs[2];
    uint8_t wbuf[1], rbuf[2];

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
    iomsgs[1].len   = 2;

    msgset.msgs = iomsgs;
    msgset.nmsgs = 2;

    int rc = ioctl (i2cfd, I2C_RDWR, &msgset);
    if (rc < 0) {
        fprintf (stderr, "read16: error reading from %02X.%02X: %m\n", addr, reg);
        abort ();
    }

    // reg+1 in upper byte; reg+0 in lower byte
    return (rbuf[1] << 8) | rbuf[0];
}

// write 16-bit value to MCP23017 registers on I2C bus
//  input:
//   addr = MCP23017 address
//   reg = starting register within MCP23017
//   word = value to write
//          word[07:00] => reg+0
//          word[15:08] => reg+1
void write16 (uint8_t addr, uint8_t reg, uint16_t word)
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
        fprintf (stderr, "write16: error writing to %02X.%02X: %m\n", addr, reg);
        abort ();
    }
}
