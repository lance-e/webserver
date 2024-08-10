#ifndef __THREAD_POOL_H
#define __THREAD_POOL_H

#include <exception>
#include <cstdio>
#include <list>
#include "locker.h"

//thread pool
template<typename T>
class thread_pool{
public:
    thread_pool(int thread_number = 8 , int max_request = 10000);
    ~thread_pool();
    bool append(T* request);    //append new task to request list 
private:
    static void* worker(void* arg);       //
    void run();

private:
    int m_thread_number;              //number of thread
    int m_max_request_number;         //max number of request
    pthread_t* m_threads;             //all thread
    std::list<T*> m_workqueue;      //work list
    locker m_queuelocker;           //mutex
    sem m_queuestat;              //the state of work queue
    bool m_stop;                    //is the thread need stop
    
};

template<typename T>
thread_pool<T>::thread_pool(int thread_number , int max_request)
:m_thread_number(thread_number) ,m_max_request_number(max_request),m_stop(false),m_threads(NULL){
    if ((thread_number <= 0 ) || (max_request <= 0)){
          throw std::exception();
    }
    m_threads = new pthread_t[thread_number];
    if (m_threads == NULL){
        throw std::exception();
    }
    for (int i = 0 ; i < thread_number ; i++){
        printf("create the %dth thread\n" , i);    
        if (pthread_create(&m_threads[i], NULL , worker , this) != 0 ){
            delete [] m_threads;
            throw std::exception();
        }
        if (pthread_detach(m_threads[i]) != 0){
            delete [] m_threads;
            throw std::exception();
        }
    }     
}

template<typename T>
thread_pool<T>::~thread_pool(){
    delete [] m_threads;
    m_stop = true;
}


template<typename T>
bool thread_pool<T>::append(T* request){
    m_queuelocker.lock();
    if (m_workqueue.size() > m_max_request_number){
        m_queuelocker.unlock();
        return false;
    }
    m_workqueue.push_back(request);
    m_queuelocker.unlock();
    m_queuestat.post();
    return true;
}

template<typename T>
void* thread_pool<T>::worker(void* arg){
   thread_pool* pool = (thread_pool*)arg;    
   pool->run();
   return pool;
}

template<typename T>
void thread_pool<T>::run(){
    while(!m_stop){
        m_queuestat.wait();
        m_queuelocker.lock();
        if (m_workqueue.empty()){
            m_queuelocker.unlock();
            continue;
        }
        T* request = m_workqueue.front();   //get the front request from work list
        m_workqueue.pop_front();
        m_queuelocker.unlock();
        if(request == NULL){
            continue;
        }
        request->process();
    }
}

#endif

