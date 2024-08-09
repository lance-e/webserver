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
#include <sys/epoll.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>


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
    static process_pool<T>* create(int listenfd , int process_number =  8){       // only one instance
        if (m_instance == NULL){
            m_instance = new process_pool<T>(listenfd , process_number);
        }
        return m_instance;
    }

    ~process_pool(){
        delete m_instance;
    }

    void run();     //run the process pool

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

template<typename T>
process_pool<T> * process_pool<T>::m_instance = NULL;

// pipe for signal
static int sig_pipefd[2];


// set fd none blocking 
static int setnonblocking(int fd){
    int old_option = fcntl(fd , F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd , F_SETFL, new_option);
    return old_option;
}

// add target fd into epoll table 
static void addfd(int epollfd , int fd ){
    epoll_event event;
    event.data.fd  = fd;
    event.events = EPOLLIN | EPOLLET;
    epoll_ctl(epollfd , EPOLL_CTL_ADD , fd , &event);   //add epoll table
    setnonblocking(fd);            //set non blocking 
}

// remove target fd from epoll table
static void removefd(int epollfd , int fd){
    epoll_ctl(epollfd , EPOLL_CTL_DEL , fd , 0);
    close(fd);
}


//signal handler 
static void sig_handler(int signal){
    int save_errno = errno;
    int msg = signal;
    send(sig_pipefd[1] , (char*)&msg, 1, 0);
    errno = save_errno;
}

//register signal handler
static void add_signal(int signal , void(handler)(int) , bool restart = true){
    struct sigaction sa;
    memset(&sa , 0 , sizeof(sa));
    sa.sa_handler = handler;
    if (restart){
        sa.sa_flags |= SA_RESTART;
    }
    sigfillset(&sa.sa_mask);
    assert(sigaction(signal , &sa , NULL) == 0 );
}


//process pool's constructor function
template<typename T>
process_pool<T>::process_pool(int listenfd , int process_number)
:m_listenfd(listenfd),m_process_number(process_number), m_idx(-1),m_stop(false)
{
    assert(process_number > 0 && process_number <= MAX_PROCESS_NUMBER);     

    m_sub_process = new process[process_number];

    //create sub process  and pair socket
    for (int i = 0 ; i < process_number; i++){
        int ret = socketpair(PF_UNIX , SOCK_STREAM , 0 , m_sub_process[i].m_pipefd);
        assert(ret == 0 );

        m_sub_process[i].m_pid = fork();
        assert(m_sub_process[i].m_pid >=  0);
        if (m_sub_process[i].m_pid > 0 ){       //prarent process
            close(m_sub_process[i].m_pipefd[1]);
            continue;
        }else {           // sub process
            close(m_sub_process[i].m_pipefd[0]); 
            m_idx =  i;
            break;
        }
    }
}

//unify the source of event
template<typename T>
void process_pool<T>::setup_sig_pipe(){
    //create epoll table
    m_epollfd = epoll_create(5); 
    assert(m_epollfd != -1);

    //pair socket
    int ret = socketpair(PF_UNIX , SOCK_STREAM , 0 , sig_pipefd);
    assert(ret != -1);

    //set and add
    setnonblocking(sig_pipefd[1]);
    addfd(m_epollfd , sig_pipefd[0]);

    //set signal handler
    add_signal(SIGINT , sig_handler);
    add_signal(SIGTERM , sig_handler);
    add_signal(SIGCHLD , sig_handler);
    add_signal(SIGPIPE , SIG_IGN);
     
}

// public run process pool function
template<typename T>
void process_pool<T>::run(){
    if (m_idx != -1){
        run_child();
        return ;
    }
    run_parent();
}

