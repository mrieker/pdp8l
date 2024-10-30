
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>

int main ()
{
    int i;
    uint32_t shadow[32768];

    int extmemfd = open ("/proc/zynqpdp8l", O_RDWR);
    if (extmemfd < 0) {
        fprintf (stderr, "error opening /proc/zynqpdp8l: %m\n");
        return -1;
    }

    void *extmemmap = mmap (NULL, 0x20000, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (extmemmap == MAP_FAILED) {
        fprintf (stderr, "error mapping 128K anonymous: %m\n");
        return -1;
    }

    for (int i = 0; i < 32; i ++) {
        long va = ((long) extmemmap) + (i * 4096);
        void *mp = mmap ((void *) va, 0x1000, PROT_READ | PROT_WRITE, MAP_FIXED | MAP_SHARED, extmemfd, 0x20000 + i * 4096);
        if (mp == MAP_FAILED) {
            fprintf (stderr, "error mapping page %u at %lX of /proc/zynqpdp8l: %m\n", i, va);
            return -1;
        }
        if ((long) mp != va) {
            fprintf (stderr, "page %u remapped at %lX originally %lX\n", (long) mp, va);
            return -1;
        }
    }

    uint32_t volatile *extmemptr = (uint32_t volatile *) extmemmap;

    srand (0);
    for (i = 0; i < 32768; i ++) {
        shadow[i] = rand () & 07777;
        extmemptr[i] = shadow[i];
    }
    for (i = 0; i < 32768; i ++) {
        uint32_t temp = extmemptr[i];
        if (temp != shadow[i]) {
            printf ("%05o was %04o expected %04o\n", i, temp, shadow[i]);
        }
    }
    return 0;
}
