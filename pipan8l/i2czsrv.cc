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

// Runs on RasPI plugged into PCB3 that is piggy-backed onto backplane for front panel signals.
// Acts as UDP server so i2czlib.cc can access the MCP23017s that read/write the front panel signals.

// Must have this enabled in /boot/config.txt: dtparam=i2c_arm=on
// Must enable /dev/i2c-1 with raspi-config, should see /dev/i2c-1

/*
  https://github.com/fivdi/i2c-bus/blob/master/doc/raspberry-pi-i2c.md
    add following to /boot/config.txt           "maybe this is hardware i2c"
      dtparam=i2c_arm=on
      dtparam=i2c_arm_baudrate=400000
    add following to /etc/modules
      i2c-dev
*/

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#include "i2czlib.h"

#define ABORT() do { fprintf (stderr, "abort() %s:%d\n", __FILE__, __LINE__); abort (); } while (0)

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

struct Client {
    Client *next;
    uint64_t lastseq;
    uint32_t ipaddr;
    uint16_t portno;
};

static uint16_t read16 (int i2cfd, uint8_t addr, uint8_t reg);
static void write16 (int i2cfd, uint8_t addr, uint8_t reg, uint16_t word);

int main ()
{
    // access the i2c bus
    int i2cfd = open ("/dev/i2c-1", O_RDWR);
    if (i2cfd < 0) {
        fprintf (stderr, "error opening /dev/i2c-1: %m\n");
        ABORT ();
    }

    // set up socket to receive requests on
    int udpfd = socket (AF_INET, SOCK_DGRAM, 0);
    if (udpfd < 0) {
        fprintf (stderr, "socket error: %m\n");
        ABORT ();
    }
    int one = 1;
    if (setsockopt (udpfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one) < 0) {
        ABORT ();
    }

    struct sockaddr_in servaddr;
    memset (&servaddr, 0, sizeof servaddr);
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons (I2CZUDPPORT);
    if (bind (udpfd, (sockaddr *)&servaddr, sizeof servaddr) < 0) {
        fprintf (stderr, "bind %d error: %m\n", I2CZUDPPORT);
        ABORT ();
    }

    printf ("listening on %d\n", I2CZUDPPORT);

    Client *clients = NULL;

    while (true) {

        // receive request
        I2CZUDPPkt udppkt;
        socklen_t addrlen = sizeof servaddr;
        int rc = recvfrom (udpfd, &udppkt, sizeof udppkt, 0, (sockaddr *)&servaddr, &addrlen);
        if (rc != (int) sizeof udppkt) {
            if (rc < 0) fprintf (stderr, "error receiving: %m\n");
            else if (rc > 0) fprintf (stderr, "only received %d of %d bytes\n", rc, (int) sizeof udppkt);
            continue;
        }

        // find client
        Client *client;
        for (client = clients; client != NULL; client = client->next) {
            if ((servaddr.sin_port == client->portno) && (servaddr.sin_addr.s_addr == client->ipaddr)) break;
        }
        if (client == NULL) {
            client = (Client *) malloc (sizeof *client);
            client->next    = clients;
            client->lastseq = 0;
            client->ipaddr  = servaddr.sin_addr.s_addr;
            client->portno  = servaddr.sin_port;
            clients = client;
        }

        // skip stale packets
        // it is ok to re-process last one received (maybe reply got lost)
        if (client->lastseq > udppkt.seq) continue;
        client->lastseq = udppkt.seq;

        // process command
        switch (udppkt.cmd) {

            // read MCP23017s actual pin values
            case 1: {
                for (int i = 0; i < 4; i ++) {
                    udppkt.dirs[i] = read16 (i2cfd, I2CBA + i, IODIRA);
                    udppkt.vals[i] = read16 (i2cfd, I2CBA + i, GPIOA);
                }
                break;
            }

            // read MCP23017s output latch values
            case 2: {
                for (int i = 0; i < 4; i ++) {
                    udppkt.dirs[i] = read16 (i2cfd, I2CBA + i, IODIRA);
                    udppkt.vals[i] = read16 (i2cfd, I2CBA + i, OLATA);
                }
                break;
            }

            // write MCP23017s output latch values
            case 3: {
                for (int i = 0; i < 4; i ++) {
                    write16 (i2cfd, I2CBA + i, IODIRA, udppkt.dirs[i]);
                    write16 (i2cfd, I2CBA + i, OLATA,  udppkt.vals[i]);
                }
                break;
            }

            default: ABORT ();
        }

        // send reply
        rc = sendto (udpfd, &udppkt, sizeof udppkt, 0, (sockaddr *)&servaddr, addrlen);
        if (rc != (int) sizeof udppkt) {
            if (rc < 0) fprintf (stderr, "error sending: %m\n");
            else if (rc > 0) fprintf (stderr, "only sent %d of %d bytes\n", rc, (int) sizeof udppkt);
        }
    }
}

// https://github.com/ve3wwg/raspberry_pi/blob/master/mcp23017/i2c_funcs.c

// read 16-bit value from MCP23017 on I2C bus
//  input:
//   addr = MCP23017 address
//   reg = starting register within MCP23017
//  output:
//   returns [07:00] = reg+0 contents
//           [15:08] = reg+1 contents
static uint16_t read16 (int i2cfd, uint8_t addr, uint8_t reg)
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
            if (retry > 0) fprintf (stderr, "read16: reading recovered\n");
            // reg+1 in upper byte; reg+0 in lower byte
            return (rbuf[1] << 8) | rbuf[0];
        }

        fprintf (stderr, "read16: error reading from %02X.%02X: %m\n", addr, reg);
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
static void write16 (int i2cfd, uint8_t addr, uint8_t reg, uint16_t word)
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
        ABORT ();
    }
}
