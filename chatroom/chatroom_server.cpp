//
// Created by Roy on 10/30/20.
//

#define _GNU_SOURCE 1
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <poll.h>
#include <iostream>

#define USER_LIMIT 5
#define BUFFER_SIZE 64
#define FD_LIMIT 65535

using namespace std;

struct client_data{
    //记录来自client的address信息
    sockaddr_in address;
    char* write_buf;
    //接受读取的数据
    char buf[BUFFER_SIZE];
};

int set_non_blocking(int fd){
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
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

    ret = listen(listenfd, 5);
    assert(ret != -1);

    //创建client 连接数组
    //这里使用FD_LIMIT 是由于可以使用socket_fd(sockfd 属于FD_LIMIT内) 直接对应起client的数据结构
    client_data* users = new client_data[FD_LIMIT];

    //限制用户事件的数量
    pollfd fds[USER_LIMIT+1];
    int user_counter = 0;

    //初始化
    for (int i=0; i <= USER_LIMIT; i++){
        fds[i].fd = -1;
        fds[i].events = 0;
    }

    //用于连接client
    fds[0].fd = listenfd;
    fds[0].events = POLLIN | POLLERR;
    fds[0].revents = 0;

    while (1) {
        //user_counter 是因为当前只有server socket 一个
        ret = poll(fds, user_counter+1, -1);
        if (ret < 0){
            cout << "poll fails\n";
            break;
        }

        for (int i=0; i < user_counter+1; i++){
            if ((fds[i].fd == listenfd) && (fds[i].revents & POLLIN)){
                struct sockaddr_in client_addr;
                socklen_t client_addr_len = sizeof(client_addr_len);
                int connfd = accept(listenfd, (struct sockaddr*)&client_addr, &client_addr_len);

                if (connfd < 0){
                    cout << "erron is " << errno << endl;
                    continue;
                }

                if (user_counter >= USER_LIMIT){
                    const char* info = "too many user\n";
                    cout << info;
                    send(connfd, info, strlen(info), 0);
                    close(connfd);
                    continue;
                }

                user_counter++;
                users[connfd].address = client_addr;
                set_non_blocking(connfd);
                fds[user_counter].fd = connfd;
                fds[user_counter].events = POLLIN | POLLRDHUP | POLLERR;
                fds[user_counter].revents = 0;
                cout << "comes with a new user, now have " << user_counter << " users"<< endl;
            } else if (fds[i].revents & POLLERR) {
                cout << "get and error from " << fds[i].fd << endl;
                char errors[100];
                memset(errors, 0, 100);
                socklen_t length = sizeof(errors);
                if (getsockopt(fds[i].fd, SOL_SOCKET, SO_ERROR, &errors, &length) < 0){
                    cout << "get socket option failed\n";
                }
                continue;
            } else if (fds[i].revents & POLLRDHUP){ //断开连接
                users[fds[i].fd] = users[fds[user_counter].fd];
                close(fds[i].fd);
                fds[i] = fds[user_counter];
                i--;
                user_counter--;
                cout << "a client left\n";
            } else if (fds[i].revents & POLLIN){
                int connfd = fds[i].fd;
                memset(users[connfd].buf, 0, BUFFER_SIZE);
                ret = recv(connfd, users[connfd].buf, BUFFER_SIZE-1, 0);
                cout << "get " << ret << " bytes of data " << users[connfd].buf << " from " << connfd << endl;

                if (ret < 0){
                    if ( errno != EAGAIN){
                        close(connfd);
                        users[fds[i].fd] = users[fds[user_counter].fd];
                        fds[i] = fds[user_counter];
                        i--;
                        user_counter--;
                    }
                } else if (ret == 0){

                } else {
                    for (int j = 1; j <= user_counter; j++){
                        if (fds[j].fd == connfd){
                            continue;
                        }
                        fds[j].events |= ~POLLIN;
                        fds[j].events |= POLLOUT;
                        users[fds[j].fd].write_buf = users[connfd].buf;
                    }
                }
            } else if (fds[i].revents & POLLOUT){
                int connfd = fds[i].fd;
                if (!users[connfd].write_buf){
                    continue;
                }
                ret = send(connfd, users[connfd].write_buf, strlen(users[connfd].write_buf), 0);
                users[connfd].write_buf = NULL;
                fds[i].events |= ~POLLOUT;
                fds[i].events |= POLLIN;
            }
        }
    }

    delete [] users;
    close(listenfd);
    return 0;
}