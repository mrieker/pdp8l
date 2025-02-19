
#include <math.h>
#include <stdio.h>

int main ()
{
    for (int i = 0; i < 13; i ++) {
        double f = 220 * pow (2.0, i / 12.0);
        printf ("%6.1f\n", f);
    }
    return 0;
}
