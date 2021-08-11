//
// Created by 123456 on 2021/8/11.
// 程序介绍 : 使用系统调用poll实现的简易聊天室客户端程序
//

/* 因为使用了POLLRDHUP事件，所以我们要在代码最开始处定义_GNU_SOURCE */
#define _GNU_SOURCE 1
#include <sys/types.h>
#include <sys/socket.h>
#include <poll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <fcntl.h>

#define BUFF_SIZE 64

int main()
{
//    if(argc <= 2)
//    {
//        printf("usage: %s ip_address port_num\n", basename(argv[0]));
//        return 1;
//    }

    const char* ip = "192.168.3.31";
    int port = 8880;
    struct sockaddr_in server_addr;
    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &server_addr.sin_addr);

    int connfd = socket(AF_INET, SOCK_STREAM, 0);
    assert(connfd >= 0);

    if(connect(connfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0)
    {
        printf("connection failure!\n");
        close(connfd);
        return 1;
    }

    char read_buf[BUFF_SIZE];
    struct pollfd pfds[2];

    /* 指向标准输入(STDIN)的文件描述符为0, 使用poll监听标准输入的可读事件, 用户输入即触发可读事件 */
    pfds[0].fd = 0;
    pfds[0].events = POLLIN;
    pfds[0].revents = 0;

    /* 使用poll监听与服务器建立连接的文件描述符的可读事件和连接断开事件(POLLRDHUP)*/
    pfds[1].fd = connfd;
    pfds[1].events = POLLIN | POLLRDHUP;
    pfds[1].revents = 0;

    /* 创建使用splice重定向用户数据要使用的管道 */
    int pipe_fd[2];
    int ret = pipe(pipe_fd);
    assert(ret != -1);

    while(1)
    {
        int ret = poll(pfds, 2, -1);
        if(ret < 0)
        {
            printf("poll failure!\n");
            break;
        }

        /* 连接断开(对端关闭连接)事件*/
        if(pfds[1].events & POLLRDHUP)
        {
            printf("the remote server has closed the link!\n");
            break;
        }
        /* 内核读缓冲区可读事件 */
        else if(pfds[1].revents & POLLIN)
        {
            memset(read_buf, '\0', BUFF_SIZE);
            ret = recv(connfd, read_buf, BUFF_SIZE - 1, 0);
            if(ret < 0)
            {
                printf("recv filed!\n");
                break;
            }
            printf("%s", read_buf);
        }

        /* 如果标准输入可读则说明用户输入了数据, 此时使用splice(0拷贝)函数将用户数据重定向到connfd */
        if(pfds[0].revents & POLLIN)
        {
            ret = splice(0, NULL, pipe_fd[1], NULL, 32768, SPLICE_F_MORE | SPLICE_F_MOVE);
            ret = splice(pipe_fd[0], NULL, connfd, NULL, 32768, SPLICE_F_MORE | SPLICE_F_MOVE);
        }
    }
    close(connfd);
    return 0;
}