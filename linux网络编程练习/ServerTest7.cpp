//
// Created by 123456 on 2021/8/13.
// 程序介绍 : 统一事件源(信号和I/O事件)的服务器，即将信号当成一种事件处理。
//

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <signal.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>

#define MAX_EVENT_NUM 1024
#define BUFF_SIZE 1024

static int pipe_fd[2];

/* 信号处理函数功能: 将信号写入管道，以通知主循环 */
void sig_handler(int sig)
{
    /* 保存信号处理前的errno，处理完信号后恢复，保证了程序的可重入性 */
    int old_errno = errno;
    int msg = sig;
    /* 因为目前信号总数不超过255个，实际上一个字节可以表示所有信号，又因为网络字节序是大端，低位在高字节，所以发一个字节就能知道是哪个信号 */
    send(pipe_fd[1], (char*)&msg, 1, 0);
    errno = old_errno;
}

void add_sig(int sig)
{
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = sig_handler;
    sa.sa_flags |= SA_RESTART;
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, nullptr) != -1);
}

int set_fd_nonblock(int fd)
{
    int old_opt = fcntl(fd, F_GETFL);
    int new_opt = old_opt | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_opt);
    return old_opt;
}

void add_fd(int epfd, int fd)
{
    epoll_event e;
    e.data.fd = fd;
    e.events = EPOLLIN | EPOLLET;
    epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &e);
    set_fd_nonblock(fd);
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
    inet_pton(AF_INET, ip, &server_addr.sin_addr);

    int lfd = socket(AF_INET, SOCK_STREAM, 0);
    assert(lfd >= 0);

    int ret = bind(lfd, (struct sockaddr*)&server_addr, sizeof(server_addr));
    assert(ret != -1);

    ret = listen(lfd, 5);
    assert(ret != -1);

    struct epoll_event epet[MAX_EVENT_NUM];
    int epfd = epoll_create(5);
    add_fd(epfd, lfd);

    /* 创建一对管道描述符并使用epoll监听该管道读端的可读事件,若可读则通知主程序，从而实现了I/0事件和信号的统一处理 */
    ret = socketpair(AF_UNIX, SOCK_STREAM, 0, pipe_fd);
    assert(ret != -1);
    set_fd_nonblock(pipe_fd[1]);
    add_fd(epfd,pipe_fd[0]);

    add_sig(SIGHUP);
    add_sig(SIGCHLD);
    add_sig(SIGTERM);
    add_sig(SIGINT);

    bool stop_sever = false;
    while(!stop_sever)
    {
        bzero(epet, sizeof(epet));
        ret = epoll_wait(epfd, epet, MAX_EVENT_NUM, -1);
        if(ret < 0)
        {
            perror("epoll_wait failed");
            break;
        }

        for(int i = 0; i < ret; ++i)
        {
            int sockfd = epet[i].data.fd;
            if((sockfd == lfd) && (epet[i].events & EPOLLIN))
            {
                struct sockaddr_in client_addr;
                socklen_t client_addrlen = sizeof(client_addrlen);
                int connfd = accept(lfd, (struct sockaddr*)&client_addr, &client_addrlen);
                assert(connfd >= 0);
                add_fd(epfd, connfd);
            }
            else if((sockfd == pipe_fd[0]) && epet[i].events & EPOLLIN)
            {
                char signals[BUFF_SIZE];
                ret = recv(pipe_fd[0], signals, sizeof(signals), 0);
                if(ret <= 0)
                {
                    break;
                }

                /* 实现让服务器安全停止的一种方法 */
                for(int i = 0; i < ret; ++i)
                {
                    /* 因为一个信号一个字节所以没每一个字节判断一次 */
                    switch (signals[i])
                    {
                        case SIGHUP:
                            printf("SIGHUP triger!\n");
                        case SIGCHLD:
                            printf("SIGCHLD triger!\n");
                            continue;
                        case SIGTERM:
                            printf("SIGTERM triger!\n");
                        case SIGINT:
                        {
                            printf("SIGINT triger!\n");
                            stop_sever = true;
                        }
                    }
                }
            }
            else{
                printf("something else happend!\n");
            }
        }
    }
    close(lfd);
    close(pipe_fd[0]);
    close(pipe_fd[1]);
    return 0;
}

