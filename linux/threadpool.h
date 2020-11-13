//
// Created by Jianlang Huang on 11/12/20.
//

#ifndef LINUX_THREADPOOL_H
#define LINUX_THREADPOOL_H

//半同步/半反应堆并发模式的线程池

#include <list>
#include <cstdio>
#include <exception>
#include <pthread.h>
#include <iostream>

#include "locker.h"

template< typename T >
class threadpool {
private:
    int m_thread_number; //线程池中的线程数
    int m_max_request; //请求队列中允许的最大请求数
    pthread_t* m_threads; //描述线程池的数组，其大小为m_thread_number
    std::list<T*> m_workqueue; //request queue
    locker m_queuelocker;
    sem m_queuestat;
    bool m_stop;

    static void* worker(void* arg);
    void run();

public:
    explicit threadpool(int thread_number = 16, int max_requests = 10000);
    ~threadpool();
    bool append(T* request);
};

template <typename T>
threadpool<T>::threadpool(int thread_number, int max_requests) :
    m_thread_number(thread_number), m_max_request(max_requests),
    m_stop(false), m_threads(nullptr){
    if ((thread_number <= 0) || (max_requests <= 0)){
        throw std::exception();
    }

    m_threads = new pthread_mutex_t[m_thread_number];
    if (!m_threads){
        throw std::exception();
    }

    for(int i=0; i<m_thread_number; i++){
        std::cout << "create the " << i << " thread\n";
        if (pthread_create(m_threads+i, NULL, worker, this) != 0){
            delete[] m_threads;
            throw std::exception();
        }

        if (pthread_detach(m_threads[i])){
            delete[] m_threads;
            throw std::exception();
        }
    }
}


template <typename T>
threadpool<T> ::~threadpool() {
    delete[] m_threads;
    m_stop = true;
}

template <typename T>
bool threadpool<T>::append(T *request) {
    m_queuelocker.lock();
    if (m_workqueue.size() > m_max_request){
        m_queuelocker.unlock();
        return false;
    }
    m_workqueue.push_back(request);
    m_queuelocker.unlock();
    m_queuestat.post();
    return true;
}

template <typename T>
void* threadpool<T>::worker(void *arg) {
    threadpool* pool = (threadpool*)arg;
    pool->run();
    return pool;
}

template <typename T>
void threadpool<T>::run() {
    while (!m_stop){
        m_queuestat.wait();
        m_queuelocker.lock();
        if (m_workqueue.empty()){
            m_queuelocker.unlock();
            continue;
        }
        T* request = m_workqueue.front();
        m_workqueue.pop_front();
        m_queuelocker.unlock();
        if (!request){
            continue;
        }
        request->process();
    }
}
#endif //LINUX_THREADPOOL_H
