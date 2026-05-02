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

#endif /* OS_H */
