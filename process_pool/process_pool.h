#ifndef __PROCESS_POOL_H
#define __PROCESS_POOL_H

#include <stdio.h>
#include <sys/socket.h>
#include <assert.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

#define BUFFER_SIZE 10

//sub process class
class process{
public:
    process():m_pid(-1){};
public:
    pid_t m_pid;
    int m_pipefd[2];
};

//process pool
template<typename T>
class process_pool{
private:
    process_pool(int listenfd , int process_number = 8 );   // define to private , only could called by create();
public:
    static process_pool<T> create(int listenfd , int process_number =  8){       // only one instance
        if (!m_instance){
            m_instance = new process_pool<T>(listenfd , process_number);
        }
        return m_instance;
    }

    ~process_pool(){
        delete m_instance;
    }

    void run();     //run the process pool

private:
   void setup_sig_pipe();
   void run_child(); 
   void run_parent();

private:
    static const int MAX_PROCESS_NUMBER = 16;
    static const int USER_PER_PROC = 65536;
    static const int MAX_EVENT_NUMBER = 10000;
   
    int m_process_number ;          //number of process
    int m_idx ;                     //index of process
    int m_epollfd;                  //epoll table fd 
    int m_listenfd;                 //listen fd
    bool m_stop;                    //use to judge should sub process should stop
    process* m_sub_process;             // all sub process
    static process_pool<T>* m_instance;     //process pool instance 

};


#endif
