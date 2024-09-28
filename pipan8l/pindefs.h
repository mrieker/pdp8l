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

#ifndef _PINDEFS_H
#define _PINDEFS_H

// pin voltages vs switch positions on actual PDP-8/L panel
//  DV2 : depo sw  : +5=normal    ; gnd=pressed
//  DT2 : step sw  : +5=up run    ; gnd=down step
//  DR2 : start sw : +5=normal    ; gnd=pressed
//  CF2 : sr<10>   : +5=up one    ; gnd=down zero
//  AC1 : ifld sw  : +5=down zero ; gnd=up one
//  AA1 : mprot sw : +5=down off  ; gnd=up on
// also, all lights are active low, ie, gnd turns the light on, +5 turns light off

#define P_AC03 0x00     // U1 A
#define P_MA01 0x01
#define P_MB01 0x02
#define P_EMA  0x03
#define P_LINK 0x04
#define P_SR04 0x05
#define P_DFLD 0x06
#define P_MPRT 0x07

#define P_SR03 0x08     // U1 B
#define P_SR02 0x09
#define P_SR01 0x0A
#define P_AC00 0x0B
#define P_MB00 0x0C
#define P_AC01 0x0D
#define P_MA00 0x0E
#define P_SR00 0x0F

#define P0_WMSK ((1U << P_SR03) | (1U << P_SR02) | (1U << P_SR01) | (1U << P_SR00) | (1U << P_DFLD) | (1U << P_MPRT) | (1U << P_SR04))
#define P0_WREV (                                                                    (1U << P_DFLD) | (1U << P_MPRT))
#define P0_RREV (P0_WMSK ^ P0_WREV ^ 0xFFFFU)

#define P_MB06 0x10     // U2 A
#define P_AC06 0x11
#define P_MB05 0x12
#define P_AC04 0x13
#define P_MA04 0x14
#define P_MA03 0x15
#define P_AC02 0x16
#define P_IFLD 0x17

#define P_MB02 0x18     // U2 B
#define P_MA02 0x19
#define P_MB03 0x1A
#define P_MB04 0x1B
#define P_AC05 0x1C
#define P_MA05 0x1D
#define P_AC07 0x1E
#define P_SR10 0x1F

#define P1_WMSK ((1U << (P_SR10 - 0x10)) | (1U << (P_IFLD - 0x10)))
#define P1_WREV (                          (1U << (P_IFLD - 0x10)))
#define P1_RREV (P1_WMSK ^ P1_WREV ^ 0xFFFFU)

#define P_MA09 0x20     // U3 A
#define P_MB09 0x21
#define P_CD2  0x22
#define P_AC09 0x23
#define P_AC08 0x24
#define P_MA08 0x25
#define P_MB08 0x26
#define P_SR09 0x27

#define P_MA06 0x28     // U3 B
#define P_MA07 0x29
#define P_MB07 0x2A
#define P_SR08 0x2B
#define P_SR07 0x2C
#define P_SR06 0x2D
#define P_MB11 0x2E
#define P_SR05 0x2F

#define P2_WMSK ((1U << (P_SR08 - 0x20)) | (1U << (P_SR07 - 0x20)) | (1U << (P_SR06 - 0x20)) | (1U << (P_SR05 - 0x20)) | (1U << (P_SR09 - 0x20)))
#define P2_WREV 0
#define P2_RREV (P2_WMSK ^ P2_WREV ^ 0xFFFFU)

#define P_DB1  0x30     // U4 A
#define P_CU1  0x31
#define P_CM1  0x32
#define P_MB10 0x33
#define P_MA10 0x34
#define P_AC10 0x35
#define P_AC11 0x36
#define P_DEP  0x37

#define P_MA11 0x38     // U4 B
#define P_FET  0x39
#define P_IR00 0x3A
#define P_ION  0x3B
#define P_PARE 0x3C
#define P_EXE  0x3D
#define P_IR01 0x3E
#define P_EXAM 0x3F

#define P3_WMSK ((1U << (P_EXAM - 0x30)) | (1U << (P_DEP - 0x30)))
#define P3_WREV ((1U << (P_EXAM - 0x30)) | (1U << (P_DEP - 0x30)))
#define P3_RREV (P3_WMSK ^ P3_WREV ^ 0xFFFFU)

#define P_STRT 0x40     // U5 A
#define P_STOP 0x41
#define P_BNCY 0x42
#define P_CAD  0x43
#define P_DJ1  0x44
#define P_DH1  0x45
#define P_DEF  0x46
#define P_STEP 0x47

#define P_IR02 0x48     // U5 B
#define P_PRTE 0x49
#define P_RUN  0x4A
#define P_WCT  0x4B
#define P_BRK  0x4C
#define P_SR11 0x4D
#define P_CONT 0x4E
#define P_LDAD 0x4F

#define P4_WMSK ((1U << (P_SR11 - 0x40)) | (1U << (P_CONT - 0x40)) | (1U << (P_LDAD - 0x40)) | (1U << (P_STRT - 0x40)) | (1U << (P_STOP - 0x40)) | (1U << (P_BNCY - 0x40)) | (1U << (P_STEP - 0x40)))
#define P4_WREV (                          (1U << (P_CONT - 0x40)) | (1U << (P_LDAD - 0x40)) | (1U << (P_STRT - 0x40)) | (1U << (P_STOP - 0x40))                           | (1U << (P_STEP - 0x40)))
#define P4_RREV (P4_WMSK ^ P4_WREV ^ 0xFFFFU)

#define P_NU16S 5

#endif
