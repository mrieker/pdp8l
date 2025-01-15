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

// kernel module for raspi to enable 32-bit raspi timestamp counter

// https://stackoverflow.com/questions/31620375/arm-cortex-a7-returning-pmccntr-0-in-kernel-mode-and-ille

#include <linux/module.h>
#include <linux/kernel.h>

MODULE_LICENSE("GPL");

static void enableccntread (void *param)
{
    asm volatile ("mcr p15,0,%0,c9,c14,0" : : "r" (1));
    asm volatile ("mcr p15,0,%0,c9,c12,0" : : "r" (1));
    asm volatile ("mcr p15,0,%0,c9,c12,1" : : "r" (0x80000000));
}

int init_module ()
{
    on_each_cpu (enableccntread, NULL, 1);
    return 0;
}

void cleanup_module ()
{ }
