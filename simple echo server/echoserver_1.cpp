#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

int main(int argc, char* argv[]) {
    //解析命令行参数：程序需要接收两个参数，即服务器的IP地址和端口号。如果参数数量不足，则打印用法提示并退出。
    if (argc <= 2) {
        printf("Usage: %s ip_address portname\n", argv[0]);
        return 0;
    }

    const char* ip = argv[1];
    int port = atoi(argv[2]);

    //创建监听套接字：使用socket函数创建一个TCP套接字，返回的文件描述符存储在listenfd变量中。如果创建失败，则程序终止。
    int listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(listenfd >= 1);

    //设置服务器地址：使用struct sockaddr_in结构体来设置服务器的地址信息。
    //将IP地址和端口号转换为网络字节序，并将其存储在address结构体中。
    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    inet_pton(AF_INET, ip, &address.sin_addr);

    //绑定套接字：使用bind函数将监听套接字绑定到指定的地址上。如果绑定失败，则程序终止。
    int ret = 0;
    ret = bind(listenfd, (struct sockaddr*)(&address), sizeof(address));
    assert(ret != -1);

    //监听套接字：使用listen函数开始监听连接请求。设置监听队列长度为5。如果监听失败，则程序终止。
    ret = listen(listenfd, 5);
    assert(ret != -1);

    //接受连接：使用accept函数接受客户端的连接请求，并返回一个新的套接字文件描述符sockfd。如果接受失败，则程序终止。
    struct sockaddr_in client;
    socklen_t client_addrlength = sizeof(client);
    int sockfd = accept(listenfd, (struct sockaddr*)(&address), &client_addrlength);

    //接收数据：使用recv函数从客户端接收数据，并存储在buf_size缓冲区中。接收到的数据大小存储在recv_size变量中。
    char buf_size[1024] = {0};
    int recv_size = 0;
    recv_size = recv(sockfd, buf_size, sizeof(buf_size), 0);

    //发送数据：使用send函数将接收到的数据发送回客户端。发送成功后，发送的数据大小存储在send_size变量中。
    int send_size = 0;
    send_size = send(sockfd, buf_size, recv_size, 0);

    //闭套接字：使用close函数关闭客户端套接字和监听套接字。
    close(sockfd);
    close(listenfd);

    return 0;
}