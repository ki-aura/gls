#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <unistd.h>
#include <ctype.h>
#include <locale.h>
#include <errno.h>
#include "gls.h"
#include "display.h"

/**
 * Print a single file entry
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

void sanitize_string(char *dest, const char *src, size_t max_len) {
    size_t i;
    for (i = 0; i < max_len - 1 && src[i] != '\0'; i++) {
        dest[i] = isprint((unsigned char)src[i]) ? src[i] : '?';
    }
    dest[i] = '\0';
}

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


