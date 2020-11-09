//
// Created by Jianlang Huang on 11/7/20.
//

#ifndef THREAD_POOL_PROCESSPOOL_H
#define THREAD_POOL_PROCESSPOOL_H

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

using namespace std;

//描述一个子进程的类, m_pid 是目标子进程的pid, m_pidfd 是父进程和子进程通信用的管道
class process{
public:
    //进程本身的pid
    //父进程 = -1
    //子进程 >= 0
    pid_t m_pid;
    //父进程与子进程之间通信管道
    int m_pipefd[2];

public:
    //default ctor
    process(): m_pid(-1){}
};

//使用单例模式创建线程池
template <typename T>
class ProcessPool {
private:
    //进程池允许的最大进程数量
    static const int MAX_PROCESS_NUMBER = 16;
    //每个进程能够处理的客户数量
    static const int USER_PER_PROCESS = 65536;
    //epoll 能处理的事件数
    static const int MAX_EVENT_NUMBER = 10000;
    //池子中进程池的总数
    int m_process_number;
    int m_idx;
    int m_epollfd;
    int m_listenfd;
    int m_stop;
    //保存所有子进程的描述信息
    process* m_sub_process;

    //进程池静态实例
    static ProcessPool<T>* m_instance;

private:
    ProcessPool(int listenfd, int process_number = 8);

    void setup_sig_pipe();
    void run_parent();
    void run_child();

public:
    static ProcessPool<T>* create(int listenfd, int process_number = 8){
        if (!m_instance) {
            m_instance = new ProcessPool<T>(listenfd, process_number);
        }
        return m_instance;
    }

    ~ ProcessPool(){
        delete[] m_sub_process;
    }

    void run();
};

//static variable
template <typename  T>
ProcessPool<T>* ProcessPool<T>::m_instance = nullptr;

//静态变量区
static int sig_pipefd[2];


