//
// Created by 123456 on 2021/8/22.
// 程序介绍 : 使用进程池实现的的CGI(通用网关接口)服务器
//

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/epoll.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/stat.h>

/* 使用进程池 */
#include "proccesspool.h"

/* 用于处理客户端CGI请求的类，它可以作为线程池类的模板参数 */
class cgi_conn
{
public:
    cgi_conn() {}
    ~cgi_conn() {}

    /* 初始化客户连接，清空读缓冲区 */
    void init(int epfd, int connfd, struct sockaddr_in addr)
    {
        m_epollfd = epfd;
        m_connfd = connfd;
        m_client_addr = addr;
        memset(m_buf, '\0', M_BUFF_SIZE);
        m_read_idex = 0;
    }

    /* 这个CGI服务器要做的处理工作，执行指定的CGI程序 */
    void process()
    {
        int idx = 0;
        int ret = -1;

        /* 循环读取和分析缓存区中数据 */
        while(1)
        {
            idx = m_read_idex;
            ret = recv(m_connfd, m_buf + idx, M_BUFF_SIZE - 1 - idx, 0);
            if(ret < 0)
            {
                /* 如果读操作发生错误，则关闭客户连接，如果只是暂时没数据可读，则退出循环 */
                if(errno != EAGAIN)
                {
                    removefd(m_epollfd, m_connfd);
                }
                break;
            }
            else if(ret == 0)
            {
                /* 客户端关闭了连接，服务器也关闭对应连接 */
                removefd(m_epollfd, m_connfd);
                break;
            }
            else {
                m_read_idex += ret;
                for (; idx < m_read_idex; ++idx) {
                    /* 如果遇到\r\n就开始处理客户请求 */
                    if (idx >= 1 && m_buf[idx] == '\n' && m_buf[idx - 1] == '\r') {
                        break;
                    }
                }
                if (idx == m_read_idex) {
                    /* 没有收到完整的客户请求，还需要继续接收 */
                    continue;
                }
                else
                {
                    /* 收到完整客户请求，开始处理 */
                    m_buf[idx - 1] = '\0';
                    if(access(m_buf, F_OK) == -1)
                    {
                        /* 如果客户要运行的CGI程序不存在，则断开连接 */
                        printf("can not find this file!\n");
                        removefd(m_epollfd, m_connfd);
                        break;
                    }

                    /* 创建子进程执行CGI程序 */
                    pid_t pid = fork();
                    if(pid < 0)
                    {
                        removefd(m_epollfd, m_connfd);
                        break;
                    }
                    else if(pid > 0)
                    {
                        /* 父进程只需要关闭客户连接 */
                        removefd(m_epollfd, m_connfd);
                        break;
                    }
                    else
                    {
                        /* 子进程将标准输出重定向到客户连接，并执行CGI程序 */
                        close(STDOUT_FILENO);
                        dup(m_connfd);
                        execl(m_buf, m_buf, nullptr);
                        exit(0);
                    }
                }
            }
        }
    }


private:
    static const int M_BUFF_SIZE = 1024;
    static int m_epollfd;
    int m_connfd;
    struct sockaddr_in m_client_addr;
    char m_buf[M_BUFF_SIZE];
    int m_read_idex = 0;                 /* 标志读缓冲区中已经读入的客户数据的最后一个字节的下一个位置 */
};

int cgi_conn::m_epollfd = -1;

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

    processpool<cgi_conn>* pool = processpool<cgi_conn>::create(lfd);
    if(pool)
    {
        /* 启动线程池 */
        pool->run();
        delete pool;
    }

    close(lfd);          /* 在哪个文件里创建了资源，就在哪个文件里销毁资源 */
    return 0;
}




