//
// Created by 123456 on 2021/8/9.
// 程序功能 : 使用select系统调用实现同时接受普通用户数据和带外(紧急，OOB)数据。
// 注意 : socket上接收到普通数据和带外数据都会使select返回，但socket处于不同的就绪状态，前者处于可读状态，后者处于异常状态。
//

//#include <sys/types.h>
//#include <sys/socket.h>
//#include <arpa/inet.h>
//#include <netinet/in.h>
//#include <stdio.h>
//#include <unistd.h>
//#include <stdlib.h>
//#include <assert.h>
//#include <sys/select.h>
//#include <errno.h>
//#include <string.h>
//#include <fcntl.h>
//
//#define BUFFSIZE 1024
//
//int main(int argc, char* argv[])
//{
//    if(argc <= 2)
//    {
//        printf("usage: %s ip_address port_number\n", basename(argv[0]));
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
//    if(lfd < 0)
//    {
//        printf("errno = %d\n", errno);
//        return 1;
//    }
//
//    int ret = bind(lfd, (struct sockaddr*)&server_addr, sizeof(server_addr));
//    assert(ret != -1);
//
//    ret = listen(lfd, 5);
//    assert(ret != -1);
//
//    struct sockaddr_in client_addr;
//    socklen_t client_addrlen = sizeof(client_addr);
//    int connfd = accept(lfd, (struct sockaddr*)&client_addr, &client_addrlen);
//    if(connfd < 0)
//    {
//        printf("errno = %d\n", errno);
//        close(lfd);
//        return 1;
//    }
//
//    char buf[BUFFSIZE];
//    fd_set read_fd;
//    fd_set exception_fd;
//    FD_ZERO(&read_fd);
//    FD_ZERO(&exception_fd);
//
//    while(1)
//    {
//        memset(buf, '\0', BUFFSIZE);
//        /* 注意fd_set每次都要重新绑定文件描述符, 因为每次select调用后如果有事件发生内核都会修改fd_set */
//        FD_SET(connfd, &read_fd);
//        FD_SET(connfd, &exception_fd);
//
//        /* 进行select调用 */
//        timeval t = {0, 0};
//        ret = select(connfd + 1, &read_fd, nullptr, &exception_fd, &t);
//        if(ret < 0)
//        {
//            printf("errno = %d\n", errno);
//            break;
//        }
//
//        /* 处理普通用户数据 */
//        if(FD_ISSET(connfd, &read_fd))
//        {
//            ret = recv(connfd, buf, BUFFSIZE - 1, 0);
//            if(ret <= 0)
//            {
//                /* 包含调用失败和对方主动关闭连接两种情况 */
//                break;
//            }
//            printf("recv %d byte nomal data %s.\n", ret, buf);
//        }
//
//        /* 用带MSG_OOB标志的recv接受带外数据 */
//        if(FD_ISSET(connfd, &exception_fd))
//        {
//            ret = recv(connfd, buf, BUFFSIZE - 1, MSG_OOB);
//            if(ret <= 0)
//            {
//                /* 包含调用失败和对方主动关闭连接两种情况 */
//                break;
//            }
//            printf("recv %d byte OOB data %s.\n", ret, buf);
//        }
//    }
//    close(connfd);
//    close(lfd);
//    return 0;
//}
