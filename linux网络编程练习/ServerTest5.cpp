//
// Created by 123456 on 2021/8/11.
// 程序介绍 : 可同时处理tcp请求和udp请求的回射服务器
//

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>
#include <string.h>
#include <fcntl.h>
#include <pthread.h>

#define MAX_EVENT_NUM 1024
#define TCP_BUFFER_SIZE 512
#define UDP_BUFFER_SIZE 1024

int set_fd_nonblock(int fd)
{
    int old_opt = fcntl(fd, F_GETFL);
    int new_opt = old_opt | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_opt);
    return old_opt;
}

void add_fd(int epfd, int fd)
{
    /* 注意在ET工作模式(属于异步)下的文件描述符必须是非阻塞的 */
    epoll_event event;
    event.events = EPOLLIN | EPOLLET;
    event.data.fd = fd;
    epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &event);
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
    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    inet_pton(AF_INET, ip, &address.sin_addr);

    /* 创建tcp流式套接字 */
    int lfd = socket(AF_INET, SOCK_STREAM, 0);
    assert(lfd >= 0);

    int ret = bind(lfd, (struct sockaddr*)&address, sizeof(address));
    assert(ret != -1);

    ret = listen(lfd, 5);
    assert(ret != -1);

    /* 创建udp数据报套接字 */
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    inet_pton(AF_INET, ip, &address.sin_addr);

    int udpfd = socket(AF_INET, SOCK_DGRAM, 0);
    assert(lfd >= 0);

    ret = bind(udpfd, (struct sockaddr*)&address, sizeof(address));
    assert(ret != -1);

    /* 创建epoll内核事件表并注册lfd和udpfd上的可读事件 */
    int epfd = epoll_create(5);
    assert(epfd != -1);

    struct epoll_event events[MAX_EVENT_NUM];

    add_fd(epfd, lfd);
    add_fd(epfd, udpfd);

    while(1)
    {
        ret = epoll_wait(epfd, events, MAX_EVENT_NUM, -1);
        if(ret < 0)
        {
            printf("epoll failed！\n");
            break;
        }

        for(int i = 0; i < ret; ++i)
        {
            int sockfd = events[i].data.fd;
            /* 如果是监听到lfd上的可读事件 则接受客户端的tcp请求并建立连接 */
            if(sockfd == lfd)
            {
                struct sockaddr_in client_addr;
                socklen_t client_addrlen = sizeof(client_addr);
                int connfd = accept(sockfd, (struct sockaddr*)&client_addr, &client_addrlen);
                assert(connfd >= 0);

                add_fd(epfd, connfd);
            }
            /* 如果是监听到udpfd上的可读事件，不需要建立连接，直接回射客户数据 */
            else if(sockfd == udpfd)
            {
                char buf[UDP_BUFFER_SIZE];
                memset(buf, '\0', UDP_BUFFER_SIZE);
                struct sockaddr_in client_addr;
                socklen_t client_addrlen = sizeof(client_addr);
                int res = recvfrom(udpfd, buf, UDP_BUFFER_SIZE - 1, 0, (struct sockaddr*)&client_addr, &client_addrlen);
                if(res < 0)
                {
                    /* 因为udp不保证稳定交付数据，所以接收不到也很正常 */
                    printf("udp recv failure\n");
                    continue;
                }

                sendto(udpfd, buf, UDP_BUFFER_SIZE - 1, 0, (struct sockaddr*)&client_addr, client_addrlen);
            }
            /* 处理tcp套接字connfd的可读事件 */
            else if(events[i].events & EPOLLIN)
            {
                char buf[TCP_BUFFER_SIZE];
                while(1)
                {
                    memset(buf, '\0', TCP_BUFFER_SIZE);
                    int res = recv(sockfd, buf, TCP_BUFFER_SIZE - 1, 0);
                    if(res < 0)
                    {
                        /* 因为工作在ET模式(异步)下, 所以需要使用errno和返回值来判断是哪种情况 */
                        if((errno == EAGAIN) || (errno == EWOULDBLOCK))
                        {
                            break;
                        }
                        close(sockfd);
                        printf("tcp recv failed!\n");
                        break;
                    }
                    else if(res == 0)
                    {
                        printf("client has closed the connection!\n");
                        close(sockfd);
                    }
                    else
                    {
                        send(sockfd, buf, TCP_BUFFER_SIZE - 1, 0);
                    }
                }
            }
            else
            {
                printf("something else happend!\n");
            }
        }
    }
    close(lfd);
    close(udpfd);
    return 0;
}

