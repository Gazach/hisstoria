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
    printf("  hiss list   [path]\n");
    printf("  hiss ls     [path]\n");
    printf("  hiss listdir [path]\n");
    printf("  hiss lsdir   [path]\n");
    printf("  hiss read    <file>\n");
    printf("  hiss r       <file>\n");
    printf("  hiss write   <file> <content...>\n");
    printf("  hiss w       <file> <content...>\n");
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

    // list command - lists files and folders in a directory
    if (strcmp(argv[1], "list") == 0 || strcmp(argv[1], "ls") == 0) {
        const char *target = (argc >= 3) ? argv[2] : ".";
        return cmd_list(target);
    }

    // listdir command - lists only subdirectories
    if (strcmp(argv[1], "listdir") == 0 || strcmp(argv[1], "lsdir") == 0) {
        const char *target = (argc >= 3) ? argv[2] : ".";
        return cmd_listdir(target);
    }

    // read command - prints file contents to stdout
    if (strcmp(argv[1], "read") == 0 || strcmp(argv[1], "r") == 0) {
        if (argc != 3) {
            fprintf(stderr, "Usage: hiss read <file>\n");
            return 1;
        }
        return cmd_read(argv[2]);
    }

    // write command - overwrites a file with the given content
    if (strcmp(argv[1], "write") == 0 || strcmp(argv[1], "w") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Usage: hiss write <file> <content...>\n");
            return 1;
        }
        return cmd_write(argv[2], argc - 3, &argv[3]);
    }
    

    fprintf(stderr, "hiss: unknown command '%s'\n", argv[1]);
    return 1;
}
