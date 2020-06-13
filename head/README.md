# Linux Head Command

Linux Command can has option like `-n` with - sign. It just for convention.
If there is no reason for custom option rules, It is good to follow this convention.

## Option

Linux Command Option has two type which are `the parameter option` and `the none parameter option`.
And then, The parameter option can be merged like `ls -als`, even it is possible with `head -n5`

Futhermore, Linux Command Option has `the long option` and `the short option` like `--version` and `-l`.

## Option parsing API

```C
#include <unistd.h>

int getopt(int argc, char * const argv[], const char *optdecl);

extern char * optarg;
extern int optind, opterr, optopt;
```

`getopt()` is option parse api which has used from UNIX.

```C++
#define _GNU_SOURCE
#include <getopt.h>

int getopt_long(int argc, char * const argv[], const char * optdecl, const struct option *longoptdecl, iint *longindex);

struct option {
    const char *name;
    int has_arg;
    int *flags;
    int val;
}

extern char *optarg;
extern int optind, opterr, optopt;
```

`getopt_long()` API can parse `-` and even `--` optioon
