//
// Created by 123456 on 2021/8/11.
// 程序介绍 : 使用系统调用poll实现的简易聊天室服务器程序
//

/* 因为使用了POLLRDHUP事件，所以我们要在代码最开始处定义_GNU_SOURCE */
#define _GNU_SOURCE 1
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>

#define BUFF_SIZE 64              /* 读缓冲区大小 */
#define USER_LIMIT 5              /* 最大用户数量 */
#define MAX_FD_NUM 1024           /* 文件描述符数量限制 */

struct client_data
{
    struct sockaddr_in addr;      /* 客户socket地址 */
    char* client_write;           /* 待写到客户端数据的位置 */
    char read_buf[BUFF_SIZE];     /* 用于存放该用户发来的数据的缓冲区 */
};

int set_fd_nonblock(int fd)
{
    int old_opt = fcntl(fd, F_GETFL);
    int new_opt = old_opt | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_opt);
    return old_opt;
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

    struct client_data* user = new client_data[MAX_FD_NUM];
    pollfd fds[USER_LIMIT + 1];
    int user_count = 0;

    /* 初始化pollfd类型的结构体数组 */
    fds[0].fd = lfd;
    fds[0].events = POLLIN | POLLERR;
    fds[0].revents = 0;

    for(int i = 1; i <= USER_LIMIT; ++i)
    {
        fds[i].fd = -1;
        fds[i].events = 0;
        fds[i].revents = 0;
    }

    while(1)
    {
        ret = poll(fds, USER_LIMIT + 1, -1);
        if(ret < 0)
        {
            printf("poll failure!\n");
            break;
        }

        /* 轮询fds数组 */
        for(int j = 0; j <= user_count; ++j)
        {
            /* 有客户端连接请求 */
            if((fds[j].fd == lfd) && (fds[j].revents & POLLIN))
            {
                struct sockaddr_in client_addr;
                socklen_t client_addrlen = sizeof(client_addr);

                int connfd = accept(lfd, (struct sockaddr*)&client_addr, &client_addrlen);
                if(connfd < 0)
                {
                    printf("connection failure!\n");
                    continue;
                }

                /* 已达到系统允许用户数量上限 */
                if(++user_count > USER_LIMIT)
                {
                    const char* info = "to many users!";
                    printf("%s\n", info);
                    send(connfd, info, strlen(info), 0);
                    close(connfd);
                    --user_count;
                    continue;
                }

                set_fd_nonblock(connfd);
                fds[user_count].fd = connfd;
                fds[user_count].events = POLLIN | POLLRDHUP | POLLERR;
                fds[user_count].revents = 0;
                user[connfd].addr = client_addr;
                printf("comes a new user, now we have %d users.\n", user_count);
            }

            /* 捕获到错误(POLLERR)事件, 使用getsockopt获取并清除错误 */
            else if(fds[j].revents & POLLERR)
            {
                printf("get an error from %d\n", fds[j].fd);
                char errors[100];
                memset(errors, '\0', 100);
                socklen_t errlen = sizeof(errors);
                ret = getsockopt(fds[j].fd, SOL_SOCKET, SO_ERROR, errors, &errlen);
                if(ret < 0)
                {
                    printf("get socket opt failed!\n");
                }
                continue;
            }

            /* 客户端断开了连接 */
            else if(fds[j].revents & POLLRDHUP)
            {
                user[fds[j].fd] = user[fds[user_count].fd];
                close(fds[j].fd);
                fds[j] = fds[user_count];
                --j;
                --user_count;
                printf("a clint left!\n");
            }

            /* connfd可读事件, 即收到某个客户发来的数据 */
            else if(fds[j].revents & POLLIN)
            {
                memset(user[fds[j].fd].read_buf, '\0', BUFF_SIZE);
                ret = recv(fds[j].fd, user[fds[j].fd].read_buf, BUFF_SIZE - 1, 0);
                printf("recv %d bytes data from %d: %s\n", ret, fds[j].fd, user[fds[j].fd].read_buf);
                if(ret < 0)
                {
                    /* 如果读操作失败，则关闭连接, errno等于EAGAIN说明已经完成读取，缓冲区中没有数据可读了 */
                    if(errno != EAGAIN)
                    {
                        printf("recv failed!\n");
                        user[fds[j].fd] = user[fds[user_count].fd];
                        close(fds[j].fd);
                        fds[j] = fds[user_count];
                        --j;
                        --user_count;
                    }
                }
                else if(ret == 0)
                {
                    /* 客户端关闭连接的情况在前面POLLRDHUP事件就已经处理 */
                }
                else {
                    /* 通知除当前接收数据的文件描述符外的其他文件描述符准备写 */
                    for (int i = 1; i <= user_count; ++i) {
                        if (fds[j].fd == fds[i].fd) {
                            continue;
                        }
                        fds[i].events &= ~POLLIN;
                        fds[i].events |= POLLOUT;
                        user[fds[i].fd].client_write = user[fds[j].fd].read_buf;
                    }
                }
            }

            /* 往没有收到数据的其他connfd上写数据 */
            if(fds[j].revents & POLLOUT)
            {
                ret = send(fds[j].fd, user[fds[j].fd].client_write, strlen(user[fds[j].fd].client_write), 0);
                user[fds[j].fd].client_write = nullptr;
                if(ret < 0)
                {
                    printf("send failed!\n");
                    continue;
                }
                /* 写完之后要重新注册此connfd上的读事件(POLLIN) */
                fds[j].events &= ~POLLOUT;
                fds[j].events |= POLLIN;
            }
        }
    }
    delete [] user;
    close(lfd);
    return 0;
}
