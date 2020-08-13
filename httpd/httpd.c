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
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>

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

// HTTPHeaderField is linked listtt
struct HTTPRequest
{
    int protocol_minor_version;
    char *method;
    char *path;
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
static void upcase(char *str);
static void trap_signal(int sig, sighandler_t handler);
static void signal_exit(int sig);
static void validate_directory(char *path);
static void service(FILE *in, FILE *out, char *docroot);
static void free_request(struct HTTPRequest *req);
static void free_fileinfo(struct FileInfo *info);
static struct HTTPRequest *read_request(FILE *in);
static void read_request_line(struct HTTPRequest *req, FILE *in);
static struct HTTPHeaderField *read_header_field(FILE *in);
static long content_length(struct HTTPRequest *req);
static char *lookup_header_field_value(struct HTTPRequest *req, char *name);
static struct FileInfo *get_fileinfo(char *docroot, char *urlppath);
static char *build_fspath(char *docroot, char *urlpath);
static void respond_to(struct HTTPRequest *req, FILE *out, char *docroot);
static void do_file_response(struct HTTPRequest *req, FILE *out, char *docroot);
static void output_common_header_fields(struct HTTPRequest *req, FILE *out, char *status);
static char *guess_content_type(struct FileInfo *info);
static void method_not_allowed(struct HTTPRequest *req, FILE *out);
static void not_implemented(struct HTTPRequest *req, FILE *out);
static void not_found(struct HTTPRequest *req, FILE *out);
static int listen_socket(char *port);
static void server_main(int server_fd, char *docroot);
static void became_daemon(void);

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        fprintf(stderr, "USAGE: %s <docroot>\n", argv[0]);
        exit(1);
    }
    validate_directory(argv[1]);
    install_signal_handlers();

    service(stdin, stdout, argv[1]);
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
    struct stat buf;

    if (stat(path, &buf) < -1)
        log_exit("path is not available");
    if (!S_ISDIR(buf.st_mode))
        log_exit("path is not directory");
}

// Service is main logic for HTTP
static void service(FILE *in, FILE *out, char *docroot)
{
    struct HTTPRequest *req;

    req = read_request(in);
    respond_to(req, out, docroot);
    free_request(req);
}

// request memory release function
static void free_request(struct HTTPRequest *req)
{
    struct HTTPHeaderField *h, *head;

    head = req->header;
    while (head)
    {
        h = head;
        head = head->next;
        free(h->name);
        free(h->value);
        free(h);
    }
    free(req->method);
    free(req->path);
    free(req->body);
    free(req);
}

static struct HTTPRequest *read_request(FILE *in)
{
    struct HTTPRequest *req;
    struct HTTPHeaderField *h;

    req = xmalloc(sizeof(struct HTTPRequest));
    read_request_line(req, in);
    req->header = NULL;
    while ((h = read_header_field(in)))
    {
        h->next = req->header;
        req->header = h;
    }
    req->length = content_length(req);
    if (req->length != 0)
    {
        if (req->length > MAX_REQUEST_BODY_LENGTH)
            log_exit("request bod too long");
        req->body = xmalloc(req->length);
        if (fread(req->body, req->length, 1, in) < 1)
            log_exit("failed to read request body");
    }
    else
    {
        req->body = NULL;
    }
    return req;
};

static void read_request_line(struct HTTPRequest *req, FILE *in)
{
    char buf[LINE_BUF_SIZE];
    char *path, *p;
    // Data from network is can't be reliable, so we need to make length limit
    if (!fgets(buf, LINE_BUF_SIZE, in))
        log_exit("no request line");
    // buf : GET' '/path/to/file' 'HTTP/1.0\0;
    p = strchr(buf, ' ');
    if (!p)
        log_exit("parse error on request line (1): %s", buf);
    *p++ = '\0';
    req->method = xmalloc(p - buf);
    strcpy(req->method, buf);
    upcase(req->method);

    path = p;
    p = strchr(path, ' ');
    if (!p)
        log_exit("parse error on request line (2): %s", buf);
    *p++ = '\0';
    req->path = xmalloc(p - path);
    strcpy(req->path, path);

    // strncasecmp compare 2 word a,b with no upper/lower case from 0~n
    if (strncasecmp(p, "HTTP/1.", strlen("HTTP/1.")) != 0)
        log_exit("parse error on request line (3): %s", buf);
    p += strlen("HTTP/.");
    req->protocol_minor_version = atoi(p);
}

