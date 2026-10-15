#pragma once

/* MSVC has no getopt.h. The data tool compiles eSpeak's compat/getopt.c. */

struct option
{
    char* name;
    int has_arg;
    int* flag;
    int val;
};

extern int optind;
extern int opterr;
extern int optopt;
extern char* optarg;

#define no_argument 0
#define required_argument 1
#define optional_argument 2

int getopt (int nargc, char* const* nargv, const char* options);
int getopt_long (int nargc, char* const* nargv, const char* options,
                 const struct option* long_options, int* idx);