// child process handle
template<typename T>
void process_pool<T>::run_child(){
    setup_sig_pipe();       //every process have their own epoll table

    //child process use m_idx to find m_pipefd to contact with parent process
    int pipefd = m_sub_process[m_idx].m_pipefd[1];         

    addfd(m_epollfd , pipefd);

    epoll_event events[USER_PER_PROC];
    T* users = new T[USER_PER_PROC];
    assert(users);
    
    int number = 0 ;
    int ret = -1;
    while (!m_stop){
        number = epoll_wait(m_epollfd , events , MAX_EVENT_NUMBER , -1);
        if ((number < 0 ) && (errno != EINTR)){
            printf("epoll failure\n");
            break;
        }
        for (int i = 0 ; i < number ;i++){
            int socketfd = events[i].data.fd ;
            if ((socketfd == pipefd) && (events[i].events & EPOLLIN)){      //contact with parent , mean new connection
                int client;
                ret = recv(socketfd , (void*)&client , sizeof(client) ,0);
                if ((ret < 0 && errno == EAGAIN ) || (ret == 0 )){
                    continue;
                }else {
                    struct sockaddr_in client_addr;
                    socklen_t  client_addr_len = sizeof(client_addr);
                    int connfd = accept(m_listenfd , (struct sockaddr*)&client_addr , &client_addr_len);
    
                    if (connfd < 0){
                        printf("errno is %d\n" , errno);
                        continue;
                    }

                    //add new connection into sub process's epoll table
                    addfd(m_epollfd , connfd);
                    users[connfd].init(m_epollfd ,connfd , client_addr);

                }
            }else if ((socketfd  == sig_pipefd[0]) && (events[i].events & EPOLLIN )){ // handle signal
                int sig;
                char signals[1024];
                ret = recv(socketfd ,signals , sizeof(signals) , 0);          
                if (ret <= 0 ){
                    continue;
                }else {
                    for (int i = 0 ; i < ret ; i++){
                        switch (signals[i]){
                            case SIGCHLD: 
                            {
                                pid_t pid ;
                                int state;
                                if ((pid = waitpid( -1 , &state , WNOHANG)) >  0){
                                    continue;
                                }
                                break;
                            }
                            case SIGINT:
                            case SIGTERM:
                            {
                                m_stop = true;
                                break;
                            }
                            default:
                                break;
                        }
                    }
                }
            }else if (events[i].events & EPOLLIN){
                users[socketfd].process();
            }else{
                continue;
            }

        }
    }
    delete []users;
    users = NULL;
    close(pipefd);
    close(m_epollfd);
}

//
template<typename T>
void process_pool<T>::run_parent(){
    setup_sig_pipe();       //every process have their own epoll table

    addfd(m_epollfd, m_listenfd);

    epoll_event events[MAX_EVENT_NUMBER];

    int sub_process_counter = 0 ;
    int new_conn = 1;
    int number = 0 ;
    int ret = -1;
    while(!m_stop){
        number = epoll_wait(m_epollfd , events , MAX_EVENT_NUMBER , -1);
        if ((number < 0 ) && (errno != EINTR)){
            printf("epoll failure\n");
            break;
        }
        for (int i = 0 ; i <  number ; i++){
            int socketfd  = events[i].data.fd;
            if ((socketfd == m_listenfd ) && (events[i].events & EPOLLIN)){     //new connection
                // use Round Robin to select a sub process to handle this new connection
                int i = sub_process_counter; 
                do{
                    if (m_sub_process[i].m_pid != -1){
                        break;
                    }
                    i = (i + 1) % m_process_number;
                }while(i != sub_process_counter);

                if (m_sub_process[i].m_pid == -1){     // no sub process
                    m_stop = -1;
                    break;
                }
                sub_process_counter = (i + 1) % m_process_number;
                send(m_sub_process[i].m_pipefd[0] , (char*)&new_conn , sizeof(new_conn) , 0);
                printf("send request to child\n");
            }else if ((socketfd == sig_pipefd[0]) && (events[i].events & EPOLLIN)){
                int sig;
                char signals[1024];
                ret = recv(socketfd ,signals , sizeof(signals) , 0);          
                if (ret <= 0 ){
                    continue;
                }else {
                    for (int i = 0 ; i < ret ; i++){
                        switch (signals[i]){
                            case SIGCHLD: 
                            {
                                pid_t pid ;
                                int state;
                                if ((pid = waitpid( -1 , &state , WNOHANG)) >  0){
                                    for (int i = 0 ; i < m_process_number ; i++){
                                        if (m_sub_process[i].m_pid == pid){
                                            printf("child %d exit\n" ,pid );
                                            close(m_sub_process[i].m_pipefd[0]);
                                            m_sub_process[i].m_pid = -1;
                                        }
                                    }
                                }
                                //judge whether all sub process close
                                m_stop = true;
                                for (int i = 0 ; i< m_process_number ; i++){
                                    if (m_sub_process[i].m_pid != -1){
                                        m_stop = false;
                                    }
                                }
                                break;
                            }
                            case SIGINT:
                            case SIGTERM:
                            {
                                // if parent process receive terminate signal , mean should kill all sub process , here is directly kill these
                                printf("kill all child now\n");
                                for (int i = 0 ; i < m_process_number ; i++){
                                    int pid = m_sub_process[i].m_pid;
                                    if (pid != -1){
                                        kill(pid , SIGTERM);
                                    }
                                }
                                break;
                            }
                            default:
                                break;
                        }
                    }
                }
            }
        }
    }
    close(m_epollfd);
}


#endif
