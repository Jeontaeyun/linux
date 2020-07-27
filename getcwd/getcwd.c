#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#define INIT_BUFSIZE 1024;

static char *my_getcwd();

int main(int argc, char *argv[])
{
    char *path = my_getcwd();
    if (path)
    {
        printf("%s\n", path);
    }
}

static char *
my_getcwd()
{
    char *buf, *tmp;
    size_t size = INIT_BUFSIZE;

    buf = malloc(size);
    if (!buf)
        return NULL;
    for (;;)
    {
        errno = 0;
        if ((getcwd(buf, size)))
            return buf;
        if (errno != ERANGE)
            break;
        size *= 2;
        tmp = realloc(buf, size);
        if (!tmp)
            break;
        buf = tmp;
    }
    free(buf);
    return NULL;
}