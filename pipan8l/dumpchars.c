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

// simple test program to dump characters read from stdin without buffering

// cc -o dumpchars dumpchars.c
// ./dumpchars < inputdevice

#include <errno.h>
#include <stdio.h>
#include <unistd.h>

int main ()
{
    char ch;
    int rc;

    setlinebuf (stdout);

    while ((rc = read (STDIN_FILENO, &ch, 1)) > 0) {
        printf ("ch=%03o <%c>\n", ch, (((ch < 32) || (ch > 126)) ? '.' : ch));
    }
    printf ("rc=%d errno=%d\n", rc, errno);
    return 0;
}
