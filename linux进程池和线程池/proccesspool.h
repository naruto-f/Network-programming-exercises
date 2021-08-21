//
// Created by 123456 on 2021/8/21.
// 库文件介绍 : 半同步/半异步进程池
//

#ifndef LINUX_PROCCESSPOOL_H
#define LINUX_PROCCESSPOOL_H

#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <signal.h>
#include <fcntl.h>

/* 存放子进程信息的数据结构 */
class process
{
public:
    process() : m_pid(-1) { }

public:
    pid_t m_pid;          /* 子进程的进程编号*/
    int m_pipefd[2];      /* 子进程与父进程通信的管道 */
};

/* 进程池, 模板参数T为进行业务逻辑处理的类 */
template<class T>
class processpool
{
private:
    /* 这里使用设计模式中的单例模式, 将构造函数声明为private, 在一个对外公开的接口中调用默认构造函数，并返回一个类的静态实例, 从而保证了一个程序中只有唯一额进程池实例 */
    processpool(int lfd, int pool_size = 8);
public:
    /* 对外提供的创建进程池的静态公共接口, 保证了一个程序中只有一个进程池实例 */
    static processpool<T>* create(int lfd, int pool_size)
    {
        if(!m_instance)
        {
            m_instance = new processpool(lfd, pool_size);
        }
        return m_instance;
    }

    /* 进程池的析构函数，除了执行进程池的指针外都是内置数据类型，所以只需处理进程池指针 */
    ~processpool()
    {
        delete [] m_sub_process;
    }

    /* 启动进程池 */
    void run();
private:
    /* 设置信号处理函数和初始化用于统一事件源的信号管道*/
    void setup_sig_pipe();
    void run_parent();
    void run_child();

    static const int MAX_CHILD_PROCESS = 16;        /* 进程池最多能存放的进程数 */
    static const int USER_PER_PROCESS = 65535;      /* 每个子进程最多能处理的客户连接数 */
    static const int MAX_EVENT_NUM = 10000;         /* 一次epoll调用最多能返回的事件数 */
    int listenfd;                                   /* 初始化进程池时由外部传入的监听socket */
    int m_epollfd;                                  /* 每一个子进程都有自己的一个epollfd */
    int pool_content;                               /* 当前进程池的容量(即进程池中的子进程数量) */
    int m_idx;                                      /* 子进程在进程池中的索引, 从0开始 */
    bool m_stop;                                    /* 子进程通过m_stop判断是否停止运行 */
    process* m_sub_process;                         /* 执行进程池(即process的数组)的指针 */
    static processpool<T>* m_instance;              /* 进程池的一个静态实例 */
};
/* 类的静态成员变量的初始化要在类外 */
template<class T>
processpool<T>* processpool<T>::m_instance = nullptr;

/* 用于统一事件源的信号管道 */
static int sig_pipefd[2];

/* static修饰普通函数，则此函数的作用域被限定在定义它的文件中，即使其他文件包含此头文件，也不能使用这些static函数 */
static int set_fd_nonblock(int fd)
{
    int old_opt = fcntl(fd, F_GETFL);
    int new_opt = old_opt | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_opt);
    return old_opt;
}

static void addfd(int epollfd, int fd)
{
    epoll_event e;
    e.data.fd = fd;
    e.events = EPOLLIN | EPOLLET;
    int ret = epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &e);
    assert(ret != -1);
    set_fd_nonblock(fd);
}

static void removefd(int epollfd, int fd)
{
    int ret = epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, nullptr);
    assert(ret != -1);
    close(fd);
}

/* 用于统一事件源的事件处理函数 */
static void sig_handler(int sig)
{
    int save_no = errno;           /* 保存信号处理前的errno，使函数可重入 */
    int msg = sig;
    send(sig_pipefd[1], (char*)&msg, sizeof(msg), 0);
    errno = save_no;
}

static void addsig(int sig, void( *handler )(int), bool restart = true)
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

