#include <stdio.h>
#include <pwd.h>
#include <unistd.h>
#include <sys/types.h>

int main(int argc, char *argv[])
{
    uid_t uid;
    struct passwd *user;

    uid = getuid();
    printf("UID=%d\n", uid);

    user = getpwuid(uid);
    printf("NAME=%s\n", user->pw_name);
    printf("PASSWORD=%s\n", user->pw_passwd);
    printf("UID=%d\n", user->pw_uid);
    printf("GID=%d\n", user->pw_gid);
    printf("DIR=%s\n", user->pw_dir);
    printf("SHELL=%s\n", user->pw_shell);
}