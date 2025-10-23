#ifndef OPTIONS_H
#define OPTIONS_H

/*
 * options.h - Public entry points for argument parsing
 * ----------------------------------------------------
 * Provides the narrow interface that gls.c needs to populate an Options
 * instance.  Keeping this header small reinforces that all parsing state lives
 * inside options.c and avoids leaking getopt() internals into other modules.
 */

#include "gls.h"

void show_option_help(const char *program_name);
void parse_options(int argc, char *argv[], Options *opts);

#endif
