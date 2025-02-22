
// strip IFNDEF WOW <...> from music.pal
// leaves the ... part in

// cc -Wall -o nonwow nonwow.c

#include <stdio.h>
#include <string.h>

int main (int argc, char **argv)
{
    char c, line[256], *p;
    int brackets;

    char const *ifstuff = "IFNDEF WOW";
    if (argc > 1) ifstuff = argv[1];

    while (fgets (line, sizeof line, stdin) != NULL) {
        p = strstr (line, ifstuff);
        if (p != NULL) {
            *(p ++) = 0;
            fputs (line, stdout);
            brackets = 0;
            do {
                while ((c = *(p ++)) != 0) {
                    if ((c == '>') && (-- brackets <= 0)) goto endnwow;
                    if (brackets > 0) fputc (c, stdout);
                    if (c == '<') brackets ++;
                }
                p = line;
            } while (fgets (line, sizeof line, stdin) != NULL);
            break;
        endnwow:;
            fputs (p, stdout);
        } else {
            fputs (line, stdout);
        }
    }

    return 0;
}
