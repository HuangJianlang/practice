//
// Created by Roy on 10/30/20.
//

#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>
#include <assert.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <string.h>
#include <sys/select.h>

using namespace std;

#define BUFFER_SIZE 1023

int set_non_block(int fd){
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

//ip server ip, port server port
int unblock_connect(const char* ip, int port, int time){
    int ret = 0;
    struct sockaddr_in server_addr;
    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &server_addr.sin_addr);
    server_addr.sin_port = htons(port);

    int sockfd = socket(PF_INET, SOCK_STREAM, 0);
    int fd_old_option = set_non_block(sockfd);
    ret = connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr));

    if (ret == 0){
        //成功连接, 恢复原来的option
        cout << "connect to server immediately\n";
        fcntl(sockfd, F_SETFL, fd_old_option);
        return sockfd;
    } else if (ret != EINPROGRESS){
        cout << "unblock connect not support\n";
        return -1;
    }

    fd_set read_fds;
    fd_set write_fds;
    struct timeval timeout;

    FD_ZERO(&read_fds);
    FD_SET(sockfd, &write_fds);

    timeout.tv_sec = time;
    timeout.tv_usec = 0;

    //最大的sockfd + 1
    ret = select(sockfd + 1, NULL, &write_fds, NULL, &timeout);

    if (ret <= 0){
        cout << "connection timeout\n";
        return -1;
    }

    if (!FD_ISSET(sockfd, &write_fds)) {
        cout << "no events on sockfd found\n";
        close(sockfd);
        return -1;
    }

    int error = 0;
    socklen_t length = sizeof(error);
    //调用getsockopt 来获取并消除sockfd上的错误

    if (getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &error, &length) < 0){
        cout << "get socket option failed\n";
        close(sockfd);
        return -1;
    }

    if (error != 0){
        cout << "connection failed after select with the error: " << error << endl;
        close(sockfd);
        return -1;
    }

    //连接成功
    cout << "connection ready after select with the socket: " << sockfd << endl;
    fcntl(sockfd, F_SETFL, fd_old_option);
    return sockfd;
}

int main(int argc, char* argv[]){

    if (argc <= 2){
        cout << "usage fails" << endl;
        return 1;
    }

    const char* ip = argv[1];
    int port = atoi(argv[2]);

    int sockfd = unblock_connect(ip, port, 10);

    if (sockfd < 0){
        return 1;
    }
    close(sockfd);
    return 0;
}