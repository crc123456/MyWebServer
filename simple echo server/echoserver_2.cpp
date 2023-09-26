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
#include <iostream>

using namespace std;

#define MAX_EVENTS_NUMBER 5

//这段代码实现了将文件描述符设置为非阻塞模式的功能
//1. `fcntl`函数：该函数可以对文件描述符进行各种控制操作，包括获取和设置文件描述符的状态。第一个参数`fd`指定要操作的文件描述符，第二个参数`F_GETFL`表示获取文件描述符的状态标志，返回值为当前文件描述符的状态标志。
//2. `O_NONBLOCK`：该宏定义在`fcntl.h`头文件中，表示将文件描述符设置为非阻塞模式。
//3. `F_SETFL`：该参数表示设置文件描述符的状态标志。第二个参数`new_state`表示要设置的状态标志，包括`O_NONBLOCK`等。第三个参数`fd`表示要操作的文件描述符。
//4. 返回值：该函数返回原来文件描述符的状态标志，即`old_state`，方便后续需要时恢复文件描述符的状态。
//因此，这段代码的作用是将文件描述符设置为非阻塞模式，并返回原来的状态标志。在非阻塞模式下，当读写操作无法立即完成时，函数将立即返回，而不是一直等待操作完成。这种方式可以提高程序的响应速度和吞吐量，特别是在高并发的网络编程中，非常有用。
int set_non_blocking(int fd) {
    int old_state = fcntl(fd, F_GETFL);
    int new_state = old_state | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_state);
    return old_state;
}

//这段代码实现了将文件描述符添加到epoll事件监听中的功能，具体解析如下：
//1. `epoll_event`结构体：该结构体用于描述事件的类型和相关的数据。其中，`events`字段表示事件类型，可以是`EPOLLIN`（可读事件）、`EPOLLOUT`（可写事件）等；`data`字段用于存储与事件相关的数据，这里将文件描述符`fd`存储在`data.fd`中。
//2. `epoll_ctl`函数：该函数用于控制epoll事件监听器，可以添加、修改或删除事件。第一个参数`epollfd`是epoll事件监听器的文件描述符，第二个参数`EPOLL_CTL_ADD`表示添加事件，第三个参数`fd`表示要添加的文件描述符，第四个参数`event`表示要添加的事件。
//3. `set_non_blocking`函数：该函数用于将文件描述符设置为非阻塞模式。具体实现和之前解析的函数相同，将文件描述符设置为非阻塞模式。
//4. 综合作用：这段代码的作用是将文件描述符添加到epoll事件监听器中，并设置监听的事件类型为可读事件（`EPOLLIN`）。同时，将文件描述符设置为非阻塞模式。通过这样的操作，可以实现对文件描述符的异步监听，当有可读事件发生时，程序可以立即响应并进行相应的处理。在高并发的网络编程中，使用epoll来实现异步IO操作可以提高程序的性能和响应速度。
void addfd(int epollfd, int fd) {
    epoll_event event;
    event.events = EPOLLIN | EPOLLET;
    event.data.fd = fd;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    set_non_blocking(fd);
}

