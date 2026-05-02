/* ============================================================
 *  os.c — OS-level file operations 
 * ============================================================ */
#include "os.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Platform-specific includes for directory operations */
#ifdef _WIN32
#   include <windows.h>
#else
#   include <dirent.h>
#   include <sys/stat.h>
#   include <unistd.h>
#   include <errno.h>
#endif

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
/*  Internal helpers                                             */
/* ------------------------------------------------------------ */

/* Copies src to dest — both are full file paths */
static int copy_file(const char *src, const char *dest) {
    FILE *in = fopen(src, "rb");
    if (!in) {
        fprintf(stderr, "hiss: cannot open '%s'\n", src);
        return 1;
    }

    FILE *out = fopen(dest, "wb");
    if (!out) {
        fprintf(stderr, "hiss: cannot write to '%s'\n", dest);
        fclose(in);
        return 1;
    }

    char   buf[65536];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        if (fwrite(buf, 1, n, out) != n) {
            fprintf(stderr, "hiss: write error\n");
            fclose(in);
            fclose(out);
            return 1;
        }
    }

    fclose(in);
    fclose(out);
    return 0;
}

/* Joins base and name into a malloc'd path string: "base/name" */
static char *join_path(const char *base, const char *name) {
    size_t blen    = strlen(base);
    int    has_sep = blen > 0 && (base[blen - 1] == '/' || base[blen - 1] == '\\');
    size_t len     = blen + 1 + strlen(name) + 1;

    char *out = malloc(len);
    if (!out) return NULL;

    if (has_sep)
        snprintf(out, len, "%s%s",   base, name);
    else
        snprintf(out, len, "%s%c%s", base, PATH_SEP, name);

    return out;
}

/* Creates a single directory; succeeds silently if it already exists */
static int make_dir(const char *path) {
#ifdef _WIN32
    if (CreateDirectoryA(path, NULL)) return 0;
    return (GetLastError() == ERROR_ALREADY_EXISTS) ? 0 : -1;
#else
    if (mkdir(path, 0755) == 0) return 0;
    return (errno == EEXIST) ? 0 : -1;
#endif
}

/* ------------------------------------------------------------ */
/*  Directory internals                                          */
/* ------------------------------------------------------------ */

/* Recursively copies src directory tree into dest (dest is the full target path) */
static int copy_dir_recursive(const char *src, const char *dest) {
    if (make_dir(dest) != 0) {
        fprintf(stderr, "hiss: cannot create directory '%s'\n", dest);
        return 1;
    }

#ifdef _WIN32
    size_t pat_len = strlen(src) + 3;
    char  *pattern = malloc(pat_len);
    if (!pattern) { fprintf(stderr, "hiss: out of memory\n"); return 1; }
    snprintf(pattern, pat_len, "%s\\*", src);

    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern, &fd);
    free(pattern);

    if (h == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "hiss: cannot read directory '%s'\n", src);
        return 1;
    }

    int result = 0;
    do {
        if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0)
            continue;

        char *src_child  = join_path(src,  fd.cFileName);
        char *dest_child = join_path(dest, fd.cFileName);
        if (!src_child || !dest_child) {
            fprintf(stderr, "hiss: out of memory\n");
            free(src_child); free(dest_child);
            result = 1; break;
        }

        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            result = copy_dir_recursive(src_child, dest_child);
        else
            result = copy_file(src_child, dest_child);

        free(src_child);
        free(dest_child);
    } while (result == 0 && FindNextFileA(h, &fd));

    FindClose(h);
    return result;
#else
    DIR *d = opendir(src);
    if (!d) {
        fprintf(stderr, "hiss: cannot read directory '%s'\n", src);
        return 1;
    }

    struct dirent *entry;
    int result = 0;
    while (result == 0 && (entry = readdir(d)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        char *src_child  = join_path(src,  entry->d_name);
        char *dest_child = join_path(dest, entry->d_name);
        if (!src_child || !dest_child) {
            fprintf(stderr, "hiss: out of memory\n");
            free(src_child); free(dest_child);
            result = 1; break;
        }

        struct stat st;
        if (stat(src_child, &st) == 0 && S_ISDIR(st.st_mode))
            result = copy_dir_recursive(src_child, dest_child);
        else
            result = copy_file(src_child, dest_child);

        free(src_child);
        free(dest_child);
    }

    closedir(d);
    return result;
#endif
}

/* Recursively deletes a directory and all its contents */
static int remove_dir_recursive(const char *path) {
#ifdef _WIN32
    size_t pat_len = strlen(path) + 3;
    char  *pattern = malloc(pat_len);
    if (!pattern) { fprintf(stderr, "hiss: out of memory\n"); return 1; }
    snprintf(pattern, pat_len, "%s\\*", path);

    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern, &fd);
    free(pattern);

    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0)
                continue;

            char *child = join_path(path, fd.cFileName);
            if (!child) { FindClose(h); return 1; }

            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                remove_dir_recursive(child);
            else
                DeleteFileA(child);

            free(child);
        } while (FindNextFileA(h, &fd));
        FindClose(h);
    }

    if (!RemoveDirectoryA(path)) {
        fprintf(stderr, "hiss: cannot remove directory '%s'\n", path);
        return 1;
    }
    return 0;
#else
    DIR *d = opendir(path);
    if (d) {
        struct dirent *entry;
        while ((entry = readdir(d)) != NULL) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                continue;

            char *child = join_path(path, entry->d_name);
            if (!child) { closedir(d); return 1; }

            struct stat st;
            if (stat(child, &st) == 0 && S_ISDIR(st.st_mode))
                remove_dir_recursive(child);
            else
                unlink(child);

            free(child);
        }
        closedir(d);
    }

    if (rmdir(path) != 0) {
        fprintf(stderr, "hiss: cannot remove directory '%s'\n", path);
        return 1;
    }
    return 0;
#endif
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

    int result = copy_file(src, dest_path);
    if (result == 0)
        printf("copied '%s' -> '%s'\n", src, dest_path);
    free(dest_path);
    return result;
}

int cmd_cut(const char *src, const char *dest_dir) {
    /* Copy first — if that fails, don't delete the source */
    int result = cmd_cpy(src, dest_dir);
    if (result != 0)
        return result;

    /* Delete source */
    if (remove(src) != 0) {
        fprintf(stderr, "hiss: could not delete '%s' after move\n", src);
        return 1;
    }

    return 0;
}

int cmd_cpydir(const char *src_dir, const char *dest_parent) {
    const char *dname    = filename_from_path(src_dir);
    char       *dest_dir = join_path(dest_parent, dname);
    if (!dest_dir) {
        fprintf(stderr, "hiss: out of memory\n");
        return 1;
    }

    int result = copy_dir_recursive(src_dir, dest_dir);
    if (result == 0)
        printf("copied dir '%s' -> '%s'\n", src_dir, dest_dir);
    free(dest_dir);
    return result;
}

int cmd_cutdir(const char *src_dir, const char *dest_parent) {
    /* Copy first — if that fails, don't delete the source */
    int result = cmd_cpydir(src_dir, dest_parent);
    if (result != 0)
        return result;

    /* Delete source directory */
    if (remove_dir_recursive(src_dir) != 0) {
        fprintf(stderr, "hiss: could not remove source directory '%s' after move\n", src_dir);
        return 1;
    }

    return 0;
}
