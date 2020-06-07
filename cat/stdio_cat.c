#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{
    int i;
    for (i = 1; i < argc; i++)
    {
        /***
         * FILE 타입은 stdio.h에서 제공하는 구조체 타입
         */
        FILE *f;
        int c;

        /**
         * fopen는 path를 받고 해당 API가 path 
         * 경로의 파일과 스트림을 형성한다. 
         */
        f = fopen(argv[i], "r");
        if (!f)
        {
            perror(argv[i]);
            exit(1);
        }
        while ((c = fgetc(f)) != EOF)
        {
            /**
             * 표준 입출력에 쓰는 바이트 단위 API 
             * 반환값으로 바이트 값을 반환
             * fgetc를 통해 스트림을 받아오고 c에 저장한 후 STDOUT API인 putchar를 통해
             * 출력하는 코드입니다.
             */
            if (putchar(c) < 0)
                exit(1);
        }
        fclose(f);
    }
    exit(0);
}