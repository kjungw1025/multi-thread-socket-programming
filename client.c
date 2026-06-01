#include <stdio.h>

int main(int argc, char *argv[]) {

    if (argc != 4) {
        fprintf(stderr, "Usage: %s {IP} {PORT} {APTYPE}\n", argv[0]);
        return 1;
    }

    const char  *ip     = argv[1];
    int         port    = atoi(argv[2]);
    const char  *aptype = argv[3];
}