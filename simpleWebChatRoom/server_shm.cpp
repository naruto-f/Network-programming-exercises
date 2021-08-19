//
// Created by 123456 on 2021/8/18.
// 程序介绍 : 使用共享内存作为用户读缓冲区的简易聊天室服务器, 是Chatroom_server的升级版
//

#include <sys/socket.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/epoll.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#define USER_LIMIT 5             /* 聊天室中最多能同时容纳的用户数 */
#define MAX_EVENT_NUM 1024       /* epoll的最多事件数量 */
#define BUFF_SIZE 1024           /* 用户读缓冲区的大小 */
#define FD_LIMIT 65535           /* 最大文件描述符 */
#define PROCESS_LIMIT 65536      /* 进程数量限制 */

/* 处理一个客户连接必要的数据 */
struct client_data
{
    struct sockaddr_in addr;
    int sockfd;
    pid_t pid;                  /* 处理sockfd对应的客户连接的子进程编号 */
    int pipefd[2];              /* 和父进程通信的管道 */
};

static const char* shm_name = "my_shm";
struct client_data* user = nullptr;
int shmfd;
char* share_mem = nullptr;
int* sub_process = nullptr;
/* 当前用户数量 */
int user_count = 0;
bool stop_child = false;
/* 用于统一事件源的管道 */
int sig_pipefd[2];

void del_source()
{
    close(sig_pipefd[0]);
    close(sig_pipefd[1]);
    delete [] user;
    delete [] sub_process;
    unlink(shm_name);
}

int set_fd_nonblock(int fd)
{
    int old_opt = fcntl(fd, F_GETFL);
    int new_opt = old_opt | O_NONBLOCK;
    fcntl(fd, new_opt);
    return old_opt;
}

void addfd(int epfd, int fd)
{
    epoll_event e;
    e.data.fd = fd;
    e.events = EPOLLIN | EPOLLET;
    epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &e);
    set_fd_nonblock(fd);
}

void sig_handler(int sig)
{
    int old_errno = errno;
    int msg = sig;
    send(sig_pipefd[1], (char*)&msg, 1, 0);
    errno = old_errno;
}

void child_sig_handler(int sig)
{
    stop_child = true;
}

void addsig(int sig, void( *handler )(int sig), bool restart = true)
{
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;
    if(restart)
    {
        sa.sa_flags |= SA_RESTART;
    }
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, nullptr) != -1);
}

