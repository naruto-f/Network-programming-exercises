//
// Created by 123456 on 2021/8/14.
// 程序功能 : 使用定时器实现关闭非活动连接功能的服务器程序
//

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include <assert.h>
#include <pthread.h>
#include <string.h>
#include "lst_timer.h"

#define FD_LIMIT 65535               /* 最大文件描述符熟练限制 */
#define MAX_EVENT_NUM 1024           /* 最多事件数*/
#define TIMESLOT 5                   /* 超时时间 */

static int pipe_fd[2];
static int epfd = 0;
static sort_timer_lst lst_timer;

void timer_handler()
{
    /* 定时处理任务，实际上就是调用tick函数 */
    lst_timer.tick();

    /* 因为一次alarm调用只会触发一次SIGALRM信号, 所以我们要重新定时，以不断触发SIGALRM信号 */
    alarm(TIMESLOT);
}

/* 任务回调函数，在本程序中为注销目标文件描述符的epoll事件并关闭连接 */
void cb_func(struct client_data* cd)
{
    epoll_ctl(epfd, EPOLL_CTL_DEL, cd->sockfd, 0);
    assert(cd);
    close(cd->sockfd);
    printf("close the fd %d！\n", cd->sockfd);
}

void sig_handler(int sig)
{
    /* 保存errno值，使程序保持可重入性 */
    int old_errno = errno;
    int msg = sig;
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
    struct epoll_event e;
    e.data.fd = fd;
    e.events = EPOLLIN | EPOLLET;
    epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &e);
    set_fd_nonblock(fd);
}

int main(int argc, char* argv[])
{
    if(argc <= 2)
    {
        printf("usage: %s ip_adderss port_num\n", basename(argv[0]));
        return 1;
    }

    const char* ip = argv[1];
    int port = atoi(argv[2]);
    struct sockaddr_in server_addr;
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
    epfd = epoll_create(5);
    assert(epfd != -1);
    add_fd(epfd, lfd);

    ret = socketpair(AF_UNIX, SOCK_STREAM, 0, pipe_fd);
    assert(ret != -1);
    add_fd(epfd, pipe_fd[0]);
    set_fd_nonblock(pipe_fd[1]);

    add_sig(SIGALRM);
    add_sig(SIGTERM);
    alarm(TIMESLOT);

    /* 用空间换时间，提高程序性能 */
    struct client_data* user = new client_data[FD_LIMIT];

    bool stop_server = false;
    /* 用于标记是否有定时器超时，因为定时任务的优先级较其他任务(如I/O)低，所以不立即处理，等到最后才执行，所以定时任务的处理可能会有延迟1 */
    bool timeout = false;

    while(!stop_server)
    {
        ret = epoll_wait(epfd, epet, MAX_EVENT_NUM, -1);
        if(ret < 0 && errno != EINTR)
        {
            printf("epoll failure!\n");
            break;
        }

        for(int i = 0; i < ret; ++i)
        {
            int sockfd = epet[i].data.fd;
            if((sockfd == lfd) && epet->events & EPOLLIN)
            {
                struct sockaddr_in client_addr;
                socklen_t client_addrlen = sizeof(client_addr);
                int connfd = accept(lfd, (struct sockaddr*)&client_addr, &client_addrlen);
                if(connfd < 0)
                {
                    printf("connect failed!\n");
                    break;
                }
                add_fd(epfd, connfd);
                util_timer* t = new util_timer;
                time_t cur = time(nullptr);
                t->erpire = cur + 3 * TIMESLOT;
                t->user_data = &user[connfd];
                t->cb_func = cb_func;
                user[connfd].sockfd = connfd;
                user[connfd].addr = client_addr;
                user[connfd].timer = t;
                lst_timer.add_timer(t);
            }
            else if(sockfd == pipe_fd[1] && epet->events & EPOLLIN)
            {
                char signals[1024];
                memset(user[sockfd].buf, '\0', BUFF_SIZE);
                ret = recv(sockfd, user[sockfd].buf, BUFF_SIZE - 1, 0);
                if(ret <= 0)
                {
                    //handle the error
                    continue;
                }
                for(int i = 0; i < ret; ++i)
                {
                    switch (signals[i])
                    {
                        case SIGALRM:
                            /* 因为超时任务优先级比其他事件低，所以先标记超时后处理其他事件 */
                            timeout = true;
                            break;
                        case SIGTERM:
                            stop_server = true;
                    }
                }
            }
            else if(epet[i].events & EPOLLIN)
            {
                memset(user[sockfd].buf, '\0', BUFF_SIZE);
                ret = recv(sockfd, user[sockfd].buf, BUFF_SIZE - 1, 0);
                printf("recv %d bytes data from %d: %s\n", ret, sockfd, user[sockfd].buf);
                if(ret < 0)
                {
                    if(errno != EAGAIN)
                    {
                        cb_func(&user[sockfd]);
                        if(user[sockfd].timer)
                        {
                            /* 将该定时器从链表中移除 */
                            lst_timer.del_timer(user[sockfd].timer);
                        }
                        printf("recv failed!\n");
                    }
                }
                else if(ret == 0)
                {
                    /* 客户端关闭了连接，服务端也关闭 */
                    cb_func(&user[sockfd]);
                    if(user[sockfd].timer)
                    {
                        /* 将该定时器从链表中移除 */
                        lst_timer.del_timer(user[sockfd].timer);
                    }
                }
                else
                {
                    /* 如果某个客户连接上有数据可读，那么我们要调整连接对应的定时器，以延迟该连接被关闭的时间 */
                    time_t cur = time(nullptr);
                    user[sockfd].timer->erpire = cur + 3 * TIMESLOT;
                    printf("adjust timer once!\n");
                    lst_timer.adjust(user[sockfd].timer);
                }
            }
            else{
                printf("something else happend!\n");
            }
        }
        if(timeout)
        {
            timer_handler();
            timeout = false;
        }
    }
    delete [] user;
    close(lfd);
    close(pipe_fd[0]);
    close(pipe_fd[1]);
    return 0;
}

