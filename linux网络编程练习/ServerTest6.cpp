//
// Created by 123456 on 2021/8/12.
// 程序介绍 : 内核使用SIGURG通知应用程序带外数据到来。
// 注意 : 内核通知应用程序带外数据到来的两种方法: ①I/O复用系统调用(如select)报告的异常事件 ②使用SIGURG信号
//


#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>

#define BUFF_SIZE 1024

int connfd;

/* SIGURG信号处理函数 */
static void sig_urg(int sig)
{
    /* 保存errno状态, 信号处理完再恢复，使程序可重入 */
    int old_errno = errno;
    char buf[BUFF_SIZE];
    memset(buf, '\0', BUFF_SIZE);
    printf("recv oob before\n");
    int ret = recv(connfd, buf, BUFF_SIZE - 1, MSG_OOB);
    printf("recv oob after\n");
    assert(ret > 0);
    printf("get %d bytes oob data: %s\n", ret, buf);
    errno = old_errno;
}

/* 设置信号处理函数 */
void add_sig(int sig, void (*sig_handler)(int))
{
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = sig_handler;
    sa.sa_flags |= SA_RESTART;
    sigemptyset(&sa.sa_mask);
    assert(sigaction(sig, &sa, nullptr) != -1);
    printf("add_sig over!\n");
}

int main(int argc, char* argv[])
{
    if(argc <= 2)
    {
        printf("usage: %s ip_address port_num\n", basename(argv[0]));
        return 1;
    }

    const char* ip = argv[1];
    int port = atoi(argv[2]);
    struct sockaddr_in server_addr;
    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_pton(AF_INET, ip , &server_addr.sin_addr);

    int lfd = socket(AF_INET, SOCK_STREAM, 0);
    assert(lfd >= 0);

    int ret = bind(lfd, (struct sockaddr*)&server_addr, sizeof(server_addr));
    assert(ret != -1);

    ret = listen(lfd, 5);
    assert(ret != -1);

    struct sockaddr_in client_addr;
    socklen_t client_addrlen = sizeof(client_addrlen);
    connfd = accept(lfd, (struct sockaddr*)&client_addrlen, &client_addrlen);
    if(connfd < 0)
    {
        printf("errno = %d\n", errno);
    }
    else
    {
        /* 设置信号处理函数 */
        add_sig(SIGURG, sig_urg);

        /* 在使用信号SIGURG前要将文件描述符绑定宿主进程或宿主进程组 */
        fcntl(connfd, F_SETOWN, getpid());

        /* 循环接收普通用户数据 */
        char buf[BUFF_SIZE];
        while(1)
        {
            memset(buf, '\0', BUFF_SIZE);
            ret = recv(connfd, buf, BUFF_SIZE - 1, 0);
            if(ret <= 0)
            {
                break;
            }
            printf("recv %d bytes normal data: %s\n", ret, buf);
        }
        close(connfd);
    }
    close(lfd);
    return 0;
}
