#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <dirent.h>
#include <errno.h>

#ifndef PATH_MAX
#define PATH_MAX 1024
#endif

#ifndef NAME_MAX
#define NAME_MAX 255
#endif

// --- Sorting Configuration ---
// Default sort order: 0 = Alphabetical (ls default)
// To sort by time (like ls -t), change this to 1:
#define SORT_ORDER 0 

#define SORT_ALPHA 0
#define SORT_TIME  1
// -----------------------------

#define IGNORE_HIDDEN 0 // Used to control printing of dot-files

// --- Global Structure for Directory Totals ---
typedef struct {
    long total_blocks;
    int actual_files;
    int actual_directories;
    int symbolic_links;
} total_counts_t;

// --- Structure to hold file data for sorting ---
typedef struct {
    char name[NAME_MAX + 1]; // Max filename length in POSIX (often 255)
    struct stat st;          // File status information
} file_info_t;

// --- Function Prototypes ---
char *format_permissions(mode_t mode, char *perm_str);
char *get_username(uid_t uid);
char *get_groupname(gid_t gid);
long get_file_size(const struct stat *st);
char *format_time(const time_t *time);
char *handle_unprintable(const char *name, char *buffer, size_t buf_size);
void print_long_listing(const char *dirname, const char *filename, const struct stat *st, total_counts_t *counts);

// --- Sorting Functions ---
int compare_entries(const void *a, const void *b);
file_info_t *collect_entries(const char *target, total_counts_t *counts, int *num_entries);

// --- Implementation ---

/**
 * @brief Formats the file type and permissions string (e.g., "-rwxr-xr-x").
 * * @param mode The file's mode_t (st_mode).
 * @param perm_str A buffer of at least 11 characters to hold the result.
 * @return char* The permissions string.
 */
char *format_permissions(mode_t mode, char *perm_str) {
    // 1. File Type
    if (S_ISREG(mode)) perm_str[0] = '-';
    else if (S_ISDIR(mode)) perm_str[0] = 'd';
    else if (S_ISLNK(mode)) perm_str[0] = 'l';
    else if (S_ISCHR(mode)) perm_str[0] = 'c';
    else if (S_ISBLK(mode)) perm_str[0] = 'b';
    else if (S_ISFIFO(mode)) perm_str[0] = 'p';
    else if (S_ISSOCK(mode)) perm_str[0] = 's';
    else perm_str[0] = '?';

    // 2. User Permissions (rwx)
    perm_str[1] = (mode & S_IRUSR) ? 'r' : '-';
    perm_str[2] = (mode & S_IWUSR) ? 'w' : '-';
    
    // Setuid bit
    if (mode & S_ISUID) {
        perm_str[3] = (mode & S_IXUSR) ? 's' : 'S';
    } else {
        perm_str[3] = (mode & S_IXUSR) ? 'x' : '-';
    }

    // 3. Group Permissions (rwx)
    perm_str[4] = (mode & S_IRGRP) ? 'r' : '-';
    perm_str[5] = (mode & S_IWGRP) ? 'w' : '-';

    // Setgid bit
    if (mode & S_ISGID) {
        perm_str[6] = (mode & S_IXGRP) ? 's' : 'S';
    } else {
        perm_str[6] = (mode & S_IXGRP) ? 'x' : '-';
    }

    // 4. Other Permissions (rwx)
    perm_str[7] = (mode & S_IROTH) ? 'r' : '-';
    perm_str[8] = (mode & S_IWOTH) ? 'w' : '-';

    // Sticky bit
    if (mode & S_ISVTX) {
        perm_str[9] = (mode & S_IXOTH) ? 't' : 'T';
    } else {
        perm_str[9] = (mode & S_IXOTH) ? 'x' : '-';
    }

    perm_str[10] = '\0';
    return perm_str;
}

/**
 * @brief Gets the username from a UID.
 * * @param uid The user ID.
 * @return char* The username (static buffer, not thread-safe).
 */
char *get_username(uid_t uid) {
    static char buffer[32]; // Not thread-safe, but simple for this purpose
    struct passwd *pw = getpwuid(uid);
    if (pw != NULL) {
        // Use handle_unprintable for safety and consistency
        return handle_unprintable(pw->pw_name, buffer, sizeof(buffer));
    }
    snprintf(buffer, sizeof(buffer), "%d", (int)uid);
    return buffer;
}

/**
 * @brief Gets the group name from a GID.
 * * @param gid The group ID.
 * @return char* The group name (static buffer, not thread-safe).
 */
char *get_groupname(gid_t gid) {
    static char buffer[32]; // Not thread-safe, but simple for this purpose
    struct group *gr = getgrgid(gid);
    if (gr != NULL) {
        // Use handle_unprintable for safety and consistency
        return handle_unprintable(gr->gr_name, buffer, sizeof(buffer));
    }
    snprintf(buffer, sizeof(buffer), "%d", (int)gid);
    return buffer;
}

/**
 * @brief Extracts the file size in bytes from the stat structure.
 * * @param st The file's stat structure.
 * @return long The file size in bytes.
 */
long get_file_size(const struct stat *st) {
    return (long)st->st_size;
}

