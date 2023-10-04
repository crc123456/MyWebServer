#ifndef __PROCESSPOLL_H_
#define __PROCESSPOLL_H_

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

class process
{
public:
    int pid;
    int pipe[2];

    process() : pid(-1), pipe{0, 0} {}
};

template <typename T>
class processpool
{
private:
    static const int MAX_EVENTS_NUMBER = 5; //最大事件数量
    static const int MAX_USER_PRE_PROCESS = 10000; //最大预处理用户数量
    int idx; //进程池中的索引
    int listenfd; //监听的文件描述符
    int epollfd; //epoll文件描述符
    int max_processes_num; //最大进程数量
    process* sub_processes; //子进程数组，为一个指向process类对象的指针
    static processpool<T>* instance; //processpool类的实例，为一个指向processpool类对象的指针

    processpool(int listenfd, int max_processes_num = 8);
    ~processpool() {
        delete []sub_processes;
    }

public:
    //静态成员函数，用于创建processpool对象的实例。接受一个监听的文件描述符和最大进程数量作为参数。
    //如果instance为空，则创建一个新的processpool对象并返回，否则返回已存在的instance。
    static processpool<T>* create(int listenfd, int _max_processes_num = 8) {
        if (instance == nullptr) {
            instance = new processpool<T>(listenfd, _max_processes_num);
            return instance;
        }

        return instance;
    }

    void run(); //运行进程池
    void run_parent(); //运行父进程
    void run_child(); //运行子进程
    void setup_up_sig(); //设置信号处理函数
};

template <typename T>
processpool<T>* processpool<T> :: instance = nullptr;

template <typename T>
processpool<T>::processpool(int listenfd, int _max_processes_num) :
    idx(-1), listenfd(listenfd), epollfd(0), max_processes_num(_max_processes_num), sub_processes(nullptr)
{
    sub_processes = new process[max_processes_num];

    for (int i = 0; i < max_processes_num; i++) {
        //使用socketpair函数创建一个UNIX域套接字对，并将结果存储在sub_processes[i].pipe数组中。
        //然后，通过fork函数创建子进程，将返回值存储在sub_processes[i].pid中。
        socketpair(PF_UNIX, SOCK_STREAM, 0, sub_processes[i].pipe);
        sub_processes[i].pid = fork();

        //如果sub_processes[i].pid大于0，表示当前进程是父进程，则关闭sub_processes[i].pipe[1]，继续下一次循环。
        //如果sub_processes[i].pid等于0，表示当前进程是子进程，则关闭sub_processes[i].pipe[0]，将i赋值给idx，并跳出循环。
        //sub_processes[i].pipe[1]是父进程用来向子进程发送消息的文件描述符，sub_processes[i].pipe[0]是子进程用来接收父进程消息的文件描述符。
        //在父进程中，通过关闭sub_processes[i].pipe[1]，可以确保父进程不会向子进程发送消息，只能接收子进程发送的消息。
        //而在子进程中，通过关闭sub_processes[i].pipe[0]，可以确保子进程不会接收父进程的消息，只能向父进程发送消息。
        if (sub_processes[i].pid > 0) {
            close(sub_processes[i].pipe[1]);
            continue;
        }
        else
        {
            close(sub_processes[i].pipe[0]);
            idx = i;
            break;
        }
    }
}

//这段代码是一个函数 `set_non_blocking`，用于将文件描述符 `fd` 设置为非阻塞模式。
//函数首先调用 `fcntl` 函数，传入 `fd` 和 `F_GETFL` 参数，获取文件描述符 `fd` 的当前状态。`fcntl` 函数返回的是文件描述符的标志位，其中包含了文件描述符的一些属性和状态信息。
//然后，函数将获取到的旧状态 `old_state` 和 `O_NONBLOCK` 进行按位或操作，生成新的状态 `new_state`，将 `O_NONBLOCK` 标志位添加到文件描述符的状态中。
//最后，函数调用 `fcntl` 函数，传入 `fd`、`F_SETFL` 和 `new_state` 参数，将新的状态设置给文件描述符 `fd`。
//函数返回的是旧状态 `old_state`，可以用于记录之前的文件描述符状态，或者进行其他操作。
static int set_non_blocking(int fd) {
    int old_state = fcntl(fd, F_GETFL);
    int new_state = old_state | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_state);

    return old_state;
}