int child_run(int idx, client_data* users, char* shared_mem)
{
    /* 子进程需要处理与客户端的连接和与父进程的管道通信 */
    epoll_event epet_child[MAX_EVENT_NUM];
    int epfd_child = epoll_create(5);
    assert(epfd_child != -1);

    int connfd = users[idx].sockfd;
    int pipefd = users[idx].pipefd[1];
    addfd(epfd_child, connfd);
    addfd(epfd_child, pipefd);

    /* 子进程需要设置自己的信号处理函数 */
    addsig(SIGTERM, child_sig_handler, false);

    while(!stop_child)
    {
        int num = epoll_wait(epfd_child, epet_child, MAX_EVENT_NUM, -1);
        if(num < 0 && errno != EINTR)
        {
            printf("epoll failed!\n");
            stop_child = true;
        }

        for(int i = 0; i < num; ++i)
        {
            int sockfd = epet_child[i].data.fd;
            /* 客户端发来新消息 */
            if(sockfd == connfd && epet_child[i].events & EPOLLIN)
            {
                memset(shared_mem + idx * BUFF_SIZE, '\0', BUFF_SIZE);
                int ret = recv(sockfd, shared_mem + idx * BUFF_SIZE, BUFF_SIZE - 1, 0);
                if(ret < 0)
                {
                    if(errno != EAGAIN)
                    {
                        stop_child = true;
                    }
                }
                else if(ret == 0)
                {
                    printf("client connection has closed!\n");
                    stop_child = false;
                }
                else
                {
                    /* 告诉主进程自己收到了客户端数据 */
                    send(pipefd, (char*)&idx, sizeof(idx), 0);
                }
            }
            /* 收到主进程通知，将某个进程收到的客户消息发送给客户端 */
            else if(sockfd == pipefd && epet_child[i].events & EPOLLIN)
            {
                int client = 0;
                /* 接收主进程发来的连接编号(在user数组中的偏移) */
                int ret = recv(pipefd, (char*)&client, sizeof(client), 0);
                if(ret < 0)
                {
                    if(errno != EAGAIN)
                    {
                        stop_child = true;
                    }
                }
                else if(ret == 0)
                {
                    stop_child = true;
                }
                else
                {
                    send(connfd, shared_mem + client * BUFF_SIZE, BUFF_SIZE, 0);
                }
            }
            else
            {
                continue;
            }
        }
    }
    close(connfd);
    close(pipefd);
    close(epfd_child);
    return 0;
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
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &server_addr.sin_addr);

    int lfd = socket(AF_INET, SOCK_STREAM, 0);
    assert(lfd >= 0);

    int ret = bind(lfd, (struct sockaddr*)&server_addr, sizeof(server_addr));
    assert(ret != -1);

    ret = listen(lfd, 5);
    assert(ret != -1);

    user = new client_data[USER_LIMIT + 1];

    /* sub_process数组存放子进程pid和其处理的与客户的连接文件符的映射关系 */
    sub_process = new int[PROCESS_LIMIT];
    for(int i = 0; i < PROCESS_LIMIT; ++i)
    {
        sub_process[i] = -1;
    }

    epoll_event epet[MAX_EVENT_NUM];
    int epfd = epoll_create(5);
    assert(epfd != -1);
    addfd(epfd, lfd);

    ret = socketpair(AF_UNIX, SOCK_STREAM, 0, sig_pipefd);
    addfd(epfd, sig_pipefd[0]);
    set_fd_nonblock(sig_pipefd[1]);

    addsig(SIGCHLD, sig_handler);
    addsig(SIGTERM, sig_handler);
    addsig(SIGINT, sig_handler);
    /* 当子进程退出时，让子进程自己清理分配的内存等资源 */
    addsig(SIGPIPE, SIG_IGN);

    /* 创建共享文件对象及关联内存块, mode是linux文件权限 */
    shmfd = shm_open(shm_name, O_CREAT | O_RDWR, 0666);
    printf("errno = %d\n", errno);
    assert(shmfd != -1);

    /* 将共享文件大小改为len */
    ret = ftruncate(shmfd, USER_LIMIT * BUFF_SIZE);
    assert(ret != -1);

    share_mem = (char*)mmap(nullptr, USER_LIMIT * BUFF_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shmfd,0);
    assert(share_mem != MAP_FAILED);
    close(shmfd);

    bool stop_server = false;
    bool terminate = false;
    while(!stop_server)
    {
        int num = epoll_wait(epfd, epet, MAX_EVENT_NUM, -1);
        if(num < 0 && errno != EINTR)
        {
            printf("epoll failure!\n");
            break;
        }

        for(int i = 0; i < num; ++i)
        {
            int sockfd = epet[i].data.fd;
            if(sockfd == lfd && epet[i].events & EPOLLIN)
            {
                struct sockaddr_in client_addr;
                socklen_t client_addrlen = sizeof(client_addr);
                int connfd = accept(lfd, (struct sockaddr*)&client_addr, &client_addrlen);
                if(connfd < 0)
                {
                    printf("accept failed!\n");
                    continue;
                }

                if(user_count >= USER_LIMIT)
                {
                    const char* info = "to many users!";
                    printf("%s\n", info);
                    send(connfd, info, strlen(info), 0);
                    close(connfd);
                    continue;
                }

                user[user_count].sockfd = connfd;
                user[user_count].addr = client_addr;

                ret = socketpair(AF_UNIX, SOCK_STREAM, 0, user[user_count].pipefd);
                assert(ret != -1);

                pid_t pid = fork();
                if(pid < 0)
                {
                    printf("fork failed!\n");
                    close(connfd);
                    continue;
                }
                /* 当前处于子进程 */
                else if(pid == 0)
                {
                    close(lfd);
                    close(epfd);
                    close(user[user_count].pipefd[0]);
                    close(sig_pipefd[0]);
                    close(sig_pipefd[1]);
                    child_run(user_count, user, share_mem);
                    /* 将共享内存从进程中分离 */
                    munmap((void*)share_mem, BUFF_SIZE * USER_LIMIT);
                    exit(0);
                }
                /* 位于父进程 */
                else
                {
                    close(connfd);
                    close(user[user_count].pipefd[1]);
                    addfd(epfd, user[user_count].pipefd[0]);
                    user[user_count].pid = pid;
                    /* 记录新的客户连接在user中的索引值, 并建立pid与索引值的映射关系 */
                    sub_process[pid] = connfd;
                    ++user_count;
                }
            }
            /* 处理信号 */
            else if(sockfd == sig_pipefd[0] && epet[i].events & EPOLLIN)
            {
                char signal[100];
                memset(signal, '\0', 100);
                ret = recv(sockfd, signal, 100, 0);
                if(ret == -1)
                {
                    continue;
                }
                else if(ret == 0)
                {
                    continue;
                }
                else
                {
                    for(int i = 0; i < ret; ++i)
                    {
                        switch (signal[i]) {
                            case SIGCHLD:
                            {
                                pid_t pid;
                                int stat;
                                /* 有某个子进程退出 */
                                while((pid = waitpid(-1, &stat, WNOHANG)) > 0)
                                {
                                    int del_user = sub_process[pid];
                                    /* 防止取到的连接编号不合法，先将sub_process[del_user]置为-1 */
                                    sub_process[del_user] = -1;
                                    if(del_user < 0 || del_user > USER_LIMIT)
                                    {
                                        continue;
                                    }
                                    /* 清除del_user号连接使用的相关数据 */
                                    epoll_ctl(epfd, EPOLL_CTL_DEL, user[del_user].pipefd[0], 0);
                                    close(user[del_user].pipefd[0]);
                                    /* 将user数组最后一个元素覆盖到要删除的连接位置，并将用户总数减一 */
                                    user[del_user] = user[--user_count];
                                    sub_process[user[del_user].pid] = del_user;
                                }
                                if(terminate && user_count == 0)
                                {
                                    stop_server = true;
                                }
                            }
                                break;
                            case SIGTERM:
                            case SIGINT:
                                /* 向所有子进程发送SIGTERM信号使其关闭 */
                                printf("kill all the child now!\n");
                                if(user_count == 0)
                                {
                                    stop_server = true;
                                    break;
                                }
                                for(int i = 0; i < user_count; ++i)
                                {
                                    kill(user[i].pid, SIGTERM);
                                }
                                terminate = true;
                                break;
                            default:
                                break;
                        }
                    }
                }
            }
            /* 收到某个子进程发来的收到客户消息的共享内存(偏移 / BUFF_SIZE) */
            else if(epet[i].events & EPOLLIN)
            {
                int idex = 0;
                ret = recv(sockfd, (char*)&idex, sizeof(idex), 0);
                if(ret < 0)
                {
                    continue;
                }
                else if(ret == 0)
                {
                    continue;
                }
                else
                {
                    /* 向除了向主进程发送数据的其他所有子进程发送接收到的信息在共享内存中的偏移 */
                    if(idex < 0)
                    {
                        continue;
                    }
                    for(int j = 0; j < user_count; ++j)
                    {
                        if(sockfd == user[j].pipefd[0])
                        {
                            continue;
                        }
                        printf("send data to child across pipe!\n");
                        send(user[j].pipefd[0], (char*)&idex, sizeof(idex), 0);
                    }
                }
            }
            else
            {
                printf("somthing else happend!\n");
            }
        }
    }
    del_source();
    close(lfd);
    close(epfd);
    return 0;
}