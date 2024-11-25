
#include <stdio.h>
#include <stdlib.h>

int main (int argc, char **argv)
{
    if (argc == 2) {
        double x = strtod (argv[1], NULL);
        printf ("%8.4f\n", ((int) (x * 80 + 0.5)) / 80.0);
    }
    return 0;
}
