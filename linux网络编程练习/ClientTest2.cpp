//
// Created by 123456 on 2021/8/12.
// 程序介绍 : 发送带外数据和普通数据测试ServerTest6的功能是否正确
//

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>

int main(int argc, char* argv[0])
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
    inet_pton(AF_INET, ip , &server_addr.sin_addr);

    int connfd = socket(AF_INET, SOCK_STREAM, 0);
    assert(connfd >= 0);

    int ret = connect(connfd, (struct sockaddr*)&server_addr, sizeof(server_addr));
    if(ret < 0)
    {
        printf("connect failed!\n");
    }
    else{
        const char* oob_data = "abc";
        const char* normal_data = "123";
        send(connfd, normal_data, strlen(normal_data), 0);
        sleep(5);
        send(connfd, oob_data, strlen(oob_data), MSG_OOB);
        send(connfd, normal_data, strlen(normal_data), 0);
        sleep(3);
    }
    close(connfd);
    return 0;
}
