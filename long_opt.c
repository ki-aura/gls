#define _POSIX_C_SOURCE 200809L
#include "long_opt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <stdbool.h>
#include <errno.h>


// ===============================
// option definitions
// ===============================

// set long options
static struct option long_options[] = {
	{"help",    no_argument, 0, 'h'},
	{"version", no_argument, 0, 'V'},
	{"all",     no_argument, 0, 'a'},
	{"time",    no_argument, 0, 't'},
	{0, 0, 0, 0}
};

//set short options
//NOTE: short options that need an argument must be followed by a :
static const char short_options[] = "hVat"; // 
    
// ===============================
// Internal Functions
// ===============================

static void print_help(Options* opts, const char *prog_name) {
    printf("Usage: %s [OPTIONS] target file(s)/directory(s)...\n", prog_name);
    printf("\nOptions:\n");
    printf("  -h, --help              Show this help message and exit\n");
    printf("  -V, --version           Show version and exit\n");
    printf("  -a, --all               Show All (include files starting with .)\n");
    printf("  -t, --time              Sort by time (default is alphabetical)\n");
    printf("\nIf no target is specified, default will be current directory\n");
	free_options(opts);
	exit(EXIT_SUCCESS);
}

static void print_version(Options* opts, const char *prog_name) {
    printf("%s version: %s\n", prog_name, PROG_VERSION);
	free_options(opts);
	exit(EXIT_SUCCESS);
}

static void free_string_array(char **array, int count) {
    if (array) {
        for (int i = 0; i < count; i++) {
            free(array[i]);
        }
        free(array);
    }
}


// ===============================
// Public API functions
// ===============================

Options* parse_loptions(int argc, char *argv[]) {

	// Create opts structure and set any non-zero defaults
    Options *opts = calloc(1, sizeof(Options));
    if (!opts) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        exit(EXIT_FAILURE);
    }
    
    int opt; // iterator as we parse the options
    int option_index = 0; // used by getopt_long - ignored by us unless we want to know which option is being processed
    
    while ((opt = getopt_long(argc, argv, short_options, long_options, &option_index)) != -1) {
        switch (opt) {
			// ------ standard help & version options --------
            case 'h': print_help(opts, argv[0]); break;
            case 'V': print_version(opts, argv[0]); break;

			// ------ simple bool options --------
            case 'a':  opts->show_all = true; break;
            case 't':  opts->sort_by_time = true; break;
                                
			// ------ standard handler options --------
            case '?':
            	// we only get here if getopt_long has already found an error and printed error message
                free_options(opts);
                exit(EXIT_FAILURE);
                
            default:
   				fprintf(stderr, "Internal error: unexpected getopt_long return value: %d\n", opt);
                free_options(opts);
                exit(EXIT_FAILURE);
        }
    }
    
	// optind tells us where the first non-option argument is (i.e., the first operand)
	// If there are no operands, optind == argc
    opts->operand_count = argc - optind;
    
	// NOTE: getopt_long re-orders argv so that options come first, operands at end:
	//    demo -d 1 *.c --exclude "fred" *h --woo
	// is reordered to:
	//    demo -d 1 --exclude "fred" --woo *.c *.h
	// So optind would be 6, pointing to *.c at argv[6], even though 
	// *.c was originally at argv[3] on the command line

    
    // Collect operands    
    if (opts->operand_count > MAX_OPERANDS) {
        fprintf(stderr, "Error: too many operands (max %d)\n", MAX_OPERANDS);
        free_options(opts);
        exit(EXIT_FAILURE);
    }
	
	// flag to use a default if there's no target specified 
	bool use_default = false;
    if (opts->operand_count == 0) {
    	use_default = true;
    	opts->operand_count = 1;
    }

    opts->operands = malloc(opts->operand_count * sizeof(char*));
    if (!opts->operands) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        free_options(opts);
        exit(EXIT_FAILURE);
    }
    
    for (int i = 0; i < opts->operand_count; i++) {
        opts->operands[i] = strdup(use_default ? "." : argv[optind + i]);
        if (!opts->operands[i]) {
            fprintf(stderr, "Error: Memory allocation failed\n");
            // Clean up already allocated operands - partial cleanup before calling free_options
            // We need to set operand_count to i so free_options knows how many to free
            opts->operand_count = i;
            free_options(opts);
            exit(EXIT_FAILURE);
        }
    }
    
    return opts;
}

void free_options(Options *opts) {
    if (opts) {
        free_string_array(opts->operands, opts->operand_count);
        free(opts);
    }
}