static void addfd(int epollfd, int fd) {
    //首先创建一个 epoll_event 结构体变量 event，并设置 event.events 成员为 EPOLLIN | EPOLLET，表示关注可读事件和边缘触发模式。
    epoll_event event;
    event.events = EPOLLIN | EPOLLET;
    //将文件描述符 fd 赋值给 event.data.fd，表示将该文件描述符与事件关联起来
    event.data.fd = fd;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    //将文件描述符 fd 设置为非阻塞模式
    set_non_blocking(fd);
}

static void removefd(int epollfd, int fd) {
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, nullptr);
    close(fd);
}

template <typename T>
void processpool<T> :: run()
{
    if (idx == -1) {
        run_parent();
    }
    else {
        run_child();
    }
}

//创建一个 epoll 实例 epollfd
template <typename T>
void processpool<T> :: setup_up_sig()
{
    epollfd = epoll_create(5);
    assert(epollfd != -1);
}

template <typename T>
void processpool<T> :: run_parent()
{
    //创建一个 epoll_event 数组 events，用于存储 epoll 实例 epollfd 中返回的事件
    epoll_event events[MAX_EVENTS_NUMBER];
    //调用 setup_up_sig() 函数，创建 epoll 实例 epollfd
    setup_up_sig();

    //调用 addfd(epollfd, listenfd) 函数，将监听套接字 listenfd 添加到 epoll 实例 epollfd 中，并设置为非阻塞模式
    addfd(epollfd, listenfd);

    int pre_idx = 0;
    int has_new_cli = 1;
    int number = 0;
    while(1) {
        //调用 epoll_wait 函数等待事件发生。epoll_wait 函数会阻塞，直到有事件发生或超时
        number = epoll_wait(epollfd, events, MAX_EVENTS_NUMBER, -1);

        //当有事件发生时，通过循环遍历 events 数组，处理每个事件。对于监听套接字 listenfd，表示有新的连接请求到达。在处理新的连接请求时，会选择一个子进程来处理。
        for (int i = 0; i < number; i++) {
            int sockfd = events[i].data.fd;
            if (sockfd == listenfd) {
                //通过 pre_idx 变量来记录上一个处理连接请求的子进程的索引。然后，通过循环找到下一个可用的子进程，将连接请求发送给该子进程
                int pos = pre_idx;
                do
                {
                    pos = (pos + 1) % max_processes_num;
                } while (sub_processes[pos].pid == -1);
                pre_idx = pos;

                send(sub_processes[pos].pipe[0], (void*)&has_new_cli, sizeof(has_new_cli), 0);
                printf("parent processes has sent msg to %d child\n", pos);
                
            }
        }
    }
}

template <typename T>
void processpool<T> :: run_child()
{
    epoll_event events[MAX_EVENTS_NUMBER];
    setup_up_sig();

    //获取子进程的管道写入端文件描述符 pipefd
    int pipefd = sub_processes[idx].pipe[1];
    //调用 addfd(epollfd, pipefd) 函数，将管道写入端文件描述符 pipefd 添加到 epoll 实例 epollfd 中，并设置为非阻塞模式
    addfd(epollfd, pipefd);
    //创建一个 T 类型的数组 users，用于存储每个连接的处理对象
    T* users = new T[MAX_USER_PRE_PROCESS];

    int number = 0;
    while(1) {
        number = epoll_wait(epollfd, events, MAX_EVENTS_NUMBER, -1);
        for (int i = 0; i < number; i++) {
            int sockfd = events[i].data.fd;
            if (sockfd == pipefd && (events[i].events & EPOLLIN)) {
                struct sockaddr_in client;
                //获取客户端地址结构体 client 的大小，用于 accept() 函数的参数
                socklen_t client_addrlength = sizeof(client);
                //接受新的连接请求，并返回连接的文件描述符 connfd
                int connfd = accept(listenfd, (struct sockaddr*)(&client), &client_addrlength);
                addfd(epollfd, connfd);
                users[connfd].init(epollfd, connfd, client);
                printf("child %d is addfding \n", idx);
                continue;
            }
            else if (events[i].events & EPOLLIN) {
                printf("child %d has recv msg\n", idx);
                //调用连接处理对象 users[sockfd] 的 process() 函数，处理接收到的数据
                users[sockfd].process();
            }
        }
    }

    delete []users;
    users = nullptr;
    close(epollfd);
    close(pipefd);
}

#endif