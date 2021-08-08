//
// Created by 123456 on 2021/8/8.
// 程序功能 : 使用两个(主从)状态机来处理逻辑，实现对客户端http请求的读取和分析
/* 一个简单的http请求实例:
 *    GET http://www.baidu.com/index.html HTTP/1.0
 *    User-Agent:Wget/1.12 (linux-gun)
 *    Host: www.baidu.com
 *    Connection: close
 */


#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>

/* 应用程序读缓冲区大小 */
#define BUFFER_SIZE 4096

enum CHECK_STATE {
    CHECK_STATE_REQUESTLINE = 0,  /* 正在分析请求行 */
    CHECK_STATE_HEADER            /* 正在分析头部字段 */
};

//从状态机的三种可能状态(即行的读取状态)
enum LINE_STATE {
    LINE_OK = 0,    /* 读取到一个完整的行 */
    LINE_BAD,       /* 行出错 */
    LINE_OPEN       /* 行数据尚不完整 */
};

//服务器处理http请求的结果
enum HTTP_CODE {
    NO_REQUEST,     /* 请求不完整，需要继续读取客户端数据 */
    GET_REQUEST,    /* 获取到一个完整的客户端请求 */
    BAD_REQUEST,    /* 客户请求有语法错误 */
    FORBIDDEN_REQUEST,    /* 客户端对资源没有访问权限 */
    INTERNAL_ERROR,       /* 服务器内部错误 */
    CLOSED_CONNECTION     /* 客户端已关闭连接 */
};

static const char* to_client_info[] = {"I get a correct result !\n", "Something Wrong !\n"};

/* prase_line
 * 函数功能 : 从状态机，用于解析出一行内容
 * 参数buf : 用户(应用程序)读缓冲区
 * 参数check_index : 服务器已经解析的用户数据字节数, 指向将要分析的缓冲区中的字节
 * 参数read_index : 当前已经读取的用户数据字节数, 指向已经读取的缓冲区中最后一个字节的下一个字节
 * 返回值 : 表示当前行的读取状态
 */
LINE_STATE prase_line(char* buf, int& check_index, int& read_index)
{
    char temp = '0';   /* 用于存放当前分析的字节 */
    for(;check_index < read_index; ++check_index)
    {
        temp = buf[check_index];
        if(temp == '\r')
        {
            if(check_index + 1 == read_index)
            {
                return LINE_OPEN;
            }
            else if(buf[check_index + 1] == '\n')
            {
                buf[check_index++] = '\0';
                buf[check_index++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
        if(temp == '\n')
        {
            if(check_index > 1 && buf[check_index - 1] == '\r')
            {
                buf[check_index - 1] = '\0';
                buf[check_index++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    return LINE_OPEN;
}

/* 解析请求行 */
HTTP_CODE prase_requestline(char* pos, CHECK_STATE& state)
{
    /* 如果请求行中没有' '或'\t', 则这个请求行一定是错误的 */
    char* url = strpbrk(pos, " \t");
    if(!url)
    {
        return BAD_REQUEST;
    }
    *url++ = '\0';

    /* 仅支持GET方法 */
    if(strcasecmp(pos, "GET") == 0)
    {
        printf("The http method is GET !\n");
    }
    else{
        return BAD_REQUEST;
    }

    int step = strspn(url, " \t");
    url += step;
    char* version = strpbrk(url, " \t");
    if(!version)
    {
        return BAD_REQUEST;
    }
    *version++ = '\0';
    version += strspn(version, " \t");
    if(strcasecmp(version, "HTTP/1.0") != 0)
    {
        return BAD_REQUEST;
    }

    if(strncasecmp(url, "http://", 7) == 0)
    {
        url += 7;
        url = strchr(url, '/');
    }

    if(!url || url[0] != '/')
    {
        return BAD_REQUEST;
    }

    printf("url is %s\n", url);
    state = CHECK_STATE_HEADER;
    return NO_REQUEST;
}

/* 处理头部字段 */
HTTP_CODE prase_header(char* pos)
{
    if(pos[0] = '\0')
    {
        return GET_REQUEST;
    }
    else if(strncasecmp(pos, "Host:", 5) == 0)
    {
        pos += 5;
        pos += strspn(pos, " \t");
        printf("Host is %s\n", pos);
    }
    else{
        printf("I can not handle this header !\n");
    }
    return NO_REQUEST;
}

HTTP_CODE prase_content(char* buf, int& check_index, int& read_index, CHECK_STATE& state, int& start_line)
{
    LINE_STATE line_state = LINE_OK;      /* 记录当前行的读取状态 */
    HTTP_CODE rescode = NO_REQUEST;           /* 记录http请求的处理结果 */
    while((line_state = prase_line(buf, check_index, read_index)) == LINE_OK)
    {
        /* 读取到一个完整http请求行, 开始解析 */
        char* temp = buf + start_line;
        start_line = read_index;    //此时已读的索引就是下一行的起始地址
        /* 主状态机 */
        switch (state) {
            case CHECK_STATE_REQUESTLINE:
                rescode = prase_requestline(temp, state);
                if(rescode == BAD_REQUEST)
                {
                    return BAD_REQUEST;
                }
                break;
            case CHECK_STATE_HEADER:
                rescode = prase_header(temp);
                if(rescode == BAD_REQUEST)
                {
                    return BAD_REQUEST;
                }
                else if(rescode == GET_REQUEST)
                {
                    return GET_REQUEST;
                }
                break;
            default:
            {
                return INTERNAL_ERROR;
            }
        }
    }
    /* 若没有读取一个完整的行，则还需要继续读取客户端数据 */
    if(line_state == LINE_OPEN)
    {
        return NO_REQUEST;
    }
    else{
        return BAD_REQUEST;
    }
}


int main(int argc, char* argv[])
{
    if(argc <= 2)
    {
        printf("usage : %s ip_adress port_number\n", basename(argv[0]));
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

    struct sockaddr_in client_addr;
    socklen_t sock_size = sizeof(client_addr);
    int connfd = accept(lfd, (struct sockaddr*)&client_addr, &sock_size);
    if(connfd == -1)
    {
        printf("accept errno = %d\n", errno);
        return 1;
    }
    else{
        char recv_buf[BUFFER_SIZE];
        memset(recv_buf, '\0', BUFFER_SIZE);

        int data_read = 0;       /* 每次recv调用后读取的字节数 */
        int read_index = 0;      /* 服务器已从缓冲区内读了多少字节的客户数据 */
        int check_index = 0;     /* 当前已经分析了多少字节的客户数据 */
        int start_line = 0;      /* 当前分析的行在buffer中的起始位置 */
        CHECK_STATE check_state = CHECK_STATE_REQUESTLINE;

        while(1)
        {
            data_read = recv(connfd, recv_buf + read_index, BUFFER_SIZE - read_index, 0);
            if(data_read == -1)
            {
                printf("recv errno = %d\n", errno);
                break;
            }
            else if(data_read == 0)
            {
                printf("remote client has closed the connection !\n");
                break;
            }
            read_index += data_read;
            HTTP_CODE result = prase_content(recv_buf, check_index, read_index, check_state, start_line);
            if(result == NO_REQUEST)     /* 尚未得到一个完整的http请求 */
            {
                continue;
            }
            else if(result == GET_REQUEST)
            {
                send(connfd, to_client_info[0], sizeof(*to_client_info[0]), 0);
                break;
            }
            else{
                send(connfd, to_client_info[1], strlen(to_client_info[1]), 0);
                break;
            }
        }
    }
    return 0;
}