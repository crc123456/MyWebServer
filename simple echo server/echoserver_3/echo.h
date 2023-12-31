#ifndef __ECHO_H_
#define __ECHO_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <arpa/inet.h>

#include "processpoll.h"

class echo
{
private:
    static const int BUFFER_SIZE  = 1024;
    static int epollfd; //静态成员变量，用于保存 epoll 实例的文件描述符
    int sockfd; //连接的套接字文件描述符
    sockaddr_in client_addr; //连接的客户端地址信息
    char buf[1024] = {0}; //用于接收和发送数据的缓冲区

public:
    echo() {}
    ~echo() {}

    void init(int _epollfd, int _sockfd, const sockaddr_in &address) {
        epollfd = _epollfd;
        sockfd = _sockfd;
        client_addr = address;
    }

    void process() {
        while(1) {
            memset(buf, 0, sizeof(buf));
            int ret = recv(sockfd, buf, sizeof(buf), 0);
            if (ret < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    break;
                }
            }
            else if (ret == 0) {
                removefd(epollfd, sockfd);
                break;
            }
            else {
                send(sockfd, buf, sizeof(buf), 0);
            }
        }
        return;
    }
};

int echo :: epollfd = -1;

#endif