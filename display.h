#ifndef DISPLAY_H
#define DISPLAY_H

/*
 * display.h - Interfaces for rendering directory entries
 * -------------------------------------------------------
 * Declares the presentation helpers shared between gls.c and display.c.  The
 * functions here interpret struct stat data, so any consumer should include
 * gls.h first to obtain the shared FileStats definition.
 */

#include "gls.h"

void print_file_entry(const char *path, const char *filename, const struct stat *st, FileStats *stats);
void human_size(off_t bytes, char *out, size_t outsz);
void sanitize_string(char *dest, const char *src, size_t max_len);
void get_permissions(mode_t mode, char *perms);
void get_mod_time(time_t mtime, char *timestr, size_t len);

#endif
