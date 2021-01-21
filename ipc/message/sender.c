#include <stdio.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include <sys/msg.h>
#include <stdlib.h>
#include <string.h>
#include "msg_data.h"

static void printMsgInfo(int msq_id)
{
    // msqid_ds는 <sys/msg.h>에 정의 되어 있음
    struct msqid_ds m_stat;
    printf("========== messege queue info =============\n");
    if (msgctl(msq_id, IPC_STAT, &m_stat) == -1)
    {
        printf("msgctl failed\n");
        exit(0);
    }
    printf(" message queue info \n");
    printf(" msg_lspid : %d\n", m_stat.msg_lspid);
    printf(" msg_qnum : %lu\n", m_stat.msg_qnum);
    printf(" msg_stime : %ld\n", m_stat.msg_stime);

    printf("========== messege queue info end =============\n");
}

int main()
{
    key_t key = 12345;
    int msq_id;

    struct message msg;
    msg.msg_type = 1;
    msg.data.age = 80;
    strcpy(msg.data.name, "JEON TAE YUN");

    if ((msq_id = msgget(key, IPC_CREAT | 0666)) == -1)
    {
        printf("msgget failed \n");
        exit(0);
    }

    printMsgInfo(msq_id);

    if (msgsnd(msq_id, &msg, sizeof(struct real_data), 0) == -1)
    {
        printf("msgsnd failed\n");
        exit(0);
    }
    printf("message sent\n");
    printMsgInfo(msq_id);
}