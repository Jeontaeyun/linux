/*** 
  * cat은 Concatenate(연결하다)의 약자로 
  * 파일과 파일을 연결하는 명령어 입니다.
  * Linux에서 cat 명령은 파일의 내용을 간단하게 출력할 때도 사용되지만 두 개 이상의 파일을 연결해서 출력 할 때 많이 쓰입니다.
***/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

/***
 * main에서 구현되지 않은 함수를 쓰기 위해서 상단부에 선언해준다.
 ***/
static void do_cat(const char *path);
static void die(const char *s);

int main(int argc, char *argv[])
{
    int i;
    if (argc < 2)
    {
        fprintf(stderr, "%s: file name not given\n", argv[0]);
        exit(1);
    }
    for (i = 1; i < argc; i++)
    {
        /***
         * argv[1]번 부터 인자를 가지고 있다.
         * do_cat에게 책임을 Delegate(위임)한다
         ***/
        do_cat(argv[i]);
        exit(0);
    }
}

/***
 * #define : C 언어 상수 선언
 * const : 변수의 초기값을 변경할 수 없는 변수를 선언할 때 사용
 ***/
#define BUFFER_SIZE 2048

static void do_cat(const char *path)
{
    int fd;
    unsigned char buf[BUFFER_SIZE];
    int n;

    /***
    * open 시스템 콜은 커널에서 파일을 열고 fd(파일디스크립터)를 반환한다.
    * fd는 반드시 0 이상의 숫자이다.
    ***/
    fd = open(path, O_RDONLY);
    if (fd < 0)
        die(path);
    /***
    * C언어에서 무한 루프 프로세스를 만들기 위한 방법
    ***/
    for (;;)
    {
        n = read(fd, buf, sizeof buf);
        if (n < 0)
            die(path);
        if (n == 0)
        /***
         * 모든 파일은 마지막에 \0을 가진다. 
         ***/
        {
            break;
        }
        /***
         * STDOUT_FILENO는 표준 출력을 의미하는 매크로 파일 디스크립터
         * 이 부분에서는 쉘을 말한다.
         ***/
        if (write(STDOUT_FILENO, buf, n) < 0)
            die(path);
    }
    if (close(fd) < 0)
        die(path);
}

static void die(const char *s)
{
    perror(s);
    exit(1);
}