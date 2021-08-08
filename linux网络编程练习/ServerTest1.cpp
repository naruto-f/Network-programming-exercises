//
// Created by 123456 on 2021/8/5.
// 函数功能 : 使用splice函数(0拷贝)实现的回射服务器，即它将客户端发送的数据原样返回给客户端
// 注意事项 : splice函数操作的两个文件描述符中必须至少有一个是管道文件描述符
//

//#include <sys/socket.h>
//#include <netinet/in.h>
//#include <arpa/inet.h>
//#include <assert.h>
//#include <stdio.h>
//#include <unistd.h>
//#include <stdlib.h>
//#include <errno.h>
//#include <string.h>
//#include <fcntl.h>
//
//
//int main(int argc, char* argv[])
//{
//    if(argc <= 2)
//    {
//        printf("usage : %s ip_address port_number\n", basename(argv[0]));
//        return 1;
//    }
//
//    const char *ip = argv[1];
//    const int port = atoi(argv[2]);
//
//    struct sockaddr_in server_addr;
//    bzero(&server_addr, sizeof(server_addr));
//    server_addr.sin_family = AF_INET;
//    server_addr.sin_port = htons(ip);
//    inet_pton(AF_INET, ip, &server_addr.sin_addr);
//
//    int lfd = socket(AF_INET, SOCK_STREAM, 0);
//    assert(lfd >= 0);
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
//    if(connfd == -1)
//    {
//        printf("errorno = %d\n", errno);
//    }
//    else{
//        int client_pipe[2];
//        ret = pipe(client_pipe);
//        assert(ret != -1);
//
//        /* 将客户端建立连接的文件描述符中收到的数据重定向到管道client_pipe中 */
//        ret = splice(connfd, nullptr, client_pipe[1], nullptr, 32768, SPLICE_F_MORE | SPLICE_F_MOVE);
//        assert(ret != -1);
//
//        /* 将管道的输出重定向到connfd客户连接文件描述符 */
//        ret = splice(client_pipe[0], nullptr, connfd, nullptr, 32768, SPLICE_F_MORE | SPLICE_F_MOVE);
//        assert(ret != -1);
//
//        close(connfd);
//    }
//    close(lfd);
//    return 0;
//}
