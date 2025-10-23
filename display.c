/*
 * display.c - Rendering helpers for the long-format listing
 * ----------------------------------------------------------
 * The routines in this file take the raw metadata gathered by gls.c and turn
 * it into user-friendly terminal output.  This includes:
 *   - Translating struct stat fields into POSIX permission strings.
 *   - Formatting timestamps and sizes so they are easy to scan.
 *   - Resolving symlink targets and sanitising control characters to avoid
 *     confusing terminal rendering.
 *
 * Think of this module as the presentation layer: all filesystem state has
 * already been collected, so the focus here is on consistent, legible output.
 */

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <ctype.h>
#include "gls.h"
#include "display.h"

/**
 * Print a single file entry
 *
 * @param path      The directory path used to resolve symlink targets.  For
 *                  direct file arguments this is an empty string so the raw
 *                  filename is used.
 * @param filename  The leaf name as returned by readdir()/lstat().
 * @param st        File metadata snapshot, already populated by the caller.
 * @param stats     Running totals shared across entries so we can produce a
 *                  summary for single-directory listings.
 */
void print_file_entry(const char *path, const char *filename,
                     const struct stat *st, FileStats *stats) {
    char perms[11];
    char username[256];
    char groupname[256];
    char timestr[64];
    char linkbuf[PATH_MAX];
    char safe_filename[PATH_MAX];
    char fullpath[PATH_MAX];
    char display_size[32];

    if (S_ISLNK(st->st_mode)) {
        struct stat target_st;
        if (path[0] == '\0') {
            strncpy(fullpath, filename, sizeof(fullpath));
            fullpath[sizeof(fullpath)-1] = '\0';
        } else {
            snprintf(fullpath, sizeof(fullpath), "%s/%s", path, filename);
        }
        // When a symlink resolves to a directory we count it separately so the
        // summary distinguishes "links to dirs" from regular symlinks.
        if (stat(fullpath, &target_st) == 0 && S_ISDIR(target_st.st_mode))
            stats->dir_symlinks++;
        else
            stats->symlinks++;
    } else if (S_ISDIR(st->st_mode)) stats->directories++;
    else if (S_ISREG(st->st_mode)) stats->regular_files++;

    get_permissions(st->st_mode, perms);
    get_username(st->st_uid, username, sizeof(username));
    get_groupname(st->st_gid, groupname, sizeof(groupname));
    get_mod_time(st->st_mtime, timestr, sizeof(timestr));
    sanitize_string(safe_filename, filename, sizeof(safe_filename));
    human_size((long long)st->st_size, display_size, sizeof(display_size)-1);

    printf("%s %2lu %-8s %-8s %6s %s %s",
           perms,
           (unsigned long)st->st_nlink,
           username,
           groupname,
           display_size,
           timestr,
           safe_filename);

    if (S_ISLNK(st->st_mode)) {
        if (path[0] == '\0') {
            // Fullpath already contains the filename for single file arguments.
        } else {
            snprintf(fullpath, sizeof(fullpath), "%s/%s", path, filename);
        }
        // readlink() truncates to the provided buffer size, so the caller
        // ensures linkbuf is large enough for most paths.  We still sanitise
        // the output in get_link_target() to guarantee a clean display.
        if (get_link_target(fullpath, linkbuf, sizeof(linkbuf)) == 0)
            printf(" -> %s", linkbuf);
    }
    printf("\n");
}

// ----------------- Human readable file size -------------------
// Converts a size in bytes (off_t) to a human-readable string (e.g., 4.5K, 2.1M).
void human_size(off_t bytes, char *out, size_t outsz){
    const char *units[] = {"B", "K", "M", "G", "T"};
    double size = (double)bytes;
    int u = 0;
    // Loop while size is >= 1024 and we have a unit to move up to
    while (size >= 1024.0 && u < 4) {
        size /= 1024.0;
        u++;
    }
    // Format the output string with 1 decimal place and the unit
    snprintf(out, outsz, (u == 0) ? "%.0f%s" : "%.1f%s", size, units[u]);
}

/**
 * Replace non-printable characters with '?' so control codes embedded in file
 * names do not change terminal state.  This mirrors the defensive behaviour of
 * GNU ls when given filenames with escape sequences.
 */
void sanitize_string(char *dest, const char *src, size_t max_len) {
    size_t i;
    for (i = 0; i < max_len - 1 && src[i] != '\0'; i++) {
        dest[i] = isprint((unsigned char)src[i]) ? src[i] : '?';
    }
    dest[i] = '\0';
}

/**
 * Convert the permission/mode bits from struct stat into the familiar
 * "drwxr-xr-x" textual representation.  Special bits (setuid/setgid/sticky)
 * are encoded using the lower-case/upper-case letters that coreutils uses.
 */
void get_permissions(mode_t mode, char *perms) {
    if (S_ISDIR(mode)) perms[0] = 'd';
    else if (S_ISLNK(mode)) perms[0] = 'l';
    else if (S_ISCHR(mode)) perms[0] = 'c';
    else if (S_ISBLK(mode)) perms[0] = 'b';
    else if (S_ISFIFO(mode)) perms[0] = 'p';
    else if (S_ISSOCK(mode)) perms[0] = 's';
    else perms[0] = '-';

    perms[1] = (mode & S_IRUSR) ? 'r' : '-';
    perms[2] = (mode & S_IWUSR) ? 'w' : '-';
    perms[3] = (mode & S_ISUID) ? ((mode & S_IXUSR) ? 's' : 'S') : ((mode & S_IXUSR) ? 'x' : '-');
    perms[4] = (mode & S_IRGRP) ? 'r' : '-';
    perms[5] = (mode & S_IWGRP) ? 'w' : '-';
    perms[6] = (mode & S_ISGID) ? ((mode & S_IXGRP) ? 's' : 'S') : ((mode & S_IXGRP) ? 'x' : '-');
    perms[7] = (mode & S_IROTH) ? 'r' : '-';
    perms[8] = (mode & S_IWOTH) ? 'w' : '-';
    perms[9] = (mode & S_ISVTX) ? ((mode & S_IXOTH) ? 't' : 'T') : ((mode & S_IXOTH) ? 'x' : '-');
    perms[10] = '\0';
}

/**
 * Format modification time using the same cut-off as BSD/GNU ls: recent files
 * show hours/minutes, while older files show the year.  Negative diffs indicate
 * clock skew and also fall back to the year format.
 */
void get_mod_time(time_t mtime, char *timestr, size_t len) {
    struct tm *tm_info;
    time_t now = time(NULL);
    double diff = difftime(now, mtime);

    tm_info = localtime(&mtime);
    if (!tm_info) {
        strncpy(timestr, "??? ?? ??:??", len);
        return;
    }

    if (diff > 15778800 || diff < 0) {
        strftime(timestr, len, "%b %e  %Y", tm_info);
    } else {
        strftime(timestr, len, "%b %e %H:%M", tm_info);
    }
}
