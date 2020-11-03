//
// Created by Roy on 10/28/20.
//

#include <iostream>
#include <sys/fcntl.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>

using namespace std;

#define MAX_EVENT_NUMBER 1024
#define BUFFER_SIZE 10

int set_no_blocking(int fd){
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, new_option);
    return old_option;
}

void addfd(int epollfd, int fd, bool enable_et){
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN;
    if (enable_et){
        event.events |= EPOLLET;
    }
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    set_no_blocking(fd);
}

int lt(epoll_event* events, int number, int epollfd, int listenfd){
    char buf[BUFFER_SIZE];
    for (int i = 0; i < number; i++){
        int sockfd = events[i].data.fd;
        //有连接事件
        if (sockfd == listenfd){
            struct sockaddr_in client_address;
            socklen_t client_addr_len = sizeof(client_addr_len);
            int connfd = accept(listenfd, (struct sockaddr*)&client_address, &client_addr_len);
            addfd(epollfd, connfd, false); //把连接的fd注册到epoll表中, 对事件监控启用lt模式
        } else if (events[i].events & EPOLLIN){ //有读事件
            cout << "event trigger once" << endl;
            memset(buf, 0, BUFFER_SIZE);
            int ret = recv(sockfd, buf, BUFFER_SIZE-1, 0);
            if (ret < 0){
                close(sockfd);
                continue;
            }
            cout << "get " << ret << " bytes of content: " << buf << endl;
        } else {
            cout << "something happened\n";
        }
    }
}

int et(epoll_event* events, int number, int epollfd, int listenfd){
    char buf[BUFFER_SIZE];
    for (int i=0; i < number; i++){
        int sockfd = events[i].data.fd;
        if (sockfd == listenfd){
            struct sockaddr_in client_address;
            socklen_t client_addr_len = sizeof(client_address);
            int connfd = accept(listenfd, (struct sockaddr*)&client_address, &client_addr_len);
            addfd(epollfd, connfd, true);
        } else  if (events[i].events & EPOLLIN){
            cout << "event trigger once" << endl;
            while (1){
                memset(buf, 0, BUFFER_SIZE);
                int ret = recv(sockfd, buf, BUFFER_SIZE-1, 0);
                if (ret < 0){
                    if ((errno == EAGAIN) || (errno == EWOULDBLOCK)){
                        cout << "read later\n";
                        break;
                    }
                    close(sockfd);
                    break;
                } else if (ret == 0){
                    close(sockfd);
                } else {
                    cout << "get " << ret << " bytes of content: " << buf << endl;
                }
            }
        } else {
            cout << "something happened\n";
        }
    }
}

int main(int argc, char* argv[]){
    if (argc <= 2){
        cout << "usage incorrect\n";
        return 1;
    }

    const char* ip = argv[1];
    int port = atoi(argv[2]);

    int ret = 0;
    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));

    address.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &address.sin_addr);
    address.sin_port = htons(port);

    int listenfd = socket(PF_INET, SOCK_STREAM, 0);

    assert(listenfd >= 0);

    bind(listenfd, (struct sockaddr*)&address, sizeof(address));
    //backlog = 5
    ret = listen(listenfd, 5);

    epoll_event events[MAX_EVENT_NUMBER];
    int epollfd = epoll_create(5);
    assert (epollfd != -1);

    addfd(epollfd, listenfd, true);

    while (1){
        //res : 监听到的事件数量
        int res = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);

        if (res < 0){
            cout << "epoll failure\n";
            break;
        }

        lt(events, res, epollfd, listenfd);
        et(events, res, epollfd, listenfd);
    }

    close(listenfd);
    return 0;
}