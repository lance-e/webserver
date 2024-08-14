#include <stdio.h>
#include <sys/socket.h>
#include <assert.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <signal.h>
#include "errno.h"

#include "thread_pool.h"
#include "http_conn.h"

#define MAX_FD 65536
#define MAX_EVENTS_NUMBER 1024

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

//send error information
void show_error(int connfd , const char* info){
    printf("show_error: %s\n" , info);
    send(connfd , info ,strlen(info), 0);
    close(connfd);
}


int main(int argc , char* argv[]){
    if (argc <=2 ){
        printf("usage: %s ip_address port_name\n", basename(argv[0]));
        return -1;
    }

    const char* ip = argv[1];
    int port = atoi(argv[2]);


    //ignore SIGPIPE
    add_signal(SIGPIPE , SIG_IGN);

    //create thread pool
    thread_pool<http_conn> * pool = NULL;
    try{
        pool = new thread_pool<http_conn>;
    }
    catch( ... ){
        return 1;
    }
    

    //prepare for every user a http connection
    http_conn* users = new http_conn[MAX_FD];
    assert(users);
    int user_count = 0 ;



    //handle the address
    struct sockaddr_in addr;
    bzero(&addr , sizeof(addr));        //set number of byte to 0
    addr.sin_family = AF_INET;
    inet_pton(AF_INET , ip , &addr.sin_addr);
    addr.sin_port = htons(port);        //transfer to networt byte order

    //begin 

    int ret = 0 ;
    int listenfd = socket(PF_INET , SOCK_STREAM , 0);
    assert(listenfd >= 0 );

    struct linger tmp = {1,  0 };
    setsockopt(listenfd , SOL_SOCKET , SO_LINGER , &tmp , sizeof(tmp));


    ret = bind(listenfd, (struct sockaddr*)&addr , sizeof(addr));
    assert(ret != -1);

    ret = listen(listenfd, 5);
    assert(ret != -1 );

    
    epoll_event events[MAX_EVENTS_NUMBER];
    int epollfd = epoll_create(10);
    assert(epollfd);
    addfd(epollfd , listenfd , false);
    http_conn::m_epollfd = epollfd ;

    int number = 0 ;
    while(true){
        number = epoll_wait(epollfd , events , MAX_EVENTS_NUMBER , -1);
        if ((number < 0 ) && (errno != EINTR)){
            printf("epoll failure\n");
            break;
        }
        for (int i = 0 ; i < number ;i++){
            int socketfd = events[i].data.fd;
            if (socketfd == listenfd){          //new connection
                struct sockaddr_in client_addr;
                socklen_t  client_addr_len = sizeof(client_addr);
                int connfd = accept(listenfd , (struct sockaddr*)&client_addr , &client_addr_len);

                printf("new connection%d\n" , connfd);

                if (connfd < 0){
                    printf("errno is %d\n" , errno);
                    continue;
                }

                if (user_count >= MAX_FD ){
                    show_error(connfd, "Internal server busy");
                    continue;
                }

                users[connfd].init(connfd , client_addr);
            }else if (events[i].events & (EPOLLRDHUP | EPOLLERR | EPOLLHUP)){
                printf("exception happened\n" );
                //exception handing : close connection
                users[socketfd].close_conn();
            }else if (events[i].events & EPOLLIN){
                printf("epollin event\n");
                if ( users[socketfd].read()){
                    pool->append( users + socketfd );
                }else {
                    users[socketfd].close_conn();
                }
            }else if (events[i].events & EPOLLOUT ){
                printf("epollout event\n");
                if (!users[socketfd].write() ){
                    users[socketfd].close_conn();
                }
            }else {
            }
        }
    }

    close(epollfd);
    close(listenfd);
    delete [] users;
    delete pool;
    return 0;
}

