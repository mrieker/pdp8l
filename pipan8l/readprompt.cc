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

// read with prompt using readline

#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

#include <readline/history.h>
#include <readline/readline.h>

#include "readprompt.h"

char const *readprompt (char const *prompt)
{
    fflush (stdout);
    fflush (stderr);

    static char *lastline = NULL;
    static int initted = 0;

    // if stdin not a tty, just read next line from stdin and echo to stdout
    if (! isatty (STDIN_FILENO)) {
        if (lastline != NULL) free (lastline);
        int linelen = 256;
        int lineofs = -1;
        lastline = (char *) malloc (linelen);
        do {
            if (linelen - ++ lineofs < 2) {
                linelen += linelen / 2;
                lastline = (char *) realloc (lastline, linelen);
            }
            if (lastline == NULL) abort ();
            int ll;
            if ((fgets (lastline + lineofs, linelen - lineofs, stdin) == NULL) || ((ll = strlen (lastline + lineofs)) <= 0)) {
                if (lineofs > 0) break;
                free (lastline);
                lastline = NULL;
                printf ("%s^D\n", prompt);
                return NULL;
            }
            lineofs += ll;
        } while (lastline[--lineofs] != '\n');
        lastline[lineofs] = 0;
        printf ("%s%s\n", prompt, lastline);
        return lastline;
    }

    // stdin is a tty, use libreadline
    char *expansion, *line;
    int result;

    if (! initted) {
        using_history ();
        stifle_history (500);
        history_expansion_char = 0;
        rl_bind_key ('\t', rl_insert);
        initted = 1;

        // if stdout is not a tty, use stdin's tty for prompting and echo to stdout
        if (! isatty (STDOUT_FILENO)) {
            char stdinfn[20];
            snprintf (stdinfn, sizeof stdinfn, "/proc/self/fd/%d", STDIN_FILENO);
            rl_outstream = fopen (stdinfn, "w");
            if (rl_outstream == NULL) {
                fprintf (stderr, "readprompt: error opening %s: %m\n", stdinfn);
            } else {
                initted = -1;
                setlinebuf (rl_outstream);
            }
        }
    }

    // suppress control-C while reading
    struct termios oldattrs;
    if (tcgetattr (STDIN_FILENO, &oldattrs) < 0) abort ();
    struct termios newattrs = oldattrs;
    newattrs.c_cc[VINTR] = 0;
    if (tcsetattr (STDIN_FILENO, TCSANOW, &newattrs) < 0) abort ();

readit:
    if (initted < 0) usleep (333333);  // hopefully enough time for tee to catch up displaying everything sent to it
    line = readline (prompt);
    FILE *echoto = (rl_outstream == NULL) ? stdout : rl_outstream;
    if (line == NULL) {
        fputs ("^D\n", echoto);
        if (initted < 0) printf ("\377%s^D\n", prompt);
    } else {
        if ((line[0] != 0) && ((lastline == NULL) || (strcmp (line, lastline) != 0))) {
            result = history_expand (line, &expansion);
            if (result != 0) {
                fprintf (echoto, "%s\n", expansion);
            }
            if ((result < 0) || (result == 2)) {
                free (expansion);
                goto readit;
            }
            add_history (expansion);
            free (line);
            line = expansion;
        }
        if (initted < 0) printf ("\377%s%s\n", prompt, line);
    }
    if (lastline != NULL) free (lastline);
    lastline = line;
    if (tcsetattr (STDIN_FILENO, TCSANOW, &oldattrs) < 0) abort ();
    return lastline;
}
