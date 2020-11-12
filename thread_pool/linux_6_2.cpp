//
// Created by Jianlang Huang on 11/8/20.
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

using namespace std;

#define BUFFER_SIZE 1024

static const char* status_line[2] = {"200 OK", "500 Internal server error"};

//simple
int main(int argc, char* argv[]){
    if ( argc <= 3){ // ./xxx ip port file_name -> 4 args
        cout << "usage: ./xxx ip port_num file_name\n";
        return 1;
    }

    char* ip = argv[1];
    int port = atoi(argv[2]);
    const char* file_name = argv[3];

    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &address.sin_addr);
    address.sin_port = htons(port);

    int listenfd = socket(PF_INET, SOCK_STREAM, 0);
    bind(listenfd, (struct sockaddr*)&address, sizeof(address));
    int ret = listen(listenfd, 5);

    struct sockaddr_in client;
    socklen_t client_len = sizeof(client);
    int connfd = accept(listenfd, (struct sockaddr*)&client, &client_len);
    if (connfd < 0){
        cout << "errno is: " << errno << endl;
    } else {
        //用于保存HTTP 应答的状态行 头部字段和一个空行的缓存区
        char header_buf[ BUFFER_SIZE ];
        memset(header_buf, 0, BUFFER_SIZE);
        //用于存放目标文件内容的应用程序缓存
        char* file_buf;
        //用于存放目标文件的属性 比如是否为目录， 文件大小等
        struct stat file_stat;

        bool valid = true;

        int len = 0;
        if (stat(file_name, &file_stat) < 0){
            valid = false;
            cout << "file does not exist.\n";
        } else { // file exists
            if (S_ISDIR(file_stat.st_mode)){
                valid = false;
                cout << "file is a directory\n";
            } else if (file_stat.st_mode & S_IROTH){
                int fd = open(file_name, O_RDONLY);
                file_buf = new char[file_stat.st_size + 1];
                memset(file_buf, 0, file_stat.st_size + 1);
                if (read(fd, file_buf, file_stat.st_size) < 0){
                    valid = false;
                }
            } else {
                valid = false;
            }
        }

        //目标文件有效 发送正常的HTTP 应答
        if (valid){
            ret = snprintf(header_buf, BUFFER_SIZE-1, "%s %s\r\b", "HTTP/1.1", status_line[0]);
            len += ret;
            ret = snprintf(header_buf + len, BUFFER_SIZE-1-len, "Content-Length: %d\r\n", file_stat.st_size);

            len += ret;
            ret = snprintf(header_buf+len, BUFFER_SIZE-1-len, "%s", "\r\n");

            struct iovec iv[2];
            iv[0].iov_base = header_buf;
            iv[0].iov_len = strlen(header_buf);
            iv[1].iov_base = file_buf;
            iv[1].iov_len = file_stat.st_size;
            ret = writev(connfd, iv, 2);
        } else {
            ret = snprintf(header_buf, BUFFER_SIZE-1, "%s %s\r\n", "HTTP/1.1", status_line[1]);
            len += ret;
            ret = snprintf(header_buf+len, BUFFER_SIZE-1-len, "%s", "\r\n");
            send(connfd, header_buf, strlen(header_buf), 0);
        }
        close(connfd);
        delete[] file_buf;

    }

    close(listenfd);
    return 0;
}