int main(int argc, char* argv[]) {
    if (argc <= 2) {
        printf("Usage: %s ip_address portname\n", argv[0]);
        return 0;
    }

    const char* ip = argv[1];
    int port = atoi(argv[2]); //atoi函数用于将字符串转换为整数
    // cout << ip << " " << port << endl;

    //这段代码用于创建一个套接字，并进行错误检查，具体解释如下：
    //1. `socket`函数：该函数用于创建一个套接字。第一个参数`PF_INET`表示使用IPv4协议，第二个参数`SOCK_STREAM`表示使用TCP协议，第三个参数`0`表示默认的协议（通常为0）。
    //2. `listenfd`变量：这里将`socket`函数的返回值赋值给`listenfd`变量，即获取创建的套接字的文件描述符。
    //3. `assert`宏：该宏用于进行错误检查，如果参数表达式的值为假（即0），则终止程序执行，并打印错误信息。在这里，如果`listenfd`的值小于1（即创建套接字失败），则程序会终止执行。
    //综合作用：这段代码的作用是创建一个TCP套接字，并将其文件描述符存储在`listenfd`变量中。同时，通过`assert`宏对套接字创建是否成功进行了错误检查，以确保程序的正常执行。
    int listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(listenfd >= 1);

    //这段代码用于设置服务器的地址信息，具体解释如下：
    //1. `struct sockaddr_in address;`：这行代码定义了一个名为`address`的`sockaddr_in`结构体变量。`sockaddr_in`结构体用于表示IPv4地址和端口号。
    //2. `memset(&address, 0, sizeof(address));`：这行代码使用`memset`函数将`address`结构体变量的内存空间清零。`memset`函数用于将指定的内存区域设置为特定的值，第一个参数为指向内存区域的指针，第二个参数为要设置的值，第三个参数为要设置的内存区域的大小。
    //3. `address.sin_family = AF_INET;`：这行代码将`address`结构体变量的`sin_family`成员设置为`AF_INET`，表示使用IPv4地址。
    //4. `address.sin_port = htons(port);`：这行代码将`address`结构体变量的`sin_port`成员设置为网络字节序的端口号。`htons`函数用于将主机字节序的端口号转换为网络字节序。
    //5. `inet_pton(AF_INET, ip, &address.sin_addr);`：这行代码将字符串形式的IP地址转换为二进制形式，并存储在`address`结构体变量的`sin_addr`成员中。`inet_pton`函数用于将点分十进制形式的IP地址转换为网络字节序的二进制形式。
    //综合作用：这段代码的作用是设置服务器的地址信息，包括IP地址和端口号。首先，通过`memset`函数将地址结构体的内存空间清零，然后分别设置地址结构体的成员，最后将字符串形式的IP地址转换为二进制形式，并存储在地址结构体中。这样，服务器就有了一个完整的地址信息，可以用于绑定监听套接字和接受客户端连接。
    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    inet_pton(AF_INET, ip, &address.sin_addr);

    //这段代码用于将套接字绑定到指定的地址和端口号，具体解释如下：
    //1. `bind`函数：该函数用于将套接字与指定的地址进行绑定。第一个参数`listenfd`是要绑定的套接字的文件描述符，第二个参数`(struct sockaddr*)(&address)`是指向地址结构体的指针，第三个参数`sizeof(address)`是地址结构体的大小。
    //2. `ret`变量：这里将`bind`函数的返回值赋值给`ret`变量，即绑定操作的结果。
    //3. `assert`宏：该宏用于进行错误检查，如果参数表达式的值为假（即0），则终止程序执行，并打印错误信息。在这里，如果`ret`的值为-1（即绑定失败），则程序会终止执行。
    //综合作用：这段代码的作用是将套接字`listenfd`与地址结构体`address`中的IP地址和端口号进行绑定。通过`assert`宏对绑定操作是否成功进行了错误检查，以确保程序的正常执行。
    int ret = 0;
    ret = bind(listenfd, (struct sockaddr*)(&address), sizeof(address));
    assert(ret != -1);

    //这段代码用于将套接字设置为监听状态，具体解释如下：
    //1. `listen`函数：该函数用于将套接字设置为监听状态，以接受客户端的连接请求。第一个参数`listenfd`是要设置为监听状态的套接字的文件描述符，第二个参数`5`表示队列中最多可以等待的连接数。
    //2. `ret`变量：这里将`listen`函数的返回值赋值给`ret`变量，即设置监听状态的结果。
    //3. `assert`宏：该宏用于进行错误检查，如果参数表达式的值为假（即0），则终止程序执行，并打印错误信息。在这里，如果`ret`的值为-1（即设置监听状态失败），则程序会终止执行。
    //综合作用：这段代码的作用是将套接字`listenfd`设置为监听状态，以接受客户端的连接请求。通过`assert`宏对设置监听状态操作是否成功进行了错误检查，以确保程序的正常执行。
    ret = listen(listenfd, 5);
    assert(ret != -1);

    //这段代码用于创建一个`epoll`实例，并将监听套接字`listenfd`添加到`epoll`的事件集合中，具体解释如下：
    //1. `epoll_event events[MAX_EVENTS_NUMBER];`：这行代码定义了一个数组`events`，用于存储`epoll`返回的就绪事件。
    //2. `int epollfd = epoll_create(5);`：这行代码调用`epoll_create`函数创建一个`epoll`实例，并将返回的文件描述符赋值给`epollfd`变量。第一个参数`5`表示监听队列的最大长度，即同时监听的文件描述符数量。
    //3. `assert(epollfd != -1);`：这行代码使用`assert`宏进行错误检查，如果`epoll_create`函数返回的文件描述符为-1（即创建`epoll`实例失败），则终止程序执行。
    //4. `addfd(epollfd, listenfd);`：这行代码调用`addfd`函数将监听套接字`listenfd`添加到`epoll`实例中。`addfd`函数的具体功能是将指定的文件描述符添加到`epoll`的事件集合中，以便后续对其进行事件监听。
    //综合作用：这段代码的作用是创建一个`epoll`实例，并将监听套接字`listenfd`添加到`epoll`的事件集合中，以便后续对其进行事件监听。通过`assert`宏对`epoll_create`函数的返回值进行了错误检查，以确保程序的正常执行。
    epoll_event events[MAX_EVENTS_NUMBER];
    int epollfd = epoll_create(5);
    assert(epollfd != -1);
    addfd(epollfd, listenfd);

    while(1) {
        //这段代码用于等待事件的发生，并返回就绪事件的数量。具体解释如下：
        //1. `epoll_wait`函数：该函数用于等待事件的发生，并返回就绪事件的数量。第一个参数`epollfd`是`epoll`实例的文件描述符，第二个参数`events`是用于存储就绪事件的数组，第三个参数`MAX_EVENTS_NUMBER`表示`events`数组的长度，第四个参数`-1`表示无限等待，即直到有就绪事件发生才返回。
        //2. `number`变量：将`epoll_wait`函数的返回值赋值给`number`变量，即就绪事件的数量。
        //综合作用：这段代码的作用是等待事件的发生，并返回就绪事件的数量。通过将`epoll_wait`函数的返回值赋值给`number`变量，可以获取到就绪事件的数量，并进行后续的处理。
        int number = epoll_wait(epollfd, events, MAX_EVENTS_NUMBER, -1);
        if (number < 0) {
            printf("epoll_wait failed\n");
            return -1;
        }

        for (int i = 0; i < number; i++) {
            const auto& event = events[i]; //使用const auto&是为了避免拷贝事件对象
            const auto eventfd = event.data.fd;

            //用于判断是否有新的客户端连接请求
            if (eventfd == listenfd) {
                //如果是新的连接请求，就调用accept函数接受连接，并将新的套接字添加到epoll实例中。
                //accept函数返回的套接字会被添加到epoll的事件集合中，以便后续监听其读事件。
                struct sockaddr_in client;
                socklen_t client_addrlength = sizeof(client);
                int sockfd = accept(listenfd, (struct sockaddr*)(&client), &client_addrlength);
                addfd(epollfd, sockfd);
            //用于判断是否有可读事件
            } else if (event.events & EPOLLIN) {
                char buf[1024] = {0};
                //如果是可读事件，就循环读取数据，并将数据发送回客户端。
                while (1) {
                    memset(buf, '\0', sizeof(buf));
                    int recv_size = recv(eventfd, buf, sizeof(buf), 0);
                    //使用recv函数接收数据，如果返回值小于0，并且错误码是EAGAIN或EWOULDBLOCK，表示数据已经全部读取完毕，跳出循环。
                    if (recv_size < 0) {
                        if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
                            break;
                        }
                        //如果返回值小于0并且不是EAGAIN或EWOULDBLOCK，表示接收数据出错，关闭套接字并退出循环。
                        printf(" sockfd %d,recv msg failed\n", eventfd);
                        close(eventfd);
                        break;
                    //如果返回值等于0，表示客户端关闭了连接，关闭套接字并退出循环。
                    } else if (recv_size == 0) {
                        close(eventfd);
                        break;
                    //如果返回值大于0，表示成功接收到数据，将数据发送回客户端。
                    } else {
                        send(eventfd, buf, recv_size, 0);
                    }
                }
            }
        }
    }
    close(listenfd);
    return 0;
}