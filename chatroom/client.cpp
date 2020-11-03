//
// Created by Roy on 10/27/20.
//

#define _GNU_SOURCE 1
#include <iostream>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <cstring>
#include <assert.h>
#include <stdlib.h>

//#ifndef POLLRDHUP
//#define POLLRDHUP 0x2000
//#endif

using namespace std;

#define BUFFER_SIZE 64

int main(int argc, char* argv[]){

    if (argc <= 2){
        cout << "usage: %s ip_address port_number\n";
        return -1;
    }

    const char* ip = argv[1];
    int port = atoi(argv[2]);

    struct sockaddr_in server_address;
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &server_address.sin_addr);
    server_address.sin_port = htons(port);

    //AF_INET Address family -> 设置地址时使用
    //PF_INET Protocol family -> 设置socket时使用
    int sockfd = socket(PF_INET, SOCK_STREAM, 0);

    //descriptor 从0开始 -> sockfd >= 0
    assert(sockfd >= 0);

    if (connect(sockfd, (struct sockaddr*)&server_address, sizeof(server_address))){
        cout << "connection failed\n";
        close(sockfd);
        return 1;
    }

    pollfd fds[2];
    //设置stdin(fd = 0) 的可读事件
    fds[0].fd = 0;
    fds[0].events = POLLIN;
    fds[0].revents = 0;

    //sockfd
    fds[1].fd = sockfd;
    fds[1].events = POLLIN | POLLRDHUP;
    fds[1].revents = 0;

    char read_buf[BUFFER_SIZE];
    int pipefd[2];
    int ret = pipe(pipefd);
    assert(ret != -1);

    while(1) {
        ret = poll(fds, 2, -1);
        if (ret < 0) {
            cout << "poll fails\n";
            break;
        }

        if (fds[1].revents & POLLRDHUP){
            cout << "server closes the connection\n";
            break;
        } else if (fds[1].revents & POLLIN){
            memset(read_buf, 0, BUFFER_SIZE);
            recv(fds[1].fd, read_buf, BUFFER_SIZE-1, 0);
            cout << read_buf << endl;
        }

        if (fds[0].revents & POLLIN){
            //(int fd_in, loff_t *off_in, int fd_out, loff_t *off_out, size_t len, unsigned int flags);
            ret = splice(0, NULL, pipefd[1], NULL, 32768, SPLICE_F_MORE | SPLICE_F_MOVE);
            ret = splice(pipefd[0], NULL, sockfd, NULL, 32768, SPLICE_F_MORE | SPLICE_F_MOVE);
        }
    }
    close(sockfd);
    return 0;
}
