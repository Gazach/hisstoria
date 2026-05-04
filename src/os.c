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

/*
 * filename_from_path — extracts the filename component from a full path.
 *
 * Scans the path string from right-to-left looking for the last '/' (POSIX)
 * or '\\' (Windows) separator, then returns a pointer to the character that
 * follows it.  If no separator is found the whole path is treated as a
 * filename and returned unchanged.  The returned pointer points into the
 * original string, so no allocation takes place.
 */
const char *filename_from_path(const char *path) {
    const char *slash     = strrchr(path, '/'); // read from right to find last slash
    const char *backslash = strrchr(path, '\\'); // also check for backslash for Windows paths
    const char *last      = slash > backslash ? slash : backslash; // get the last separator
    return last ? last + 1 : path; // return the part after the last separator, or the whole path if no separator found
}

/* ------------------------------------------------------------ */
/*  Internal helpers                                             */
/* ------------------------------------------------------------ */

/*
 * copy_file — performs a binary copy of one file to another.
 *
 * Opens 'src' for reading and 'dest' for writing in binary mode, then pumps
 * data through a 64 KB stack buffer until the source is exhausted.  Any
 * read/write error causes an immediate return of 1; on success 0 is returned.
 * Both file handles are closed before returning regardless of outcome.
 */
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

/*
 * join_path — concatenates two path components into a newly allocated string.
 *
 * If 'base' already ends with a separator ('/' or '\\') the two components
 * are simply concatenated; otherwise the platform-specific PATH_SEP character
 * is inserted between them.  The caller is responsible for free()-ing the
 * returned buffer.  Returns NULL if malloc fails.
 */
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

/*
 * make_dir — creates a single directory at 'path'.
 *
 * On Windows it calls CreateDirectoryA; on POSIX it calls mkdir with
 * permissions 0755.  If the directory already exists the function returns 0
 * (success) rather than an error so callers do not need to pre-check
 * existence.  Returns 0 on success, -1 on any other failure.
 */
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

/*
 * copy_dir_recursive — deep-copies an entire directory tree.
 *
 * First creates 'dest' via make_dir, then iterates every entry inside 'src':
 *   - Sub-directories are handled with a recursive call.
 *   - Regular files are copied with copy_file.
 * On Windows the Win32 FindFirstFile/FindNextFile API is used; on POSIX
 * opendir/readdir together with stat() are used to distinguish files from
 * directories.  Dot-entries ("." and "..") are skipped in both cases.
 * Returns 0 on full success, 1 on the first error encountered.
 */
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

/*
 * remove_dir_recursive — deletes a directory and everything inside it.
 *
 * Iterates the directory contents, recursing into sub-directories and
 * deleting regular files directly (DeleteFileA on Windows, unlink on POSIX).
 * After all children have been removed the directory itself is deleted
 * (RemoveDirectoryA / rmdir).  Returns 0 on success, 1 if the final
 * directory removal fails.
 */
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

/*
 * cmd_cpy — copies a single file into a destination directory.
 *
 * Extracts the filename from 'src' with filename_from_path, builds the full
 * destination path by joining 'dest_dir' and the filename, then delegates to
 * copy_file.  On success prints a "copied" message to stdout.  The
 * destination directory must already exist; this function does not create it.
 * Returns 0 on success, 1 on failure.
 */
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

/*
 * cmd_cut — moves a single file into a destination directory.
 *
 * Implements move as copy-then-delete: first calls cmd_cpy to copy the file;
 * if that succeeds the source file is removed with remove().  If the copy
 * fails the source is left untouched.  Returns 0 on success, 1 on failure.
 */
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

/*
 * cmd_cpydir — copies an entire directory tree under a destination parent.
 *
 * Extracts the directory name from 'src_dir', joins it with 'dest_parent' to
 * form the full target path, then calls copy_dir_recursive to perform the
 * deep copy.  On success prints a "copied dir" message to stdout.
 * Returns 0 on success, 1 on failure.
 */
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

/*
 * cmd_cutdir — moves an entire directory tree under a destination parent.
 *
 * Implements move as copy-then-delete: first calls cmd_cpydir to deep-copy
 * the directory; if that succeeds remove_dir_recursive deletes the original
 * source tree.  If the copy fails the source is left untouched.
 * Returns 0 on success, 1 on failure.
 */
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

