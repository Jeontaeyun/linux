#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <signal.h>
#include <sys/stat.h>

#define _FILE_OFFSET_BITS 64
#define SERVER_NAME "CONNECT_HTTP"
#define SERVER_VERSION "1.0"
#define HTTP_MINOR_VERSION 0
#define BLOCK_BUF_SIZE 1024
#define LINE_BUF_SIZE 4096
#define MAX_REQUEST_BODY_LENGTH (1024 * 1024)

struct HTTPHeaderField
{
    char *name;
    char *value;
    struct HTTPHeaderField *next;
};

struct HTTPRequest
{
    int protocol_minor_version;
    char *method;
    struct HTTPHeaderField *header;
    char *body;
    long length;
};

struct FileInfo
{
    char *path;
    long size;
    int ok;
};

typedef void (*sighandler_t)(int);
static void install_signal_handlers(void);
static void service(FILE *in, FILE *out, char *docroot);
static void log_exit(char *fmt, ...);
static void *xmalloc(size_t sz);
static void install_signal_handlers(void);
static void trap_signal(int sig, sighandler_t handler);
static void signal_exit(int sig);
static void validate_directory(char *path);

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        fprintf(stderr, "USAGE: %s <docroot>\n", argv[0]);
        exit(1);
    }
    validate_directory(argv[1]);
    exit(0);
}

// ... mean Variable Arguments need #include <stdarg.h>
static void
log_exit(char *fmt, ...)
{
    /**
    * Usage of variable arguments
    * vs_list ap;
    * vs_start(ap, fmt)
    * /-- use ap --/
    * vs_end(ap)
    **/
    va_list ap;

    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);
    exit(1);
}

static void *
xmalloc(size_t sz)
{
    void *p;

    p = malloc(sz);
    if (!p)
        log_exit("failed to allocate memory");
    return p;
}

static void
install_signal_handlers(void)
{
    trap_signal(SIGPIPE, signal_exit);
}

static void trap_signal(int sig, sighandler_t handler)
{
    struct sigaction act;
    act.sa_handler = handler;
    sigemptyset(&act.sa_mask);
    act.sa_flags = SA_RESTART;
    if (sigaction(sig, &act, NULL) < 0)
        log_exit("sigaction() failed: %s", strerror(errno));
}

static void signal_exit(int sig)
{
    log_exit("exit by signal %d", sig);
}

static void validate_directory(char *path)
{
    struct stat *buf;
    if (stat(path, buf) < -1)
    {
        log_exit("path is not directory");
    }
    if (S_ISDIR(buf->st_mode))
        printf("DIRECTORY");
    exit(0);
}