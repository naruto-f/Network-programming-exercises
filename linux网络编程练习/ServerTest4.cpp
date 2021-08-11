//
// Created by 123456 on 2021/8/9.
// 程序功能 : 演示linux自带的epoll调用LT(电平触发，默认工作模式)和ET(水平触发)的工作方式上的区别
// 总结 : 对于采用LT工作模式的文件描述符，当epoll_wait检测到其上有事件发生并通知应用程序，应用程序可以不立即处理，下次epoll_wait
//        返回时还会向应用程序通告此事件; 而采用ET工作模式的文件描述符，应用程序必须立即处理事件，后续epoll_wait不会再向应用程序通告这个事件。
// 注意 : 1.ET模式在很大程度上降低了同一个epoll事件被重复触发的次数，所以效率比EL模式要高。
//        2.每个使用ET模式的文件描述符都应该是非阻塞的。如果文件描述符是阻塞的，那么读或写操作将会因为没有后续的事件而一直处于阻塞(饥饿)状态。
//

//#include <sys/socket.h>
//#include <sys/epoll.h>
//#include <netinet/in.h>
//#include <arpa/inet.h>
//#include <unistd.h>
//#include <stdlib.h>
//#include <stdio.h>
//#include <string.h>
//#include <assert.h>
//#include <errno.h>
//#include <fcntl.h>
//
//#define MAX_EVENT_NUM 1024
//#define BUFFSIZE 10
//
//
/* 函数名称 : set_fd_nonblock
 * 参数fd : 将要设置为非阻塞的文件描述符
 * 返回值 : 文件描述符fd所有旧的属性设置的按位或
 * 函数功能 : 将文件描述符fd设置为非阻塞
 */
//int set_fd_nonblock(int fd)
//{
//    int old_option = fcntl(fd, F_GETFL);
//    int new_option = old_option | O_NONBLOCK;
//    fcntl(fd, F_SETFL, new_option);
//    return old_option;
//}
//
//
///* 函数名称 : addfd
// * 参数epfd : 指向epoll内核事件表的文件描述符
// * 参数fd : 要将自己的事件写入内核事件表的文件描述符
// * 参数et_enable : 指出是否允许ET工作模式
// * 函数功能 : 将fd上的事件注册到epfd指向的内核事件表
// */
//void addfd(int epfd, int fd, bool et_enable)
//{
//    struct epoll_event ep;
//    ep.data.fd = fd;
//    ep.events = EPOLLIN;
//    if(et_enable)
//    {
//        ep.events |= EPOLLET;
//    }
//    epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ep);
//    set_fd_nonblock(fd);
//}
//
///* 文件描述符在ET模式下的工作方式 */
//void et(int epfd, int lfd, epoll_event* ep_et, int epollwait_ret)
//{
//    char buf[BUFFSIZE];
//    struct sockaddr_in client_addr;
//    socklen_t client_addrlen = sizeof(client_addr);
//    for(int i = 0; i < epollwait_ret; ++i)
//    {
//        int sockfd = ep_et[i].data.fd;
//        /* 如果文件描述符等于监听描述符lfd, 则是客户端connect请求*/
//        if(sockfd == lfd)
//        {
//            int connfd = accept(lfd, (struct sockaddr*)&client_addr, &client_addrlen);
//            if(connfd <= 0)
//            {
//                printf("errno = %d\n", errno);
//                close(lfd);
//                return;
//            }
//            else{
//                addfd(connfd, epfd, true);
//            }
//        }
//        /* 如果是连接文件描述符connfd, 则是内核缓冲区可读 */
//        else if(ep_et->events & EPOLLIN)
//        {
//            printf("event triger once!\n");
//            while(1)
//            {
//                memset(buf, '\0', BUFFSIZE);
//                int ret = recv(ep_et[i].data.fd, buf, BUFFSIZE - 1, 0);
//                if(ret < 0)
//                {
//                    /* 在非阻塞I/0模式下, 错误码EAGAIN或EWOULDBLOCK表示缓冲区中无数据可读(即数据全部读取完成，此后epoll就能再次触发EPOLLIN事件以驱动下一次读操作)，请重新再试或变为阻塞模式 */
//                    if(errno == EAGAIN || errno == EWOULDBLOCK)
//                    {
//                        printf("read later\n");
//                    }
//                    else{
//                        close(sockfd);
//                    }
//                    break;
//                }
//                else if(ret == 0)
//                {
//                    printf("client has closed the connection!\n");
//                    close(sockfd);
//                }
//                else{
//                    printf("get %d bytes of content: %s\n", ret, buf);
//                }
//            }
//        }
//        else{
//            printf("Something else happen !\n");
//        }
//    }
//
//}
//
///* 文件描述符在LT模式下的工作模式 */
//void lt(int epfd, int lfd, epoll_event* ep_et, int epollwait_ret)
//{
//    char buf[BUFFSIZE];
//    struct sockaddr_in client_addr;
//    socklen_t client_addrlen = sizeof(client_addr);
//    for(int i = 0; i < epollwait_ret; ++i)
//    {
//        int sockfd = ep_et[i].data.fd;
//        /* 如果文件描述符等于监听描述符lfd, 则是客户端connect请求*/
//        if(sockfd == lfd)
//        {
//            int connfd = accept(lfd, (struct sockaddr*)&client_addr, &client_addrlen);
//            if(connfd <= 0)
//            {
//                printf("errno = %d\n", errno);
//                close(lfd);
//                return;
//            }
//            else{
//                addfd(connfd, epfd, false);
//            }
//        }
//            /* 如果是连接文件描述符connfd, 则是内核缓冲区可读 */
//        else if(ep_et->events & EPOLLIN)
//        {
//            printf("event triger once!\n");
//            memset(buf, '\0', BUFFSIZE);
//            int ret = recv(ep_et[i].data.fd, buf, BUFFSIZE - 1, 0);
//            if(ret <= 0)
//            {
//                close(sockfd);
//                continue;
//            }
//            printf("get %d bytes of content: %s\n", ret, buf);
//        }
//        else{
//            printf("Something else happen !\n");
//        }
//    }
//
//}
//
//int main(int argc, char* argv[])
//{
//    if(argc <= 2)
//    {
//        printf("usage: %s ip_address port_num\n", basename(argv[0]));
//        return 1;
//    }
//
//    const char* ip = argv[1];
//    int port = atoi(argv[2]);
//    struct sockaddr_in server_addr;
//    server_addr.sin_family = AF_INET;
//    server_addr.sin_port = htons(port);
//    inet_pton(AF_INET, ip, &server_addr.sin_addr);
//
//    int lfd = socket(AF_INET, SOCK_STREAM, 0);
//    assert(lfd >= 0);
//
//    int ret = bind(lfd, (struct sockaddr*)&server_addr, sizeof(server_addr));
//    assert(lfd != -1);
//
//    ret = listen(lfd, 5);
//    assert(lfd != -1);
//
//    /* 创建epoll内核事件表 */
//    int epfd = epoll_create(5);
//    assert(epfd != -1);
//    addfd(epfd, lfd, true);
//    epoll_event ep_et[MAX_EVENT_NUM];
//
//    while(1)
//    {
//        printf("i am here\n");
//        ret = epoll_wait(epfd, ep_et, MAX_EVENT_NUM, -1);
//        if(ret < 0)
//        {
//            printf("epoll failure !\n");
//            break;
//        }
//        else{
//            et(epfd, lfd, ep_et, ret);
//            //lt(epfd, lfd, ep_et, ret);
//        }
//    }
//}