/* 接收一个监听套接字和一个进程池容量作为参数的构造函数 */
template<class T>
processpool<T>::processpool(int lfd, int pool_size) : listenfd(lfd), pool_content(pool_size), m_idx(-1), m_stop(false)
{
    assert(pool_content > 0 && pool_content <= MAX_CHILD_PROCESS);

    m_sub_process = new process[pool_content];
    assert(m_sub_process);

    for(int i = 0; i < pool_content; ++i)
    {
        int ret = socketpair(AF_UNIX, SOCK_STREAM, 0, m_sub_process[i].m_pipefd);
        assert(ret != -1);

        m_sub_process[i].m_pid = fork();
        assert(m_sub_process[i].m_pid >= 0);
        if(m_sub_process[i].m_pid > 0)
        {
            /* 当前位于父进程 */
            close(m_sub_process[i].m_pipefd[1]);
            continue;
        }
        else
        {
            /* 当前位于子进程 */
            close(m_sub_process[i].m_pipefd[0]);
            m_idx = i;
            break;
        }
    }
}

template<class T>
void processpool<T>::setup_sig_pipe()
{
    m_epollfd = epoll_create(5);
    assert(m_epollfd != -1);

    int ret = socketpair(AF_UNIX, SOCK_STREAM, 0, sig_pipefd);
    assert(ret != -1);
    set_fd_nonblock(sig_pipefd[1]);
    addfd(m_epollfd ,sig_pipefd[0]);

    addsig(SIGCHLD, sig_handler);
    addsig(SIGTERM, sig_handler);
    addsig(SIGINT, sig_handler);
    addsig(SIGPIPE, SIG_IGN);
}

template<class T>
void processpool<T>::run() {
    if(m_idx != -1)
    {
        run_child();
        return;
    }
    run_parent();
}

template<class T>
void processpool<T>::run_parent() {
    setup_sig_pipe();
    addfd(m_epollfd, listenfd);

    epoll_event ev[MAX_EVENT_NUM];
    int num = 0;
    int new_conn = 1;
    int child_counter = 0;
    int ret = -1;
    while(!m_stop)
    {
        num = epoll_wait(m_epollfd, ev, MAX_EVENT_NUM, -1);
        if(num < 0 && errno != EINTR)
        {
            printf("epoll failuer!\n");
            break;
        }

        for(int i = 0; i < num; ++i)
        {
            int sockfd = ev[i].data.fd;
            if(sockfd == listenfd)
            {
                /* 主进程接收到客户连接请求，使用rr算法为客户从进程池中分配一个空闲的子进程 */
                int j = child_counter;
                do
                {
                    if(m_sub_process[j].m_pid != -1)
                    {
                        break;
                    }
                    j = (j + 1) % pool_content;
                }while(j != child_counter);
                if(m_sub_process[j].m_pid == -1)
                {
                    m_stop = true;
                    break;
                }
                child_counter = (i + 1) / pool_content;

                /* 向选中的空闲子进程发送特定信息让子进程准备接收客户连接 */
                send(m_sub_process[j].m_pipefd[0], (char*)&new_conn, sizeof(new_conn), 0);
                printf("send request to child!\n");
            }
            else if(sockfd == sig_pipefd[0])
            {
                /* 处理信号 */
                char signal[1024];
                memset(signal, '\0', 100);
                ret = recv(sockfd, signal, sizeof(signal), 0);
                if((ret < 0 && ret != EAGAIN) || ret == 0)
                {
                    continue;
                }

                for(int i = 0; i < ret; ++i)
                {
                    switch (signal[i])
                    {
                        case SIGCHLD:
                        {
                            pid_t pid = 0;
                            int stat;
                            /* 有某个子进程退出 */
                            while((pid = waitpid(-1, &stat, WNOHANG)) > 0)
                            {
                                /* 遍历进程池，将退出子进程的信息消除 */
                                for(int j = 0; j < pool_content; ++j)
                                {
                                    if(pid == m_sub_process[j].m_pid)
                                    {
                                        m_sub_process[j].m_pid = -1;
                                        close(m_sub_process[j].m_pipefd[0]);
                                        printf("child %d join!\n", j);
                                    }
                                }
                            }

                            /* 如果所有子进程都已经退出，那么父进程也退出 */
                            m_stop = true;
                            for(int j = 0; j < pool_content; ++j)
                            {
                                if(m_sub_process[j].m_pid != -1)
                                {
                                    m_stop = false;
                                    break;
                                }
                            }
                            break;
                        }
                        case SIGTERM:
                        case SIGINT:
                        {
                            /* 父进程退出前通知所有子进程 */
                            printf("kill all the child process!\n");
                            for (int j = 0; j < pool_content; ++j)
                            {
                                if (m_sub_process[j].m_pid != -1)
                                {
                                    kill(m_sub_process[j].m_pid, SIGTERM);
                                }
                            }
                            break;
                        }
                        default:
                            break;
                    }
                }
            }
            else
            {
                continue;
            }
        }
    }
    close(m_epollfd);

}

