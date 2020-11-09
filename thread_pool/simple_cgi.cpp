//
// Created by Jianlang Huang on 11/8/20.
//

#include <iostream>
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
#include <sys/epoll.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>

#include "ProcessPool.h"

class cgi_conn{
private:
    static const int BUFFER_SIZE = 1024;
    static int m_epollfd;
    int m_sockfd;
    sockaddr_in m_address;
    char m_buf[BUFFER_SIZE];
    int m_read_idx;

public:
    cgi_conn(){};
    ~cgi_conn(){};

    void init(int epollfd, int sockfd, const sockaddr_in& client_addr){
        m_epollfd = epollfd;
        m_sockfd = sockfd;
        m_address = client_addr;
        memset(m_buf, 0, BUFFER_SIZE);
        m_read_idx = 0;
    }

    void process(){
        int idx = 0;
        int ret = -1;
        while (true){
            idx = m_read_idx;
            ret = recv(m_sockfd, m_buf+idx, BUFFER_SIZE - 1 - idx, 0);

            if (ret < 0){
                if (errno != EAGAIN){
                    removefd(m_epollfd, m_sockfd);
                }
                break;
            } else if (ret == 0){
                removefd(m_epollfd, m_sockfd);
                break;
            } else {
                m_read_idx += ret;
                cout << "user content is " << m_buf << endl;
                //遇到 "\r\n" 开始处理客户请求
                for (; idx < m_read_idx; idx++){
                    if ((idx >= 1) && (m_buf[idx-1] == '\r') && (m_buf[idx] == '\n')){
                        break;
                    }
                }

                //没有遇到字符"\r\n" 需要读取更多数据
                if (idx == m_read_idx){
                    continue;
                }
                m_buf[idx - 1] = 0;

                char* file_name = m_buf;
                //判断客户要运行的cgi程序是否存在
                if (access(file_name, F_OK) == -1){
                    removefd(m_epollfd, m_sockfd);
                    break;
                }

                ret = fork();
                if (ret == -1){
                    removefd(m_epollfd, m_sockfd);
                    break;
                } else if (ret > 0){
                    removefd(m_epollfd, m_sockfd);
                    break;
                } else {
                    close(STDOUT_FILENO);
                    dup(m_sockfd);
                    execl( m_buf, m_buf, 0);
                    exit(0);
                }
            }
        }
    }
};

int main(int argc, char* argv[]){
    if (argc <= 2){
        cout << "usage error, please input ip and port number\n";
        return 1;
    }

    const char* ip = argv[1];
    int port = atoi(argv[2]);

    int listenfd = socket(PF_INET, SOCK_STREAM, 0);

    int ret = 0;
    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &address.sin_addr);
    address.sin_port = htons(port);

    bind(listenfd, (struct sockaddr*)&address, sizeof(address));
    ret = listen(listenfd, 5);

    ProcessPool<cgi_conn>* pool = ProcessPool<cgi_conn>::create(listenfd);

    if (pool){
        pool->run();
        delete pool;
    }
    close(listenfd);
    return 0;
}