/* ------------------------------------------------------------ */
/*  Size formatting                                              */
/* ------------------------------------------------------------ */

/*
 * format_size — converts a byte count into a human-readable string.
 *
 * Repeatedly divides 'bytes' by 1024 while the value is >= 1024 and a
 * higher unit (KB, MB, GB, TB) is available.  The result is written into
 * the caller-supplied buffer 'out' of size 'out_len'.  Whole-byte counts
 * are formatted as integers (e.g. "512 B"); larger values are formatted
 * with one decimal place (e.g. "1.5 MB").
 */
static void format_size(unsigned long long bytes, char *out, size_t out_len) {
    const char *units[] = { "B", "KB", "MB", "GB", "TB" };
    int unit = 0;
    double val = (double)bytes;

    while (val >= 1024.0 && unit < 4) {
        val  /= 1024.0;
        unit++;
    }

    if (unit == 0)
        snprintf(out, out_len, "%llu B", bytes);
    else
        snprintf(out, out_len, "%.1f %s", val, units[unit]);
}

/* ------------------------------------------------------------ */
/*  List command                                                  */
/* ------------------------------------------------------------ */

/*
 * cmd_list — lists all entries (files and directories) in a directory.
 *
 * Iterates every entry in 'path' (defaults to "." when NULL or empty) and
 * prints each name left-aligned in a 40-character field.  Directories are
 * annotated with "<DIR>"; regular files show a human-readable size produced
 * by format_size.  Uses Win32 FindFirstFile/FindNextFile on Windows and
 * opendir/readdir + stat on POSIX.  Returns 0 on success, 1 on failure.
 */
int cmd_list(const char *path) {
    /* Default to current directory */
    if (!path || path[0] == '\0') path = ".";

#ifdef _WIN32
    size_t pat_len = strlen(path) + 3;
    char  *pattern = malloc(pat_len);
    if (!pattern) { fprintf(stderr, "hiss: out of memory\n"); return 1; }
    snprintf(pattern, pat_len, "%s\\*", path);

    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern, &fd);
    free(pattern);

    if (h == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "hiss: cannot open directory '%s'\n", path);
        return 1;
    }

    do {
        if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0)
            continue;

        int is_dir = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;

        if (is_dir) {
            printf("%-40s  <DIR>\n", fd.cFileName);
        } else {
            unsigned long long size =
                ((unsigned long long)fd.nFileSizeHigh << 32) | fd.nFileSizeLow;
            char size_buf[32];
            format_size(size, size_buf, sizeof(size_buf));
            printf("%-40s  %s\n", fd.cFileName, size_buf);
        }
    } while (FindNextFileA(h, &fd));

    FindClose(h);
    return 0;
#else
    DIR *d = opendir(path);
    if (!d) {
        fprintf(stderr, "hiss: cannot open directory '%s'\n", path);
        return 1;
    }

    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        char *child = join_path(path, entry->d_name);
        if (!child) { closedir(d); fprintf(stderr, "hiss: out of memory\n"); return 1; }

        struct stat st;
        if (stat(child, &st) != 0) {
            printf("%-40s  ?\n", entry->d_name);
            free(child);
            continue;
        }

        if (S_ISDIR(st.st_mode)) {
            printf("%-40s  <DIR>\n", entry->d_name);
        } else {
            char size_buf[32];
            format_size((unsigned long long)st.st_size, size_buf, sizeof(size_buf));
            printf("%-40s  %s\n", entry->d_name, size_buf);
        }

        free(child);
    }

    closedir(d);
    return 0;
#endif
}

/*
 * cmd_listdir — lists only the sub-directories inside a directory.
 *
 * Behaves like cmd_list but filters out regular files, printing only entries
 * that are directories (annotated with "<DIR>").  Useful for quickly seeing
 * the folder structure without the clutter of individual files.
 * Returns 0 on success, 1 on failure.
 */
int cmd_listdir(const char *path) {
    /* Default to current directory */
    if (!path || path[0] == '\0') path = ".";

#ifdef _WIN32
    size_t pat_len = strlen(path) + 3;
    char  *pattern = malloc(pat_len);
    if (!pattern) { fprintf(stderr, "hiss: out of memory\n"); return 1; }
    snprintf(pattern, pat_len, "%s\\*", path);

    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern, &fd);
    free(pattern);

    if (h == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "hiss: cannot open directory '%s'\n", path);
        return 1;
    }

    do {
        if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0)
            continue;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            printf("%-40s  <DIR>\n", fd.cFileName);
    } while (FindNextFileA(h, &fd));

    FindClose(h);
    return 0;