static int set_non_blocking(int fd){
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

static void addfd(int epollfd, int fd){
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET;
    //epoll_ctl -> operation -> EPOLL_CTL_ADD -> bind fd and event
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    set_non_blocking(fd);
}

static void removefd(int epollfd, int fd){
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
    close(fd);
}

static void sig_handler(int sig){
    int save_errno = errno;
    int msg = sig;
    send(sig_pipefd[1], (char*)&msg, 1, 0);
    errno = save_errno;
}

static void addsig(int sig, void(handler)(int), bool restart = true){
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sig_handler;
    if (restart){
        sa.sa_flags |= SA_RESTART;
    }
    //block(mask) all signal
    sigfillset(&sa.sa_mask);
    assert( sigaction(sig, &sa, NULL) != -1);
}

//线程池实现
template <typename T>
ProcessPool<T> ::ProcessPool(int listenfd, int process_number){
    m_listenfd = listenfd;
    m_process_number = process_number;
    m_idx = -1;
    m_stop = false;

    assert( (process_number > 0) && (process_number <= MAX_PROCESS_NUMBER));
    m_sub_process = new process[process_number];
    assert ( m_sub_process);

    //创建process_number 个子进程，建立他们与父进程之间的管道
    for (int i=0; i < process_number; i++){
        int ret = socketpair(PF_UNIX, SOCK_STREAM, 0, m_sub_process[i].m_pipefd);
        assert( ret == 0);

        m_sub_process[i].m_pid = fork();
        assert( m_sub_process[i].m_pid >= 0);

        if (m_sub_process[i].m_pid > 0){
            //parent process
            close(m_sub_process[i].m_pipefd[1]);
            continue;
        } else {
            //child process
            close(m_sub_process[i].m_pipefd[0]);
            m_idx = i;
            break;
        }
    }
}

//统一事件源
template<typename T>
void ProcessPool<T> ::setup_sig_pipe() {
    m_epollfd = epoll_create(5);
    assert(m_epollfd != -1);
    //这个事件源是信号，同时以socketpair 作为载体
    int ret = socketpair(PF_UNIX, SOCK_STREAM, 0, sig_pipefd);
    assert(ret != -1);

    set_non_blocking(sig_pipefd[1]);
    addfd(m_epollfd, sig_pipefd[0]);

    addsig(SIGCHLD, sig_handler);
    addsig(SIGTERM, sig_handler);
    addsig(SIGINT, sig_handler);
    addsig(SIGPIPE, SIG_IGN);
}

//如何区别父进程和子进程？ 类似pid 原理:
// 父进程 m_pid = -1, 子进程 m_pid >= 0

template <typename T>
void ProcessPool<T>::run() {
    if (m_idx != -1){
        run_child();
        return;
    }
    run_parent();
}

template <typename T>
void ProcessPool<T> ::run_child() {
    //在多进程中 sig_pipe如何变化
    setup_sig_pipe();
    //socketpipe 全双工
    int pipefd = m_sub_process[m_idx].m_pipefd[1];

    addfd(m_epollfd, pipefd);

    epoll_event events[MAX_EVENT_NUMBER];
    T* users = new T[USER_PER_PROCESS];
    assert(users);

    int number = 0;
    int ret = -1;

    while (!m_stop){
        number = epoll_wait(m_epollfd, events, MAX_EVENT_NUMBER, -1);

        if ((number < 0) && (errno != EINTR)){
            std::cout << "epoll fail\n";
            break;
        }

        for (int i=0; i < number; i++){
            int sockfd = events[i].data.fd;
            //来自父进程的信息
            if ((sockfd == pipefd) && (events[i].events & EPOLLIN)){
                //从父子进程之间的管道读取数据
                int client = 0;
                //用于告诉child process 有新链接
                ret = recv(sockfd, (char*)&client, sizeof(client), 0);
                if (((ret < 0) && (errno != EAGAIN)) || ret == 0){
                    //ret == 0 -> connection close
                    //EAGAIN -> resource currently unavailable
                    continue;
                } else {
                    struct sockaddr_in client_address;
                    socklen_t client_address_len = sizeof(client_address);

                    //child中一直保留有祝进程中的listenfd
                    int connfd = accept(m_listenfd, (struct sockaddr*)&client_address, &client_address_len);

                    if (connfd < 0){
                        std::cout << "errno is: " << errno << endl;
                        continue;
                    }

                    addfd(m_epollfd, connfd);

                    users[connfd].init(m_epollfd, connfd, client_address);
                }
            } else if ((sockfd == sig_pipefd[0]) && (events[i].events & EPOLLIN)){
                int sig;
                char signals[1024];
                ret = recv(sig_pipefd[0], signals, sizeof(signals), 0);
                if ( ret <= 0){
                    continue;
                } else {
                    for (int i = 0; i < ret; i++){
                        switch( signals[i] ){
                            case SIGCHLD:{
                                pid_t pid;
                                int stat;
                                while ((pid = waitpid(-1, &stat, WNOHANG)) > 0){
                                    continue;
                                }
                                break;
                            }
                            case SIGTERM:
                            case SIGINT:{
                                m_stop = true;
                                break;
                            }
                            default:
                                break;
                        }
                    }
                }
            } else if (events[i].events & EPOLLIN){
                //用于处理业务逻辑
                users[sockfd].process();
            } else {
                continue;
            }
        }
    }

    delete[] users;
    users = nullptr;
    close(pipefd);
    //每一个进程都确保有一个自己的epoll事件表
    close(m_epollfd);
}

template <typename T>
void ProcessPool<T> ::run_parent() {
    setup_sig_pipe();

    addfd(m_epollfd, m_listenfd);
    epoll_event events[MAX_EVENT_NUMBER];

    int sub_process_counter = 0;
    int new_conn = 1;
    int number = 0;
    int ret = -1;

    while (!m_stop){
        number = epoll_wait(m_epollfd, events, MAX_EVENT_NUMBER, -1);
        if ( (number < 0) && (errno != EINTR)){
            cout << "epoll fails\n";
            break;
        }

        for (int i=0; i < number; i++){
            int sockfd = events[i].data.fd;
            if ( sockfd == m_listenfd){
                //轮训分配一个子进程处理
                int i = sub_process_counter;
                do {
                    //找到一个子进程
                    if (m_sub_process[i].m_pid != -1){
                        break;
                    }
                    i = (i+1) % m_process_number;
                } while (i != sub_process_counter);

                if (m_sub_process[i].m_pid == -1){
                    m_stop = true;
                    break;
                }
                sub_process_counter = (i+1)%m_process_number;
                send(m_sub_process[i].m_pipefd[0], (char*)&new_conn, sizeof(new_conn), 0);
                cout << "send request to child " << i << endl;
            } else if ((sockfd == sig_pipefd[0]) && (events[i].events & EPOLLIN)){
                int sig;
                char signals[1024];
                ret = recv(sig_pipefd[0], signals, sizeof(signals), 0);
                if (ret <= 0){
                    continue;
                } else {
                    for (int i = 0; i < ret; i++){
                        switch (signals[i]){
                            case SIGCHLD:{
                                pid_t pid;
                                int stat;
                                while ( (pid = waitpid(-1, &stat, WNOHANG)) > 0){
                                    for (int i = 0; i < m_process_number; i++){
                                        if (m_sub_process[i].m_pid == pid){
                                            cout << "child process " << pid << " join\n";
                                            close(m_sub_process[i].m_pipefd[0]);
                                            m_sub_process[i].m_pid = -1;
                                        }
                                    }
                                }
                                m_stop = true;
                                for (int i=0; i<m_process_number; i++){
                                    if (m_sub_process[i].m_pid != -1){
                                        m_stop = false;
                                    }
                                }
                                break;
                            }
                            case SIGTERM:
                            case SIGINT:{
                                cout << "kill all child processes\n";
                                for (int i=0; i < m_process_number; i++){
                                    int pid = m_sub_process[i].m_pid;
                                    if (pid != -1){
                                        kill(pid, SIGTERM);
                                    }
                                }
                                break;
                            }
                            default:
                                break;
                        }
                    }
                }
            } else {
                continue;
            }
        }
    }
    close(m_epollfd);
}
#endif //THREAD_POOL_PROCESSPOOL_H
