#ifndef GLS_H
#define GLS_H

/*
 * gls.h - Shared data structures and prototypes
 * ---------------------------------------------
 * This header now relies on long_opt.h for the Options definition.
 * It continues to expose utilities and data types needed by display.c
 * and gls.c.
 */

#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <limits.h>
#include <stddef.h>

#include "long_opt.h"   // <-- Options now comes from here

#define GLS_VERSION "1.2.0"

// ========================================
// Shared Structures (except Options)
// ========================================

typedef struct {
    int regular_files;
    int symlinks;
    int directories;
    int dir_symlinks;
    long total_blocks;
} FileStats;

typedef struct {
    char *name;
    time_t mtime;
    struct stat st;
} FileEntry;

// ========================================
// Shared Prototypes
// ========================================

int get_link_target(const char *path, char *target, size_t len);
void print_file_entry(const char *path, const char *filename, const struct stat *st, FileStats *stats);
int list_directory(const char *path, const Options *opts, bool show_header);

void init_caches(void);
void get_username(uid_t uid, char *username, size_t len);
void get_groupname(gid_t gid, char *groupname, size_t len);

void *xmalloc(size_t size);
void *xcalloc(size_t count, size_t size);
void *xrealloc(void *ptr, size_t size);
char *xstrdup(const char *str);

#endif
