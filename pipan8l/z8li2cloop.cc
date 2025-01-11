
// g++ -o x x.cc lib.armv7l.a

#include <stdio.h>
#include <unistd.h>

#include "padlib.h"

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

#define ZPI2CCMD 1      // i2cmaster.v command register (64 bits)
#define ZPI2CSTS 3      // i2cmaster.v status register (64 bits)
#define ZYNQWR 3ULL     // write command (2 bits)
#define ZYNQRD 2ULL     // read command (2 bits)
#define ZYNQST 1ULL     // send restart bit (2 bits)
#define I2CWR 0ULL      // 8th address bit indicating write
#define I2CRD 1ULL      // 8th address bit indicating read

static uint16_t const wrmsks[P_NU16S] = { P0_WMSK, P1_WMSK, P2_WMSK, P3_WMSK, P4_WMSK };

static uint32_t volatile *fpat;

static void write16 (uint8_t addr, uint8_t reg, uint16_t word);

int main (int argc, char **argv)
{
    Z8LPage *z8p = new Z8LPage ();

    uint32_t volatile *pdpat = z8p->findev ("8L", NULL, NULL, true);

    // access real PDP-8/L front panel via I2C bus connected to PIPAN8L board
    fpat = z8p->findev ("FP", NULL, NULL, true);
    fprintf (stderr, "FP version %08X\n", fpat[0]);

    while (true) {
        uint64_t cmd = (ZYNQWR << 62);
        // writing high-order word triggers i/o, so write low-order first
        fpat[ZPI2CCMD+0] = cmd;
        fpat[ZPI2CCMD+1] = cmd >> 32;
        // give busy bit plenty of time to set, should take well under 100nS
        usleep (1);
        // wait for busy bit to clear, should take less than 12uS
        for (int i = 1000000; -- i >= 0;) {
            uint32_t sts = fpat[ZPI2CSTS+1];
            if (! (sts & (1U << 31))) {
                // make sure ack was received from mcp23017.v
                ////if (sts & (1U << 30)) {
                ////    fprintf (stderr, "I2CLib::write16: i2c I/O error\n");
                ////    ABORT ();
                ////}
                break;
            }
        }
/***
        for (int pad = 0; pad < P_NU16S; pad ++) {
            // bit<7> of both ports must be configured as outputs
            uint16_t abdir = ~ wrmsks[pad];
            ASSERT ((abdir & 0x8080) == 0);
            write16 (I2CBA + pad, IODIRA, abdir);
            write16 (I2CBA + pad, GPPUA, 0xFFFF);
        }
***/
    }
}

// write 16-bit value to MCP23017 registers on I2C bus
//  input:
//   addr = MCP23017 address
//   reg = starting register within MCP23017
//   word = value to write
//          word[07:00] => reg+0
//          word[15:08] => reg+1
static void write16 (uint8_t addr, uint8_t reg, uint16_t word)
{
    // build 64-bit command
    //  send-8-bits-to-i2cbus 7-bit-address 1=read
    //  send-8-bits-to-i2cbus 8-bit-register-number
    //  send-8-bits-to-i2cbus 8-bit-reg+0-value
    //  send-8-bits-to-i2cbus 8-bit-reg+1-value
    //  0...
    uint64_t cmd =
        (ZYNQWR << 62) | ((uint64_t) addr << 55) | (I2CWR << 54) |
        (ZYNQWR << 52) | ((uint64_t) reg << 44) |
        (ZYNQWR << 42) | ((uint64_t) (word & 0xFFU) << 34) |
        (ZYNQWR << 32) | ((uint64_t) (word >> 8) << 24);
    // writing high-order word triggers i/o, so write low-order first
    fpat[ZPI2CCMD+0] = cmd;
    fpat[ZPI2CCMD+1] = cmd >> 32;
    // give busy bit plenty of time to set, should take well under 100nS
    usleep (1);
    // wait for busy bit to clear, should take less than 12uS
    for (int i = 1000000; -- i >= 0;) {
        uint32_t sts = fpat[ZPI2CSTS+1];
        if (! (sts & (1U << 31))) {
            // make sure ack was received from mcp23017.v
            ////if (sts & (1U << 30)) {
            ////    fprintf (stderr, "I2CLib::write16: i2c I/O error\n");
            ////    ABORT ();
            ////}
            return;
        }
    }
    fprintf (stderr, "I2CLib::write16: i2c I/O timeout\n");
    ABORT ();
}
