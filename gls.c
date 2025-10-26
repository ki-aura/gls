/*
 * gls.c - Core program logic for the `gls` utility
 * -----------------------------------------------
 * Now uses long_opt.c for command-line parsing and operand handling.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <pwd.h>
#include <grp.h>
#include <unistd.h>
#include <locale.h>
#include "gls.h"
#include "display.h"
#include "long_opt.h"

// ========================================
// Memory-Safe Allocation Helpers
// ========================================

void *xmalloc(size_t size) {
    void *ptr = malloc(size);
    if (ptr == NULL && size > 0) {
        fprintf(stderr, "Fatal: Out of memory (malloc %zu bytes).\n", size);
        exit(EXIT_FAILURE);
    }
    return ptr;
}

void *xcalloc(size_t count, size_t size) {
    void *ptr = calloc(count, size);
    if (ptr == NULL && count > 0 && size > 0) {
        fprintf(stderr, "Fatal: Out of memory (calloc %zu × %zu bytes).\n", count, size);
        exit(EXIT_FAILURE);
    }
    return ptr;
}

void *xrealloc(void *ptr, size_t size) {
    void *new_ptr = realloc(ptr, size);
    if (new_ptr == NULL && size > 0) {
        fprintf(stderr, "Fatal: Out of memory (realloc %zu bytes).\n", size);
        free(ptr);
        exit(EXIT_FAILURE);
    }
    return new_ptr;
}

char *xstrdup(const char *str) {
    if (!str) return NULL;
    char *copy = strdup(str);
    if (!copy) {
        fprintf(stderr, "Fatal: Out of memory (strdup).\n");
        exit(EXIT_FAILURE);
    }
    return copy;
}

// ========================================
// UID/GID Cache
// ========================================

#define CACHE_SIZE 16

typedef struct {
    uid_t uid;
    char name[256];
    bool valid;
} UidCache;

typedef struct {
    gid_t gid;
    char name[256];
    bool valid;
} GidCache;

static UidCache uid_cache[CACHE_SIZE];
static GidCache gid_cache[CACHE_SIZE];

static int next_uid_slot = 0;
static int next_gid_slot = 0;

void init_caches(void) {
    for (int i = 0; i < CACHE_SIZE; i++) {
        uid_cache[i].valid = false;
        gid_cache[i].valid = false;
    }
}

static const char *get_cached_username(uid_t uid, char *buf, size_t len) {
    for (int i = 0; i < CACHE_SIZE; i++) {
        if (uid_cache[i].valid && uid_cache[i].uid == uid) {
            strncpy(buf, uid_cache[i].name, len - 1);
            buf[len - 1] = '\0';
            return buf;
        }
    }

    struct passwd *pw = getpwuid(uid);
    if (pw && pw->pw_name) sanitize_string(buf, pw->pw_name, len);
    else snprintf(buf, len, "%d", uid);

    uid_cache[next_uid_slot].uid = uid;
    strncpy(uid_cache[next_uid_slot].name, buf,
            sizeof(uid_cache[next_uid_slot].name) - 1);
    uid_cache[next_uid_slot].name[sizeof(uid_cache[next_uid_slot].name) - 1] = '\0';
    uid_cache[next_uid_slot].valid = true;
    next_uid_slot = (next_uid_slot + 1) % CACHE_SIZE;
    return buf;
}

static const char *get_cached_groupname(gid_t gid, char *buf, size_t len) {
    for (int i = 0; i < CACHE_SIZE; i++) {
        if (gid_cache[i].valid && gid_cache[i].gid == gid) {
            strncpy(buf, gid_cache[i].name, len - 1);
            buf[len - 1] = '\0';
            return buf;
        }
    }

    struct group *gr = getgrgid(gid);
    if (gr && gr->gr_name) sanitize_string(buf, gr->gr_name, len);
    else snprintf(buf, len, "%d", gid);

    gid_cache[next_gid_slot].gid = gid;
    strncpy(gid_cache[next_gid_slot].name, buf,
            sizeof(gid_cache[next_gid_slot].name) - 1);
    gid_cache[next_gid_slot].name[sizeof(gid_cache[next_gid_slot].name) - 1] = '\0';
    gid_cache[next_gid_slot].valid = true;
    next_gid_slot = (next_gid_slot + 1) % CACHE_SIZE;
    return buf;
}

void get_username(uid_t uid, char *username, size_t len) {
    get_cached_username(uid, username, len);
}
void get_groupname(gid_t gid, char *groupname, size_t len) {
    get_cached_groupname(gid, groupname, len);
}

// ========================================
// Utility Functions
// ========================================

int get_link_target(const char *path, char *target, size_t len) {
    ssize_t ret = readlink(path, target, len - 1);
    if (ret == -1) return -1;
    target[ret] = '\0';
    char temp[PATH_MAX];
    sanitize_string(temp, target, sizeof(temp));
    strncpy(target, temp, len - 1);
    target[len - 1] = '\0';
    return 0;
}

// ========================================
// Sorting
// ========================================

static const Options *sort_options_ptr = NULL;

int compare_entries(const void *a, const void *b) {
    const FileEntry *ea = (const FileEntry *)a;
    const FileEntry *eb = (const FileEntry *)b;

    if (sort_options_ptr->sort_by_time) {
        if (ea->mtime > eb->mtime) return -1;
        if (ea->mtime < eb->mtime) return 1;
    }
    return strcoll(ea->name, eb->name);
}

// ========================================
// Directory Listing
// ========================================

int list_directory(const char *path, const Options *opts, bool show_header) {
    DIR *dir;
    struct dirent *entry;
    FileEntry *entries = NULL;
    int count = 0, capacity = 128;
    FileStats stats = {0};

    dir = opendir(path);
    if (!dir) {
        perror(path);
        return 1;
    }

    entries = xmalloc(capacity * sizeof(FileEntry));
    while ((entry = readdir(dir)) != NULL) {
        struct stat st;
        char fullpath[PATH_MAX];

        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        if (!opts->show_all && entry->d_name[0] == '.')
            continue;

        snprintf(fullpath, sizeof(fullpath), "%s/%s", path, entry->d_name);
        if (lstat(fullpath, &st) == -1) continue;

        stats.total_blocks += st.st_blocks;
        if (count >= capacity) {
            capacity *= 2;
            entries = xrealloc(entries, capacity * sizeof(FileEntry));
        }

        entries[count].name = xstrdup(entry->d_name);
        entries[count].mtime = st.st_mtime;
        entries[count].st = st;
        count++;
    }
    closedir(dir);

    if (show_header) printf("%s:\n", path);
    printf("total %ld\n", stats.total_blocks / 2);

    sort_options_ptr = opts;
    qsort(entries, count, sizeof(FileEntry), compare_entries);
    sort_options_ptr = NULL;

    for (int i = 0; i < count; i++) {
        print_file_entry(path, entries[i].name, &entries[i].st, &stats);
        free(entries[i].name);
    }
    free(entries);

    if (!show_header) {
        printf("\nSummary:\n");
        printf("  Regular files:      %d\n", stats.regular_files);
        printf("  Directories:        %d\n", stats.directories);
        printf("  Symlinks:           %d\n", stats.symlinks);
        printf("  Directory symlinks: %d\n", stats.dir_symlinks);
    }
    return 0;
}

// ========================================
// Main
// ========================================

int main(int argc, char *argv[]) {
    int result = 0;

    setlocale(LC_ALL, "");
    init_caches();

    Options *opts = parse_loptions(argc, argv);
    // (Help/version handled internally by long_opt)

    char **file_paths = xcalloc(opts->operand_count, sizeof(char *));
    char **dir_paths  = xcalloc(opts->operand_count, sizeof(char *));
    int file_count = 0, dir_count = 0;

    for (int i = 0; i < opts->operand_count; i++) {
        struct stat st;
        if (lstat(opts->operands[i], &st) == 0) {
            if (S_ISDIR(st.st_mode))
                dir_paths[dir_count++] = opts->operands[i];
            else
                file_paths[file_count++] = opts->operands[i];
        } else {
            perror(opts->operands[i]);
            result = 1;
        }
    }

    // Print files first
    for (int i = 0; i < file_count; i++) {
        struct stat lst;
        if (lstat(file_paths[i], &lst) == 0) {
            FileStats dummy = {0};
            print_file_entry("", file_paths[i], &lst, &dummy);
        }
    }

    // Then directories
    bool show_headers = (dir_count > 1 || file_count > 0);
    for (int i = 0; i < dir_count; i++) {
        if (file_count > 0 || i > 0) printf("\n");
        int ret = list_directory(dir_paths[i], opts, show_headers);
        if (ret != 0) result = ret;
    }

    free(file_paths);
    free(dir_paths);
    free_options(opts);
    return result;
}
