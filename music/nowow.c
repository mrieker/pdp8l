
// strip IFDEF WOW <...> from music.pal

// cc -Wall -o nowow nowow.c

#include <stdio.h>
#include <string.h>

int main ()
{
    char c, line[256], *p, *q;
    int brackets, comment;

    while (fgets (line, sizeof line, stdin) != NULL) {
        p = strstr (line, "IFDEF WOW");
        if (p != NULL) {
            *(p ++) = 0;
            fputs (line, stdout);
            brackets = 0;
            do {
                while ((c = *(p ++)) != 0) {
                    if (c == '<') brackets ++;
                    if ((c == '>') && (-- brackets <= 0)) goto endwow;
                }
                p = line;
            } while (fgets (line, sizeof line, stdin) != NULL);
            break;
        endwow:;
            comment = 0;
            for (q = p; (c = *q) != 0; q ++) {
                if (c == '\n') break;
                if (c == '/') comment = 1;
                if (! comment && (c > ' ')) break;
            }
            fputs (comment ? q : p, stdout);
        } else {
            fputs (line, stdout);
        }
    }

    return 0;
}
