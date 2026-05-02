/* ============================================================
 *  os.c — OS-level file operations 
 * ============================================================ */
#include "os.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------ */
/*  Helpers                                                      */
/* ------------------------------------------------------------ */

const char *filename_from_path(const char *path) {
    const char *slash     = strrchr(path, '/'); // read from right to find last slash
    const char *backslash = strrchr(path, '\\'); // also check for backslash for Windows paths
    const char *last      = slash > backslash ? slash : backslash; // get the last separator
    return last ? last + 1 : path; // return the part after the last separator, or the whole path if no separator found
}

/* ------------------------------------------------------------ */
/*  Commands                                                     */
/* ------------------------------------------------------------ */

int cmd_cpy(const char *src, const char *dest_dir) {
    const char *fname = filename_from_path(src);

    /* Build destination path */
    size_t dlen          = strlen(dest_dir);
    int    has_sep       = dlen > 0 && (dest_dir[dlen - 1] == '/' || dest_dir[dlen - 1] == '\\');
    size_t dest_path_len = dlen + 1 + strlen(fname) + 1;

    char *dest_path = malloc(dest_path_len);
    if (!dest_path) {
        fprintf(stderr, "hiss: out of memory\n");
        return 1;
    }

    if (has_sep)
        snprintf(dest_path, dest_path_len, "%s%s",   dest_dir, fname);
    else
        snprintf(dest_path, dest_path_len, "%s%c%s", dest_dir, PATH_SEP, fname);

    /* Open source */
    FILE *in = fopen(src, "rb");
    if (!in) {
        fprintf(stderr, "hiss: cannot open '%s'\n", src);
        free(dest_path);
        return 1;
    }

    /* Open destination */
    FILE *out = fopen(dest_path, "wb");
    if (!out) {
        fprintf(stderr, "hiss: cannot write to '%s'\n", dest_path);
        fclose(in);
        free(dest_path);
        return 1;
    }

    /* Copy */
    char   buf[65536];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        if (fwrite(buf, 1, n, out) != n) {
            fprintf(stderr, "hiss: write error\n");
            fclose(in);
            fclose(out);
            free(dest_path);
            return 1;
        }
    }

    fclose(in);
    fclose(out);
    printf("copied '%s' -> '%s'\n", src, dest_path);
    free(dest_path);
    return 0;
}