/**
 * @brief Formats the modification time.
 * * @param time The time_t value (st_mtime).
 * @return char* The formatted time string (static buffer, not thread-safe).
 */
char *format_time(const time_t *time) {
    static char time_str[64];
    // POSIX/ISO C function to format time
    struct tm *tm = localtime(time);
    if (tm == NULL) {
        strncpy(time_str, "??? ?? ??:??", sizeof(time_str));
    } else {
        // Example format: Oct 23 12:16
        strftime(time_str, sizeof(time_str), "%b %e %H:%M", tm);
    }
    return time_str;
}

/**
 * @brief Checks for unprintable characters and replaces them.
 * * @param name The original string.
 * @param buffer The buffer to store the result.
 * @param buf_size The size of the buffer.
 * @return char* The modified string (or original if no changes).
 */
char *handle_unprintable(const char *name, char *buffer, size_t buf_size) {
    size_t i, j;
    // We'll replace non-printable ASCII chars (excluding \n, \t) with '?'
    // The buffer must be large enough; for simplicity, we assume one replacement
    // won't overflow a reasonable buffer (like the ones used above).
    
    // Minimalistic implementation: simply check and replace
    for (i = 0, j = 0; name[i] != '\0' && j < buf_size - 1; i++) {
        // A simple check for non-printable characters (ASCII 0-31 and 127)
        if ((unsigned char)name[i] < 32 && name[i] != '\n' && name[i] != '\t') {
            buffer[j++] = '?'; // Use '?' to indicate unprintable
        } else {
            buffer[j++] = name[i];
        }
    }
    buffer[j] = '\0';
    return buffer;
}

/**
 * @brief Prints a single entry in the 'ls -l' format and updates the totals.
 * * @param dirname The name of the parent directory.
 * @param filename The name of the entry.
 * @param st The file's stat structure.
 * @param counts Pointer to the total_counts_t structure.
 */
void print_long_listing(const char *dirname, const char *filename, const struct stat *st, total_counts_t *counts) {
    char perm_str[11];
    char name_buf[256];
    char path_buf[PATH_MAX];
    ssize_t link_len = 0;

    // The total counts are updated in collect_entries now, 
    // but the final count summary needs to track printed items.
    // NOTE: This part is primarily for the final summary counts of PRINTED items.
    if (S_ISREG(st->st_mode)) {
        counts->actual_files++;
    } else if (S_ISDIR(st->st_mode)) {
        // Don't count '.' and '..' as actual directories for the total count
        if (strcmp(filename, ".") != 0 && strcmp(filename, "..") != 0) {
            counts->actual_directories++;
        }
    } else if (S_ISLNK(st->st_mode)) {
        counts->symbolic_links++;
    }
    
    // 2. Permissions and Link Count
    printf("%s %2lu", format_permissions(st->st_mode, perm_str), (unsigned long)st->st_nlink);

    // 3. User and Group
    printf(" %-8s %-8s", get_username(st->st_uid), get_groupname(st->st_gid));

    // 4. Size (Your function will wrap this) and Time
    printf(" %8ld %s", get_file_size(st), format_time(&st->st_mtime));

    // 5. Filename (with unprintable check)
    printf(" %s", handle_unprintable(filename, name_buf, sizeof(name_buf)));

    // 6. Symbolic Link Target
    if (S_ISLNK(st->st_mode)) {
        // Construct full path for readlink. Use dirname/filename
        if (strcmp(dirname, ".") == 0) {
            snprintf(path_buf, PATH_MAX, "%s", filename);
        } else {
            snprintf(path_buf, PATH_MAX, "%s/%s", dirname, filename);
        }

        // readlink(2) is a POSIX function
        link_len = readlink(path_buf, name_buf, sizeof(name_buf) - 1);
        if (link_len != -1) {
            name_buf[link_len] = '\0'; // Null-terminate the target path
            printf(" -> %s", name_buf);
        } else {
            fprintf(stderr, "\n%s: warning: failed to read link target for %s: %s\n",
                    "gls", filename, strerror(errno));
            // Just print the link itself if target read fails
        }
    }

    printf("\n");
}

/**
 * @brief Comparison function for qsort().
 * * @param a Pointer to the first file_info_t struct.
 * @param b Pointer to the second file_info_t struct.
 * @return int A value < 0, 0, or > 0 based on the sort order.
 */
int compare_entries(const void *a, const void *b) {
    const file_info_t *entry_a = (const file_info_t *)a;
    const file_info_t *entry_b = (const file_info_t *)b;

    #if SORT_ORDER == SORT_TIME
        // Time sorting (ls -t): Newest (later time) first. 
        // st_mtime is time_t, which is an integer type (seconds since epoch)
        // If a is newer than b (a.st_mtime > b.st_mtime), return -1 to put a first.
        // POSIX standard requires time_t comparison with difftime() for portability, 
        // but simple subtraction often suffices for modern Unix systems.
        if (entry_a->st.st_mtime < entry_b->st.st_mtime) return 1;
        if (entry_a->st.st_mtime > entry_b->st.st_mtime) return -1;
        
        // If times are equal, fall back to alphabetical for stable sort
        return strcoll(entry_a->name, entry_b->name);
    #else
        // Default alphabetical sorting (ls -l default)
        // strcoll is preferred over strcmp for locale-aware sorting.
        return strcoll(entry_a->name, entry_b->name);
    #endif
}

