//
// Created by 123456 on 2021/8/28.
// 程序功能介绍 : 简单服务器压力测试程序
//

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>


int set_fd_nonblock(int fd)
{
    int old_opt = fcntl(F_GETFL, fd);
    int new_opt = old_opt | O_NONBLOCK;
    fcntl(F_SETFL, fd, new_opt);
    return old_opt;
}

void addfd(int epfd, int fd)
{
    epoll_event e;
    e.data.fd = fd;
    e.events = EPOLLOUT | EPOLLET | EPOLLERR;
    epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &e);
    set_fd_nonblock(fd);
}

/* 删除fd在内核事件表中的注册事件，并关闭套接字 */
void removefd(int epfd, int fd)
{
    epoll_ctl(epfd, EPOLL_CTL_DEL, fd, nullptr);
    close(fd);
}

bool read_once(int fd, char* buf, int len)
{
    memset(buf, '\0', len);
    int byte_read = recv(fd, buf, len, 0);
    if(byte_read == -1)
    {
        return false;
    }
    else if(byte_read == 0)
    {
        return false;
    }
    printf("read in %d bytes from socket %d with content %s\n", byte_read, fd, buf);
    return true;
}

bool write_nbytes(int fd, char* buf, int len)
{
    int write_bytes = 0;
    printf("write out %d bytes to socket %d\n", len, fd);
    while(1)
    {
        write_bytes = send(fd, buf, len, 0);
        if(write_bytes < 0)
        {
            return false;
        }
        else if(write_bytes == 0)
        {
            return false;
        }

        len -= write_bytes;
        buf += write_bytes;
        if(len <= 0)
        {
            return true;
        }
    }
}

void send_request_n(int epfd, const char* ip, int port, int link_num)
{
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = port;
    inet_pton(AF_INET, ip, &server_addr.sin_addr);

    for(int i = 0; i < link_num; ++i)
    {
        sleep(1);
        int cfd = socket(AF_INET, SOCK_STREAM, 0);
        if(cfd < 0)
        {
            continue;
        }

        int ret = connect(cfd, (struct sockaddr*)&server_addr, sizeof(server_addr));
        if(ret == -1)
        {
            continue;
        }

        printf("connect a socket\n");
        addfd(epfd, cfd);
    }
}


int main(int argc, char* argv[])
{
    if(argc <= 3)
    {
        printf("usage: ip_address port_num link_num\n");
        return 1;
    }

    epoll_event epet[10000];
    int epfd = epoll_create(100);
    assert(epfd >= 0);

    /* 向服务器发送n个连接请求 */
    send_request_n(epfd, argv[1], atoi(argv[2]), atoi(argv[3]));

    char buffer[1024];
    while(1)
    {
        int num = epoll_wait(epfd, epet, 10000, 200);
        if(num < 0)
        {
            printf("epoll failure!\n");
            break;
        }

        for(int i = 0; i < num; ++i)
        {
            int sockfd = epet[i].data.fd;
            if(epet[i].events & EPOLLIN)
            {
                if(!read_once(sockfd, buffer, 1024))
                {
                    removefd(epfd, sockfd);
                }
                struct epoll_event e;
                e.data.fd = sockfd;
                e.events = EPOLLOUT | EPOLLET | EPOLLERR;
                epoll_ctl(epfd, EPOLL_CTL_MOD, sockfd, &e);
            }
            else if(epet[i].events & EPOLLOUT)
            {
                if(!write_nbytes(sockfd, buffer, 1024))
                {
                    removefd(epfd, sockfd);
                }
                struct epoll_event e;
                e.data.fd = sockfd;
                e.events = EPOLLIN | EPOLLET | EPOLLERR;
                epoll_ctl(epfd, EPOLL_CTL_MOD, sockfd, &e);
            }
            else if(epet[i].events & EPOLLERR)
            {
                removefd(epfd, sockfd);
            }
        }

    }
}

