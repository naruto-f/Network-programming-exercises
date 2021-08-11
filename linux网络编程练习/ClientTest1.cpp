//
// Created by 123456 on 2021/8/10.
// 程序功能 : I/O复用的高级应用一, 使用非阻塞connect, 实现可以同时发起多个连接并一起等待。
//

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <fcntl.h>
#include <time.h>
#include <string.h>


int set_fd_nonblock(int fd)
{
    int old_opt = fcntl(fd, F_GETFL);
    int new_opt = old_opt | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_opt);
    return old_opt;
}


/* 超时连接函数，函数成功时返回已经处于连接状态的socket，失败则返回-1 */
int nonblock_connect(const char* ip, int port, int time)
{
    struct sockaddr_in server_addr;
    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &server_addr.sin_addr);

    int cfd = socket(AF_INET, SOCK_STREAM, 0);
    if(cfd < 0)
    {
        printf("socket创建失败！\n");
        return -1;
    }
    int dopt = set_fd_nonblock(cfd);

    int ret = connect(cfd, (struct sockaddr*)&server_addr, sizeof(server_addr));
    if(ret == 0)
    {
        fcntl(cfd, F_SETFL, dopt);
        printf("连接立即建立, 立即返回！\n");
        return cfd;
    }
    else if(errno != EINPROGRESS)
    {
        /* 出错返回 */
        printf("unblock connect not support！\n");
        close(cfd);
        return -1;
    }
    /* 处于没有完全建立连接，但连接还在进行，此时errno = EINPROGRESS */
    else{
        fd_set write_fds;
        FD_ZERO(&write_fds);
        FD_SET(cfd, &write_fds);
        struct timeval t = {time, 0};

        ret = select(cfd + 1, nullptr, &write_fds, nullptr, &t);
        if(ret <= 0)
        {
            /* select超时或错误, 立即返回 */
            printf("select time out!\n");
            close(cfd);
            return -1;
        }

        /* 没有监听到写事件则连接一定没有建立 */
        if(!FD_ISSET(cfd, &write_fds))
        {
            printf("no event on sockfd fund\n");
            close(cfd);
            return -1;
        }
        else{
            /* 如果监听到写事件则使用getsockopt函数获取并清除错误码, 只有当错误码为0时才是已经成功建立了连接 */
            int error = 0;
            socklen_t errlen = sizeof(error);

            ret = getsockopt(cfd, SOL_SOCKET, SO_ERROR, &error, &errlen);
            if(ret < 0)
            {
                printf("get sockopt filed!\n");
                close(cfd);
                return -1;
            }
            /* 错误号不为0，表示连接出错 */
            else if(error != 0)
            {
                printf("select with error filed: %d\n", error);
                close(cfd);
                return -1;
            }
            else{
                /* error=0, 与服务器成功建立了连接 */
                fcntl(cfd, F_SETFL, dopt);
                printf("select with fd success: %d\n", cfd);
                return cfd;
            }
        }
    }
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

    int sock = nonblock_connect(ip, port, 10);
    if(sock < 0)
    {
        return 1;
    }
    close(sock);
    return 0;
}