static struct HTTPHeaderField *
read_header_field(FILE *in)
{
    struct HTTPHeaderField *h;
    char buf[LINE_BUF_SIZE];
    char *p;

    if (!fgets(buf, LINE_BUF_SIZE, in))
        log_exit("failed to read request header field: %s", strerror(errno));
    if ((buf[0] == '\n' || strcmp(buf, "\r\n") == 0))
        return NULL;

    p = strchr(buf, ':');
    if (!p)
        log_exit("parse error on request header field: %s", buf);
    *p++ = '\0';
    h = xmalloc(sizeof(struct HTTPHeaderField));
    h->name = xmalloc(p - buf);
    strcpy(h->name, buf);

    p += strspn(p, " \t");
    h->value = xmalloc(strlen(p) + 1);
    strcpy(h->value, p);

    return h;
}

static long content_length(struct HTTPRequest *req)
{
    char *val;
    long len;
    val = lookup_header_field_value(req, "Content-Length");
    if (!val)
        return 0;
    len = atol(val);
    if (len < 0)
        log_exit("negative Content-Length value");
    return len;
}

static char *lookup_header_field_value(struct HTTPRequest *req, char *name)
{
    struct HTTPHeaderField *h;

    for (h = req->header; h; h = h->next)
    {
        if (strcasecmp(h->name, name) == 0)
            return h->value;
    }
    return NULL;
}

static struct FileInfo *
get_fileinfo(char *docroot, char *urlpath)
{
    struct FileInfo *info;
    struct stat st;

    info = xmalloc(sizeof(struct FileInfo));
    info->path = build_fspath(docroot, urlpath);
    info->ok = 0;
    // HTTP Protocol shouldn't access beyond current directory
    if (lstat(info->path, &st) < 0)
        return info;
    if (!S_ISREG(st.st_mode))
        return info;
    info->ok = 1;
    info->size = st.st_size;
    return info;
}

static char *build_fspath(char *docroot, char *urlpath)
{
    char *path;

    path = xmalloc(strlen(docroot) + 1 + strlen(urlpath) + 1);
    sprintf(path, "%s/%s", docroot, urlpath);

    return path;
}

static void respond_to(struct HTTPRequest *req, FILE *out, char *docroot)
{
    if (strcmp(req->method, "GET") == 0)
        do_file_response(req, out, docroot);
    else if (strcmp(req->method, "HEAD") == 0)
        do_file_response(req, out, docroot);
    else if (strcmp(req->method, "POST") == 0)
        method_not_allowed(req, out);
    else
        not_implemented(req, out);
}

static void do_file_response(struct HTTPRequest *req, FILE *out, char *docroot)
{
    struct FileInfo *info;

    info = get_fileinfo(docroot, req->path);
    if (!info->ok)
    {
        free_fileinfo(info);
        not_found(req, out);
        return;
    }
    output_common_header_fields(req, out, "200 OK");
    fprintf(out, "Content-Length: %ld\r\n", info->size);
    fprintf(out, "Content-Type: %s\r\n", guess_content_type(info));
    fprintf(out, "\r\n");
    if (strcmp(req->method, "HEAD") != 0)
    {
        int fd;
        char buf[BLOCK_BUF_SIZE];
        ssize_t n;

        fd = open(info->path, O_RDONLY);
        if (fd < 0)
            log_exit("failed to open %s: %s", info->path, strerror(errno));
        for (;;)
        {
            n = read(fd, buf, BLOCK_BUF_SIZE);
            if (n < 0)
                log_exit("failed to read %s: %s", info->path, strerror(errno));
            if (n == 0)
                break;
            if (fwrite(buf, n, 1, out), n)
                log_exit("failed to write to socket: %s", strerror(errno));
        }
        close(fd);
    }
    fflush(out);
    free_fileinfo(info);
}

#define TIME_BUF_SIZE 64

// Common header field
static void
output_common_header_fields(struct HTTPRequest *req, FILE *out, char *status)
{
    time_t t;
    struct tm *tm;
    char buf[TIME_BUF_SIZE];

    t = time(NULL);
    tm = gmtime(&t);
    if (!tm)
        log_exit("gmtime() failed : %s", strerror(errno));
    strftime(buf, TIME_BUF_SIZE, "%a, %d %b %Y %H:%M:%S GMT", tm);
    fprintf(out, "HTTP/1.%d %s\r\n", HTTP_MINOR_VERSION, status);
    fprintf(out, "Date: %s\r\n", buf);
    fprintf(out, "Server: %s/%s\r\n", SERVER_NAME, SERVER_VERSION);
    fprintf(out, "Connection: close\r\n");
}

static char *
guess_content_type(struct FileInfo *info)
{
    return "text/plain";
}

static void
free_fileinfo(struct FileInfo *info)
{
    free(info->path);
    free(info);
}

