/*
 * options.c - Command-line parsing and help generation
 * ----------------------------------------------------
 * Responsible for translating argv[] into the Options struct consumed by the
 * rest of the program.  A central help table drives both getopt_long() and the
 * formatted usage output so that the description of each switch lives in one
 * place.  Additional commentary explains how truncation, path collection, and
 * validation tie into later filesystem operations.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <stdbool.h>
#include <errno.h>
#include "gls.h"
#include "options.h"

// ========================================
// Help Table Structure
// ========================================

typedef struct {
    const char *short_opt;
    const char *long_opt;
    const char *arg_name;
    const char *description;
} OptionHelp;

#define OPT_HELP(short_opt, long_opt, arg, desc) \
    {short_opt, long_opt, arg, desc}

#define OPT_HELP_END {NULL, NULL, NULL, NULL}

static const OptionHelp help_table[] = {
    OPT_HELP("-a", "--all",     NULL, "Show hidden files (files starting with .)"),
    OPT_HELP("-t", "--time",    NULL, "Sort by modification time (newest first)"),
    OPT_HELP("-T", "--Trunc",   "<N>", "Truncate filenames to N characters (1-255)"),
    OPT_HELP("-h", "--help",    NULL, "Show this help message"),
    OPT_HELP("-v", "--version", NULL, "Show version information"),
    OPT_HELP_END
};

/**
 * Display formatted help
 *
 * Tightly couples the help text to the canonical option definitions above so
 * that the usage information always matches the flags we accept.
 */
void show_option_help(const char *program_name) {
    fprintf(stdout, "Usage: %s [options] [directory...]\n\n", program_name);
    fprintf(stdout, "List directory contents in long format.\n\n");
    fprintf(stdout, "Options:\n");

    for (const OptionHelp *opt = help_table; opt->short_opt != NULL; opt++) {
        char opt_str[64];

        if (opt->long_opt) {
            if (opt->arg_name) {
                snprintf(opt_str, sizeof(opt_str), "%s, %s %s",
                        opt->short_opt, opt->long_opt, opt->arg_name);
            } else {
                snprintf(opt_str, sizeof(opt_str), "%s, %s",
                        opt->short_opt, opt->long_opt);
            }
        } else {
            if (opt->arg_name) {
                snprintf(opt_str, sizeof(opt_str), "%s %s",
                        opt->short_opt, opt->arg_name);
            } else {
                snprintf(opt_str, sizeof(opt_str), "%s", opt->short_opt);
            }
        }

        fprintf(stdout, "  %-24s %s\n", opt_str, opt->description);
    }

    fprintf(stdout, "\nIf no directory is specified, the current directory is used.\n");
}

/**
 * Build short option string from help table
 *
 * getopt_long() expects the short option string to include ':' markers for
 * options that require arguments.  Because we derive the string from the help
 * table there is zero risk of it falling out of sync.
 */
static char *build_short_opts(const OptionHelp *help_table) {
    size_t capacity = 64;
    char *opts = xmalloc(capacity);
    if (!opts) {
        fprintf(stderr, "Fatal: Out of memory (malloc short opts).\n");
        exit(EXIT_FAILURE);
    }
    size_t len = 0;

    for (const OptionHelp *opt = help_table; opt->short_opt != NULL; opt++) {
        if (opt->short_opt[0] == '-' && opt->short_opt[1] != '\0') {
            char ch = opt->short_opt[1];

            if (len + 3 >= capacity) {
                capacity *= 2;
                opts = realloc(opts, capacity);
                if (!opts) {
                    fprintf(stderr, "Fatal: Out of memory (realloc short opts).\n");
                    exit(EXIT_FAILURE);
                }
            }

            opts[len++] = ch;

            if (opt->arg_name) {
                opts[len++] = ':';
            }
        }
    }

    opts[len] = '\0';
    return opts;
}

/**
 * Build long option array from help table
 *
 * The GNU-style long options reuse the same metadata; the trailing sentinel
 * struct option is already zeroed by calloc() so getopt_long() knows when to
 * stop.
 */
static struct option *build_long_opts(const OptionHelp *help_table, size_t *count) {
    size_t n = 0;
    for (const OptionHelp *opt = help_table; opt->short_opt != NULL; opt++) {
        if (opt->long_opt) n++;
    }

    struct option *long_opts = calloc(n + 1, sizeof(struct option));
    if (!long_opts) {
        fprintf(stderr, "Fatal: Out of memory (calloc long opts).\n");
        exit(EXIT_FAILURE);
    }

    size_t idx = 0;
    for (const OptionHelp *opt = help_table; opt->short_opt != NULL; opt++) {
        if (opt->long_opt) {
            const char *name = opt->long_opt;
            if (name[0] == '-' && name[1] == '-') name += 2;

            long_opts[idx].name = name;
            long_opts[idx].has_arg = opt->arg_name ? required_argument : no_argument;
            long_opts[idx].flag = NULL;
            long_opts[idx].val = opt->short_opt[1];
            idx++;
        }
    }

    *count = n;
    return long_opts;
}

/**
 * Parse command-line options
 *
 * Populates the Options struct with both feature toggles and the target paths
 * to enumerate.  Any validation errors (such as invalid truncation lengths) are
 * reported immediately because the downstream filesystem code assumes that
 * Options has been fully sanitised.
 */
void parse_options(int argc, char *argv[], Options *opts) {
    memset(opts, 0, sizeof(Options));

    char *short_opts = build_short_opts(help_table);
    size_t long_count;
    struct option *long_opts = build_long_opts(help_table, &long_count);

    int opt;
    while ((opt = getopt_long(argc, argv, short_opts, long_opts, NULL)) != -1) {
        switch (opt) {
            case 'a': opts->show_all = true; break;
            case 't': opts->sort_by_time = true; break;
            case 'h': opts->show_help = true; break;
            case 'T': {
                char *endptr = NULL;
                errno = 0;
                long value = strtol(optarg, &endptr, 10);
                if (errno != 0 || !endptr || *endptr != '\0' || value < 1 || value > 255) {
                    fprintf(stderr, "Invalid truncation length '%s'. Expected integer between 1 and 255.\n", optarg);
                    free(short_opts);
                    free(long_opts);
                    exit(EXIT_FAILURE);
                }
                opts->truncate_length = (int)value;
                break;
            }
            case 'v': opts->show_version = true; break;
            default:
                show_option_help(argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    if (optind < argc) {
        opts->path_count = argc - optind;
        opts->paths = xmalloc(opts->path_count * sizeof(char *));
        for (int i = 0; i < opts->path_count; i++) {
            opts->paths[i] = strdup(argv[optind + i]);
        }
    } else {
        opts->path_count = 1;
        opts->paths = xmalloc(sizeof(char *));
        opts->paths[0] = strdup(".");
    }

    free(short_opts);
    free(long_opts);
}
