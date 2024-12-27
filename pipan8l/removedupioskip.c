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

// Remove redundant IOSKIP/JMP.-1 instructions from z8lmctrace output

//  cc -O2 -Wall -o removedupioskip removedupioskip.c

//  ./z8lmctrace | removedupioskip

#include <stdio.h>
#include <string.h>

int main ()
{
    int indotdotdot = 0;
    char line1[80], line2[80], line3[80], line4[80], line5[80], line6[80];
    line1[0] = line2[0] = line3[0] = line4[0] = line5[0] = line6[0] = 0;

    setlinebuf (stdout);

    do {
        // invariant:
        //  six lines read into line1..line6 including \n terminators
        //  bof/eof lines are null without \n terminator
        //  first two have been printed

        // check for IOT/JMP/IOT/JMP/IOT/JMP
        // ...with all IOTs and all JMPs the same
        if ((strstr (line3, " IOT ") != NULL) &&
            (strstr (line4, " JMP ") != NULL) &&
            (strcmp (line3 + 8, line1 + 8) == 0) &&
            (strcmp (line4 + 8, line2 + 8) == 0) &&
            (strcmp (line3 + 8, line5 + 8) == 0) &&
            (strcmp (line4 + 8, line6 + 8) == 0)) {

            // all match, print two lines of ... for lines 3 and 4
            if (! indotdotdot) {
                indotdotdot = 1;
                fputs ("    ...\n", stdout);
                fputs ("    ...\n", stdout);
            }

            // now that 3,4 are printed as ...s, shift lines 3,4,5,6 up over 1,2,3,4
            // don't bother, they are the same

            // fill lines in the bottom
            if (fgets (line5, sizeof line5, stdin) == NULL) line5[0] = 0;
            if (fgets (line6, sizeof line6, stdin) == NULL) line6[0] = 0;
        } else {
            indotdotdot = 0;

            // no match, print unprinted line
            fputs (line3, stdout);

            // now that 3 is printed, shift lines 2,3,4,5,6 up over 1,2,3,4,5
            strcpy (line1, line2);
            strcpy (line2, line3);
            strcpy (line3, line4);
            strcpy (line4, line5);
            strcpy (line5, line6);

            // fill line in the bottom
            if (fgets (line6, sizeof line6, stdin) == NULL) line6[0] = 0;
        }
    } while ((line1[0] | line2[0] | line3[0] | line4[0] | line5[0] | line6[0]) != 0);
    return 0;
}