static void
method_not_allowed(struct HTTPRequest *req, FILE *out)
{
    output_common_header_fields(req, out, "405 Method Not Allowed");
    fprintf(out, "Content-Type: text/html\r\n");
    fprintf(out, "\r\n");
    fprintf(out, "<html>\r\n");
    fprintf(out, "<header>\r\n");
    fprintf(out, "<title>405 Method Not Allowed</title>\r\n");
    fprintf(out, "<header>\r\n");
    fprintf(out, "<body>\r\n");
    fprintf(out, "<p>The request method %s is not allowed</p>\r\n", req->method);
    fprintf(out, "</body>\r\n");
    fprintf(out, "</html>\r\n");
    fflush(out);
}

static void
not_implemented(struct HTTPRequest *req, FILE *out)
{
    output_common_header_fields(req, out, "501 Not Implemented");
    fprintf(out, "Content-Type: text/html\r\n");
    fprintf(out, "\r\n");
    fprintf(out, "<html>\r\n");
    fprintf(out, "<header>\r\n");
    fprintf(out, "<title>501 Not Implemented</title>\r\n");
    fprintf(out, "<header>\r\n");
    fprintf(out, "<body>\r\n");
    fprintf(out, "<p>The request method %s is not implemented</p>\r\n", req->method);
    fprintf(out, "</body>\r\n");
    fprintf(out, "</html>\r\n");
    fflush(out);
}

static void
not_found(struct HTTPRequest *req, FILE *out)
{
    output_common_header_fields(req, out, "404 Not Found");
    fprintf(out, "Content-Type: text/html\r\n");
    fprintf(out, "\r\n");
    if (strcmp(req->method, "HEAD") != 0)
    {
        fprintf(out, "<html>\r\n");
        fprintf(out, "<header><title>Not Found</title><header>\r\n");
        fprintf(out, "<body><p>File not found</p></body>\r\n");
        fprintf(out, "</html>\r\n");
    }
    fflush(out);
}

static void
upcase(char *str)
{
    char *p;

    for (p = str; *p; p++)
    {
        *p = (char)toupper((int)*p);
    }
}

#define MAX_BACKLOG 5
#define DEFAULT_PORT "80"

static int
listen_socket(char *port)
{
    struct addrinfo hints, *res, *ai;
    int err;

    memset(&hints, 0, sizeof(struct addrinfo));
    // Declare this socket to IPv4
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    // Declare this socket to usage of server
    hints.ai_flags = AI_PASSIVE;
    if ((err = getaddrinfo(NULL, port, &hints, &res)) != 0)
        log_exit(gai_strerror(err));
    for (ai = res; ai; ai->ai_next)
    {
        int sock;

        sock = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (sock < 0)
            continue;
        if (bind(sock, ai->ai_addr, ai->ai_addrlen) < 0)
        {
            close(sock);
            continue;
        }
        freeaddrinfo(res);
        return sock;
    }
    log_exit("failed to listen socket");
    return -1;
}

static void
server_main(int server_fd, char *docroot)
{
    for (;;)
    {
        struct sockaddr_storage addr;
        socklen_t addrlen = sizeof addr;
        int sock;
        int pid;

        sock = accept(server_fd, (struct sockaddr *)&addr, &addrlen);
        if (sock < 0)
            log_exit("accept(2) failed: %s", strerror(errno));
        // Fork child proccess
        pid = fork();
        if (pid < 0)
            exit(3);
        // Child Process
        if (pid == 0)
        {
            FILE *inf = fdopen(sock, "r");
            FILE *outf = fdopen(sock, "w");

            service(inf, outf, docroot);
        }
        close(sock);
    }
}

static void become_daemon()
{
    int n;

    /**
     *  01. 작업 디렉토리를 root로 변경해 파일 시스템이 마운트 해제할 수 없도록 함.
     *     프로세스가 Current Directory로 사용하고 있는 파일 시스템은 마운트 해제할 수 없으므로 
     *     장기간 실행되는 데몬은 가급적 루트 디렉토리로 이동해야 함
     **/
    if (chdir("/") < 0)
        log_exit("chdir(2) failed: %s", strerror(errno));
    /**
     * 02. 표준 입출력 방지
     **/
    freopen("/dev/null", "r", stdin);
    freopen("/dev/null", "w", stdout);
    freopen("/dev/null", "w", stderr);
    /**
     * 03. 실제로 데몬이 되는 과정
     **/
    n = fork();
    if (n < 0)
        log_exit("fork(2): failed: %s", strerror(errno));
    if (n != 0)
        _exit(0); /* 부모 프로세스를 종료시킴 */
    if (setsid() < 0)
        /* setsid()를 통해 자식 프로세스를 프로세스 새로운 세션을 만듬
         * 제어단말(Terminal)을 가지지 않는다.
         **/
        log_exit("setsid(2) failed: %s", strerror(errno));
}