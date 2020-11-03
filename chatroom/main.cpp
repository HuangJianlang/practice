#define _GNU_SOURCE 1
#include <iostream>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

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
    // PF_INET Protocol family -> 设置socket时使用
    int sockfd = socket(PF_INET, SOCK_STREAM, 0);

    //descriptor 从0开始 -> sockfd >= 0
    assert(sockfd >= 0);

    pollfd fds[2];
    //设置stdin(fd = 0) 的可读事件
    fds[0].fd = 0;
    fds[0].events = POLLIN;
    fds[0].revents = 0;

    //sockfd
    fds[1].fd = sockfd;
    fds[1].events = POLLIN | POLLRDHUP;
    fds[1].revents = 0;


    close(sockfd);
    return 0;
}