template<class T>
void processpool<T>::run_child()
{
    setup_sig_pipe();

    int pipefd = m_sub_process[m_idx].m_pipefd[1];
    addfd(m_epollfd, pipefd);

    /* 以空间换时间，使用connfd来索引业务逻辑处理类的对象 */
    T* user = new T[USER_PER_PROCESS];
    assert(user);

    epoll_event ev[MAX_EVENT_NUM];
    int num = 0;
    int ret = -1;

    while(!m_stop)
    {
        num = epoll_wait(m_epollfd, ev, MAX_EVENT_NUM, -1);
        if(num < 0 && errno != EINTR)
        {
            printf("epoll failuer!\n");
            break;
        }

        for(int i = 0; i < num; ++i)
        {
            int sockfd = ev[i].data.fd;
            /* 接收到父进程通过管道发来的接收新客户连接的特殊标志 */
            if(sockfd == pipefd && ev[i].events & EPOLLIN)
            {
                int new_conn = 0;
                ret = recv(pipefd, (char*)&new_conn, sizeof(new_conn), 0);
                if((ret < 0 && errno != EAGAIN) || ret == 0)
                {
                    continue;
                }

                struct sockaddr_in client_addr;
                socklen_t client_addrlen = sizeof(client_addr);
                int connfd = accept(listenfd, (struct sockaddr*)&client_addr, &client_addrlen);
                if(connfd < 0)
                {
                    printf("errno = %d\n", errno);
                    continue;
                }
                addfd(m_epollfd, connfd);
                /* 逻辑处理类要实现一个init成员函数，用来初始化一个逻辑处理类对象 */
                user[connfd].init(m_epollfd, connfd, client_addr);
            }
            else if(sockfd == sig_pipefd[0] && ev[i].events & EPOLLIN)
            {
                char signal[1024];
                memset(signal, '\0', sizeof(1024));
                ret = recv(sockfd, signal, sizeof(signal), 0);
                if(ret <= 0)
                {
                    continue;
                }
                else
                {
                    for(int j = 0; j < ret; ++j)
                    {
                        switch (signal[j])
                        {
                            case SIGCHLD:
                            {
                                pid_t pid;
                                int stat;
                                while((pid = waitpid(-1, &stat, WNOHANG)) > 0)
                                {
                                    continue;
                                }
                                break;
                            }
                            case SIGTERM:
                            case SIGINT:
                            {
                                m_stop = true;
                                break;
                            }
                            default:
                                break;
                        }
                    }
                }
            }
            /* 如果不是以上两种情况且可读, 那么一定是客户端发来数据 */
            else if(ev[i].events & EPOLLIN)
            {
                /* 业务逻辑处理类需要定义一个process函数，用来实现主要功能 */
                user[sockfd].process();
            }
            else
            {
                continue;
            }
        }
    }
    delete [] user;
    user = nullptr;
    close(m_epollfd);
    close(pipefd);
}

#endif //LINUX_PROCCESSPOOL_H
