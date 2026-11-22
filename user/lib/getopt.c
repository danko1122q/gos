#include "gos.h"
#include <getopt.h>
char *optarg;
int optind = 1;
int opterr = 1;
int optopt;
static const char *next_char;
static int last_optind = 1;

int getopt(int argc, char *const argv[], const char *optstring) {
    if (!optstring) { errno = EINVAL; return -1; }
    if (optind != last_optind || optind == 1) next_char = NULL;
    optarg = NULL;
    if (!next_char || !*next_char) {
        next_char = NULL;
        if (optind >= argc || !argv[optind] || argv[optind][0] != '-' || !argv[optind][1]) return -1;
        if (!strcmp(argv[optind], "--")) { optind++; return -1; }
        next_char = argv[optind] + 1;
        optind++;
        last_optind = optind;
    }
    int c = (unsigned char)*next_char++;
    const char *spec = strchr(optstring[0] == ':' ? optstring + 1 : optstring, c);
    if (!spec || c == ':') {
        optopt = c;
        if (!*next_char) next_char = NULL;
        if (opterr && optstring[0] != ':') fprintf(stderr, "%s: invalid option -- %c\n", argv[0], c);
        return '?';
    }
    if (spec[1] == ':') {
        if (next_char && *next_char) { optarg = (char *)next_char; next_char = NULL; }
        else if (optind < argc) { optarg = argv[optind++]; next_char = NULL; last_optind = optind; }
        else {
            optopt = c;
            if (!*next_char) next_char = NULL;
            if (opterr) fprintf(stderr, "%s: option requires an argument -- %c\n", argv[0], c);
            return optstring[0] == ':' ? ':' : '?';
        }
    } else if (!*next_char) next_char = NULL;
    return c;
}
