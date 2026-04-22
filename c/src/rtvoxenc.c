#include "rtvox.h"

#include <stdio.h>

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "Usage: rtvoxenc input.wav output.rtvx\n");
        return 1;
    }

    if (!rtvox_encode_wav_to_file(argv[1], argv[2])) {
        return 1;
    }

    printf("Encoded %s -> %s\n", argv[1], argv[2]);
    return 0;
}
