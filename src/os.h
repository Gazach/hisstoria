/* ============================================================
 *  os.h — OS-level file operations
 * ============================================================ */

#ifndef OS_H
#define OS_H

/* ------------------------------------------------------------ */
/*  Platform                                                     */
/* ------------------------------------------------------------ */

#ifdef _WIN32
    #define PATH_SEP '\\'
#else
    #define PATH_SEP '/'
#endif


/* Returns a pointer to the filename portion of a path */
const char *filename_from_path(const char *path);

/* Copies src file into dest_dir, preserving the filename.
 * Returns 0 on success, 1 on error. */
int cmd_cpy(const char *src, const char *dest_dir);

/* Moves src file into dest_dir (copy + delete source).
 * Returns 0 on success, 1 on error. */
int cmd_cut(const char *src, const char *dest_dir);

/* Copies src_dir into dest_parent/dirname(src_dir) recursively.
 * Returns 0 on success, 1 on error. */
int cmd_cpydir(const char *src_dir, const char *dest_parent);

/* Moves src_dir into dest_parent (copy + delete source) recursively.
 * Returns 0 on success, 1 on error. */
int cmd_cutdir(const char *src_dir, const char *dest_parent);

#endif /* OS_H */