/**
 * @brief Reads all directory entries and collects their file information.
 * * @param target The directory path.
 * @param counts Pointer to total_counts_t to update blocks and total counts.
 * @param num_entries Pointer to store the number of collected entries.
 * @return file_info_t* Dynamically allocated array of file information, or NULL on error.
 */
file_info_t *collect_entries(const char *target, total_counts_t *counts, int *num_entries) {
    DIR *dir;
    struct dirent *entry;
    file_info_t *file_list = NULL;
    int capacity = 10; // Initial capacity, will be grown
    int count = 0;
    struct stat st; // Used internally for lstat

    dir = opendir(target);
    if (dir == NULL) {
        fprintf(stderr, "gls: cannot open directory '%s': %s\n", target, strerror(errno));
        return NULL;
    }

    // Allocate initial memory for the list
    file_list = (file_info_t *)malloc(capacity * sizeof(file_info_t));
    if (file_list == NULL) {
        perror("gls: malloc error");
        closedir(dir);
        return NULL;
    }

    // Read all entries, collect data, and calculate total blocks
    while ((entry = readdir(dir)) != NULL) {
        char full_path[PATH_MAX];
        snprintf(full_path, PATH_MAX, "%s/%s", target, entry->d_name);

        // Use lstat for link information and st_blocks
        if (lstat(full_path, &st) == 0) {
            // Update total blocks (includes hidden files)
            counts->total_blocks += st.st_blocks;

            // Check if we need to resize the list
            if (count >= capacity) {
                capacity *= 2;
                file_info_t *new_list = (file_info_t *)realloc(file_list, capacity * sizeof(file_info_t));
                if (new_list == NULL) {
                    perror("gls: realloc error (partial listing possible)");
                    // Continue with current list size if realloc fails
                } else {
                    file_list = new_list;
                }
            }

            // Store the entry information
            strncpy(file_list[count].name, entry->d_name, NAME_MAX);
            file_list[count].name[NAME_MAX] = '\0'; // Ensure null termination
            file_list[count].st = st;
            count++;
        } else {
            fprintf(stderr, "gls: cannot access '%s': %s\n", full_path, strerror(errno));
            // Entry skipped
        }
    }

    closedir(dir);
    *num_entries = count;
    return file_list;
}

/**
 * @brief Main function to list the directory contents.
 * * @param target The target path (file or directory).
 * @return int 0 on success, 1 on error.
 */
int list_directory(const char *target) {
    struct stat st;
    total_counts_t counts = {0, 0, 0, 0}; // Used for single file and final summary
    
    // Use lstat() to correctly handle symbolic links
    if (lstat(target, &st) == -1) {
        fprintf(stderr, "gls: cannot access '%s': %s\n", target, strerror(errno));
        return 1;
    }

    // --- Case 1: Single File Listing ---
    if (!S_ISDIR(st.st_mode)) {
        // Pass "." as the directory name for single file listing
        print_long_listing(".", target, &st, &counts);
        return 0; // Done
    }

    // --- Case 2: Directory Listing (Collect, Sort, Print) ---
    file_info_t *file_list = NULL;
    int num_entries = 0;

    // 1. Collect all entries and calculate total blocks
    // counts.total_blocks is updated inside collect_entries
    file_list = collect_entries(target, &counts, &num_entries);

    if (file_list == NULL) {
        return 1; // Error already printed
    }

    // Print the 'total' line (reset summary counts before printing)
    printf("total blocks %ld\n", counts.total_blocks);
    
    // Reset counts for the print loop's final summary
    counts.actual_files = 0;
    counts.actual_directories = 0;
    counts.symbolic_links = 0;

    // 2. Sort the collected entries
    qsort(file_list, num_entries, sizeof(file_info_t), compare_entries);

    // 3. Print the actual list
    for (int i = 0; i < num_entries; i++) {
        // Skip hidden files (files starting with '.') if IGNORE_HIDDEN is defined as 1
        // Note: It's defined as 0 at the top, so it prints all files like a true ls -l
        if (IGNORE_HIDDEN && file_list[i].name[0] == '.') {
            continue;
        }

        // Print the listing using the stored stat data
        print_long_listing(target, file_list[i].name, &file_list[i].st, &counts);
    }

    // Print final totals summary
    printf("\nSummary:\n");
    printf("  Regular Files: %d\n", counts.actual_files);
    printf("  Directories:   %d\n", counts.actual_directories);
    printf("  Symbolic Links: %d\n", counts.symbolic_links);

    free(file_list);
    return 0;
}

/**
 * @brief Entry point of the program.
 */
int main(int argc, char *argv[]) {
    const char *target = "."; // Default to current directory

    // Check for target from command line
    if (argc > 1) {
        target = argv[1];
    }
    
    // The ls -l functionality is the default (Spec #2)
    return list_directory(target);
}