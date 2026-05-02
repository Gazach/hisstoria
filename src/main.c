/* ============================================================
 *  hisstoria - a simple command-line utility for file operations cuz why not
 * ============================================================ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "os.h"

/* ------------------------------------------------------------ */
/*  Usage                                                        */
/* ------------------------------------------------------------ */

static void print_usage(void) {
    printf("Usage:\n");
    printf("  hiss cpy    <source>    <destination>\n");
    printf("  hiss cut    <source>    <destination>\n");
    printf("  hiss cpydir <source>    <destination>\n");
    printf("  hiss cutdir <source>    <destination>\n");
}

/* ------------------------------------------------------------ */
/*  Code Entry point                                            */
/* ------------------------------------------------------------ */

int main(int argc, char *argv[]) {

    // check if no args provided
    if (argc < 2) {
        print_usage();
        return 0;
    }

    // handle commands (Basic command)

    // copy command - copies a file to a destination directory
    if (strcmp(argv[1], "cpy") == 0) {
        if (argc != 4) {
            fprintf(stderr, "Usage: hiss cpy <source> <destination>\n");
            return 1;
        }
        return cmd_cpy(argv[2], argv[3]);
    }

    // cut command - moves a file to a destination directory
    if (strcmp(argv[1], "cut") == 0) {
        if (argc != 4) {
            fprintf(stderr, "Usage: hiss cut <source> <destination>\n");
            return 1;
        }
        return cmd_cut(argv[2], argv[3]);
    }

    // cpydir command - recursively copies a directory
    if (strcmp(argv[1], "cpydir") == 0 || strcmp(argv[1], "cpy-dir") == 0) {
        if (argc != 4) {
            fprintf(stderr, "Usage: hiss cpydir <source> <destination>\n");
            return 1;
        }
        return cmd_cpydir(argv[2], argv[3]);
    }

    // cutdir command - moves a directory to a destination
    if (strcmp(argv[1], "cutdir") == 0 || strcmp(argv[1], "cut-dir") == 0) {
        if (argc != 4) {
            fprintf(stderr, "Usage: hiss cutdir <source> <destination>\n");
            return 1;
        }
        return cmd_cutdir(argv[2], argv[3]);
    }
    

    fprintf(stderr, "hiss: unknown command '%s'\n", argv[1]);
    return 1;
}
