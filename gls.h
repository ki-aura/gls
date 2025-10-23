#ifndef GLS_H
#define GLS_H

/*
 * gls.h - Shared data structures and prototypes
 * ---------------------------------------------
 * This header ties the three implementation files together by exposing the
 * program-wide configuration, statistics tracking, and utility helpers.  Any
 * module that needs to reason about filesystem metadata should include this
 * file rather than duplicating struct definitions.
 */

#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <limits.h>
#include <stddef.h>

#define GLS_VERSION "1.2.0"

// ========================================
// Shared Structures
// ========================================

/**
 * Captures the command-line switches that shape program behaviour.  Parsed by
 * options.c and consumed by both gls.c (for traversal decisions) and display.c
 * (for formatting hints such as truncation length).
 */
typedef struct {
    bool show_all;        // -a, --all: show files starting with .
    bool sort_by_time;    // -t, --time: sort by modification time
    bool show_help;       // -h, --help: show help message
    bool show_version;    // -v, --version: show version
    int truncate_length;  // -T, --Trunc: truncate filenames to this length (0 = disabled)
    char **paths;         // array of target directory paths
    int path_count;       // number of paths
} Options;

/**
 * Mutable counters accumulated while traversing a directory tree.  The totals
 * feed the summary output shown after listing a single directory.
 */
typedef struct {
    int regular_files;
    int symlinks;
    int directories;
    int dir_symlinks;
    long total_blocks;
} FileStats;

/**
 * Intermediate representation of a directory entry.  We buffer the name, the
 * modification timestamp (for sorting), and the full stat struct so we can
 * later hand the data to the display layer without additional syscalls.
 */
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
void parse_options(int argc, char *argv[], Options *opts);
void init_caches(void);
void get_username(uid_t uid, char *username, size_t len);
void get_groupname(gid_t gid, char *groupname, size_t len);

void *xmalloc(size_t size);
void *xcalloc(size_t count, size_t size);
void *xrealloc(void *ptr, size_t size);
char *xstrdup(const char *str);

#endif
