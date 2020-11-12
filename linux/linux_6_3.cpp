//
// Created by Roy on 11/9/20.
//

#include <errno.h>
#include <iostream>
#include <cstring>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/uio.h>
#include <sys/types.h>
#include <sys/socket.h>

using namespace std;

int main(int argc, char* argv[]){
    if (argc < 4){
        cout << "usage incorrect\n";
        return 1;
    }

    char* ip = argv[1];
    int port = atoi(argv[2]);
    char* file_name = argv[3];

    int fd = open(file_name, O_RDONLY);
    assert( fd > 0);

    struct stat file_stat;
    fstat(fd, &file_stat);

    int sock = socket(PF_INET, SOCK_STREAM, 0);
    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &address.sin_addr);
    address.sin_port = htons(port);

    bind(sock, (struct sockaddr*)&address, sizeof(address));

    listen(sock, 5);

    struct sockaddr_in client;
    socklen_t client_len = sizeof(client);
    int connd = accept(sock, (struct sockaddr*)&client, &client_len);

    if (connd < 0){
        cout << "error when connect with client\n";
    } else {
        //linux
        sendfile(connd, fd, NULL, file_stat.st_size);
        close(connd);
    }

    close(sock);
    return 0;
}

