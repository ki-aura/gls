#ifndef OPTIONS_H
#define OPTIONS_H

#include <stdbool.h>

// ===============================
// Constants
// ===============================
#define MAX_OPERANDS    256
#define PROG_VERSION	"1.2.0"

// ===============================
// Structs
// ===============================
typedef struct {
/* we don't populate these are they immediately exit, but they could be here if needed
    bool help;
    bool version;	
*/
    // options
    bool show_all;
    bool sort_by_time;
    // operands
    char **operands;
    int operand_count;
} Options;

// ===============================
// Public API prototypes
// ===============================

// Parses command-line options into a dynamically allocated Options struct.
// Exits with error on invalid input.
Options *parse_loptions(int argc, char *argv[]);

// Frees the Options structure and all its dynamically allocated members.
void free_options(Options *opts);

#endif // OPTIONS_H
