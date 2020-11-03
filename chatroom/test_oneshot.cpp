//
// Created by Roy on 10/29/20.
//

#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <stdlib.h>
#include <sys/epoll.h>
#include <pthread.h>
#include <unistd.h>
#include <assert.h>
#include <sys/socket.h>

#define MAX_EVENT_NUMBER 1024
#define BUFFER_SIZE 1024

using namespace std;

struct fds {
    int epollfd;
    int sockfd;
};

int set_non_blocking(int fd){
    //fcntl manipulate file descriptor
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

//将事件注册到由epollfd管理的注册表中
void addfd(int epollfd, int fd, bool oneshot){
    //先有事件 -> 由事件绑定fd
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET;
    if (oneshot){
        event.events |= EPOLLONESHOT;
    }
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    set_non_blocking(fd);
}

//要把oneshot 事件重新置位， 在上一次响应后就会消失？
void reset_oneshot(int epollfd, int fd){
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOOLET | EPOLLONESHOT;
    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}

void *worker(void* arg){
    int sockfd = ((fds*)arg)->sockfd;

    int epollfd = ((fds*)arg)->epollfd;

    cout << "start new thread to receive data on fd: " << sockfd << endl;

    char buf[BUFFER_SIZE];
    memset(buf, 0, BUFFER_SIZE);

    while (1){
        int ret = recv(sockfd, buf, BUFFER_SIZE-1, 0);
        if (ret == 0){
            close(sockfd);
            cout << "foreiner closed the connection\n";
            break;
        } else if (ret < 0){
            //EAGAIN -> try again
            //当前在非阻塞阶段,因此若出现recv无数据可读时，程序不会阻塞而是返回一个EAGAIN
            //指稍后继续
            if (errno == EAGAIN){
                reset_oneshot(epollfd, sockfd);
                cout << "read later\n";
                break;
            }
        } else {
            cout << "get content:  " << buf << endl;
            sleep(5);
        }
    }
    cout << "end thread receiving data on fd: " << sockfd << endl;
}

int main(int argc, char* argv[]){
    if (argc <= 2){
        cout << "usage incorrect\n";
        return 1;
    }

    char* ip = argv[1];
    int port = atoi(argv[2]);

    int ret = 0;
    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &address.sin_addr);
    address.sin_port = htons(port);

    int listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert (listenfd >= 0);
    bind(listenfd, (struct sockaddr*)&address, sizeof(address));
    listen(listenfd, 5);

    //创建events数组
    epoll_event events[MAX_EVENT_NUMBER];
    int epollfd = epoll_create(5);

    //监听的socket listenfd 是不能注册为oneshot事件, 否则应用程序只能处理一个客户连接
    addfd(epollfd, listenfd, false);

    while (1){

        //触发的事件个数
        //将触发的事件从队列拷贝到用户空间的events 数组中
        int res = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
        if (res < 0){
            cout << "epoll failure\n";
            break;
        }

        for (int i=0; i < res; i++){
            int sockfd = events[i].data.fd;

            if (sockfd == listenfd){
                struct sockaddr_in client;
                bzero(&client, sizeof(client));
                socklen_t client_addr_len = sizeof(client);
                int connfd = accept(listenfd, (struct sockaddr*)&client, &client_addr_len);
                addfd(epollfd, connfd, true);
            } else if (events[i].events & EPOLLIN) {
                pthread_t thread;
                fds fds_for_new_worker;
                fds_for_new_worker.epollfd = epollfd;
                fds_for_new_worker.sockfd = sockfd;

                pthread_create(&thread, NULL, worker, (void*)&fds_for_new_worker);
            } else {
                cout << "something else happend\n";
            }
        }
    }

    close(listenfd);
    return 0;
}