#else
    DIR *d = opendir(path);
    if (!d) {
        fprintf(stderr, "hiss: cannot open directory '%s'\n", path);
        return 1;
    }

    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        char *child = join_path(path, entry->d_name);
        if (!child) { closedir(d); fprintf(stderr, "hiss: out of memory\n"); return 1; }

        struct stat st;
        if (stat(child, &st) == 0 && S_ISDIR(st.st_mode))
            printf("%-40s  <DIR>\n", entry->d_name);

        free(child);
    }

    closedir(d);
    return 0;
#endif
}

/* ------------------------------------------------------------ */
/*  Read command                                                 */
/* ------------------------------------------------------------ */

/*
 * cmd_read — prints the contents of a text file to stdout.
 *
 * Opens 'filepath' in text mode and reads it line-by-line through a 4 KB
 * buffer, writing each chunk directly to stdout with fputs.  This effectively
 * replicates the behaviour of the Unix 'cat' command for a single file.
 * Returns 0 on success, 1 if the file cannot be opened.
 */
int cmd_read(const char *filepath) {
    FILE *f = fopen(filepath, "r");
    if (!f) {
        fprintf(stderr, "hiss: cannot open '%s'\n", filepath);
        return 1;
    }

    char buf[4096];
    while (fgets(buf, sizeof(buf), f))
        fputs(buf, stdout);

    fclose(f);
    return 0;
}

/* ------------------------------------------------------------ */
/*  Write command                                                */
/* ------------------------------------------------------------ */

/*
 * cmd_write — writes a sequence of arguments to a file as a single line.
 *
 * Opens 'filepath' for writing (truncating any existing content), then
 * iterates argv[0..argc-1] and writes each token separated by a single space.
 * The special tokens "/nextline" and "/nextlines" (optionally suffixed with
 * '!' to avoid PowerShell interference) insert a newline character instead
 * of text, allowing multi-line output from a single command invocation.
 * A trailing newline is always appended after the last token.  On success
 * prints a "written to" confirmation message.  Returns 0 on success, 1 if
 * the file cannot be opened.
 */
int cmd_write(const char *filepath, int argc, char *argv[]) {
    FILE *f = fopen(filepath, "w");
    if (!f) {
        fprintf(stderr, "hiss: cannot open '%s' for writing\n", filepath);
        return 1;
    }

    /* Write each argument separated by a space.
     * /nextline or /nextlines (with or without trailing !) inserts a newline.
     * Matching is done without the trailing ! so PowerShell can't interfere. */
    for (int i = 0; i < argc; i++) {
        /* Strip a trailing '!' for comparison purposes */
        size_t alen    = strlen(argv[i]);
        int    has_bang = alen > 0 && argv[i][alen - 1] == '!';
        char   token[64] = {0};
        if (has_bang && alen - 1 < sizeof(token)) {
            memcpy(token, argv[i], alen - 1);
        } else {
            strncpy(token, argv[i], sizeof(token) - 1);
        }

        int is_nl = (strcmp(token, "/nextline") == 0 || strcmp(token, "/nextlines") == 0);

        if (is_nl) {
            fputc('\n', f);
        } else {
            /* Don't add a leading space after a newline token */
            int prev_nl = 0;
            if (i > 0) {
                size_t plen = strlen(argv[i - 1]);
                int    pbang = plen > 0 && argv[i - 1][plen - 1] == '!';
                char   prev[64] = {0};
                if (pbang && plen - 1 < sizeof(prev))
                    memcpy(prev, argv[i - 1], plen - 1);
                else
                    strncpy(prev, argv[i - 1], sizeof(prev) - 1);
                prev_nl = (strcmp(prev, "/nextline") == 0 || strcmp(prev, "/nextlines") == 0);
            }
            if (i > 0 && !prev_nl)
                fputc(' ', f);
            fputs(argv[i], f);
        }
    }
    fputc('\n', f);

    fclose(f);
    printf("written to '%s'\n", filepath);
    return 0;
}
