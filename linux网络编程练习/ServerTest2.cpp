//
// Created by 123456 on 2021/8/5.
// 函数功能 : 使用splice(0拷贝)和tee(0拷贝)函数实现0拷贝同时输出数据到终端和文件
// 注意事项 : tee操作的两个文件描述符必须都是管道文件描述符
//

//#include <assert.h>
//#include <stdio.h>
//#include <unistd.h>
//#include <errno.h>
//#include <string.h>
//#include <fcntl.h>
//
//
//int main(int argc, char* argv[])
//{
//    if(argc != 2)
//    {
//        printf("usage: %s <file>\n", argv[0]);
//        return 1;
//    }
//
//    int filefd = open(argv[1], O_CREAT | O_WRONLY | O_TRUNC, 0666);
//    assert(filefd > 0);
//
//    int pipefd_file[2];
//    int ret = pipe(pipefd_file);
//    assert(ret != -1);
//
//    int pipefd_stdout[2];
//    ret = pipe(pipefd_stdout);
//    assert(ret != -1);
//
//    /* 将标准输入内容输入管道pipefd_stdout */
//    ret = splice(STDIN_FILENO, nullptr, pipefd_stdout[1], nullptr, 32768, SPLICE_F_MOVE | SPLICE_F_MORE);
//    assert(ret != -1);
//
//    /* 使用tee函数将pipefd_stdout管道内容复制到pipefd_stdout */
//    ret = tee(pipefd_stdout[0], pipefd_file[1], 32768, SPLICE_F_NONBLOCK);
//    assert(ret != -1);
//
//    /* 分别将管道pipefd_stdout和pipefd_stdout的输出重定向到标准输出和文件描述符filefd中 */
//    ret = splice(pipefd_stdout[0], nullptr, STDOUT_FILENO, nullptr, 32768, SPLICE_F_MORE | SPLICE_F_MOVE);
//    assert(ret != -1);
//
//    ret = splice(pipefd_file[0], nullptr, filefd, nullptr, 32768, SPLICE_F_MOVE | SPLICE_F_MORE);
//    assert(ret != -1);
//
//    close(filefd);
//    close(pipefd_file[0]);
//    close(pipefd_file[1]);
//    close(pipefd_stdout[0]);
//    close(pipefd_stdout[1]);
//    return 0;
//}
