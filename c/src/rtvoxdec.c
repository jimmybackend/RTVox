#include "rtvox.h"

#include <stdio.h>

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "Usage: rtvoxdec input.rtvx output.wav\n");
        return 1;
    }

    if (!rtvox_decode_file_to_wav(argv[1], argv[2])) {
        return 1;
    }

    printf("Decoded %s -> %s\n", argv[1], argv[2]);
    return 0;
}
