#ifndef OPTIONS_H
#define OPTIONS_H

#include "gls.h"

void show_option_help(const char *program_name);
void parse_options(int argc, char *argv[], Options *opts);

#endif
