// gls_minimal.c
// Minimal ls -la clone (single file, POSIX.1-2008 compliant)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>

#ifndef PATH_MAX
#define PATH_MAX 1024
#endif

// ----------------- Permission String -----------------
static void get_permissions(mode_t mode, char *perms) {
    if (S_ISDIR(mode)) perms[0] = 'd';
    else if (S_ISLNK(mode)) perms[0] = 'l';
    else perms[0] = '-';

    perms[1] = (mode & S_IRUSR) ? 'r' : '-';
    perms[2] = (mode & S_IWUSR) ? 'w' : '-';
    perms[3] = (mode & S_IXUSR) ? 'x' : '-';
    perms[4] = (mode & S_IRGRP) ? 'r' : '-';
    perms[5] = (mode & S_IWGRP) ? 'w' : '-';
    perms[6] = (mode & S_IXGRP) ? 'x' : '-';
    perms[7] = (mode & S_IROTH) ? 'r' : '-';
    perms[8] = (mode & S_IWOTH) ? 'w' : '-';
    perms[9] = (mode & S_IXOTH) ? 'x' : '-';
    perms[10] = '\0';
}

// ----------------- Human-readable size -----------------
static void human_size(off_t bytes, char *out, size_t outsz) {
    const char *units[] = {"B", "K", "M", "G", "T"};
    double size = (double)bytes;
    int u = 0;
    while (size >= 1024.0 && u < 4) { size /= 1024.0; u++; }
    snprintf(out, outsz, (u == 0) ? "%.0f%s" : "%.1f%s", size, units[u]);
}

// ----------------- Time formatting -----------------
static void get_mod_time(time_t mtime, char *timestr, size_t len) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&mtime);
    double diff = difftime(now, mtime);
    if (!tm_info) { strncpy(timestr, "??? ?? ??:??", len); return; }
    if (diff > 15778800 || diff < 0)
        strftime(timestr, len, "%b %e  %Y", tm_info);
    else
        strftime(timestr, len, "%b %e %H:%M", tm_info);
}

// ----------------- Safe printable name -----------------
static void sanitize(char *dest, const char *src, size_t max_len) {
    size_t i;
    for (i = 0; i < max_len - 1 && src[i] != '\0'; i++)
        dest[i] = isprint((unsigned char)src[i]) ? src[i] : '?';
    dest[i] = '\0';
}

// ----------------- Single entry print -----------------
static void print_entry(const char *dirpath, const char *name) {
    char fullpath[PATH_MAX];
    struct stat st;
    snprintf(fullpath, sizeof(fullpath), "%s/%s", dirpath, name);
    if (lstat(fullpath, &st) == -1) return;

    char perms[11], user[64], group[64], tbuf[64], sizebuf[32], safe[PATH_MAX];
    get_permissions(st.st_mode, perms);
    struct passwd *pw = getpwuid(st.st_uid);
    struct group *gr = getgrgid(st.st_gid);
    snprintf(user, sizeof(user), "%s", pw ? pw->pw_name : "?");
    snprintf(group, sizeof(group), "%s", gr ? gr->gr_name : "?");
    get_mod_time(st.st_mtime, tbuf, sizeof(tbuf));
    human_size(st.st_size, sizebuf, sizeof(sizebuf));
    sanitize(safe, name, sizeof(safe));

    printf("%s %2lu %-8s %-8s %6s %s %s",
           perms, (unsigned long)st.st_nlink, user, group,
           sizebuf, tbuf, safe);

    if (S_ISLNK(st.st_mode)) {
        char linkbuf[PATH_MAX];
        ssize_t len = readlink(fullpath, linkbuf, sizeof(linkbuf) - 1);
        if (len != -1) {
            linkbuf[len] = '\0';
            printf(" -> %s", linkbuf);
        }
    }
    printf("\n");
}

// ----------------- Directory listing -----------------
static void list_directory(const char *path, int show_header) {
    DIR *dir = opendir(path);
    if (!dir) { perror(path); return; }

    if (show_header) printf("%s:\n", path);

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        // show all files including hidden ones (like -a)
        print_entry(path, entry->d_name);
    }
    closedir(dir);
    if (show_header) printf("\n");
}

// ----------------- Main -----------------
int main(int argc, char *argv[]) {
// treat "*" or "./*" as current directory
	if (argc == 2 && 
		(strcmp(argv[1], "*") == 0 || strcmp(argv[1], "./*") == 0)) {
		list_directory(".", 0);
		return 0;
	}

    for (int i = 1; i < argc; i++) {
        struct stat st;
        if (lstat(argv[i], &st) == -1) { perror(argv[i]); continue; }

        if (S_ISDIR(st.st_mode)) {
            int show_header = (argc > 2);
            list_directory(argv[i], show_header);
        } else {
            print_entry("", argv[i]);
        }
    }

    return 0;
}
