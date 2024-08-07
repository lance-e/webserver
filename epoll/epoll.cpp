#include "epoll.h"
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
// set fd none blocking 
int setnonblocking(int fd){
    int old_option = fcntl(fd , F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd , F_SETFL, new_option);
    return old_option;
}

// add target fd into epoll table , determine whether enable ET mode
void addfd(int epollfd , int fd , bool enepollet){
    epoll_event event;
    event.data.fd  = fd;
    event.events = EPOLLIN;
    if (enepollet){
        event.events |= EPOLLET;        //enable
    }
    epoll_ctl(epollfd , EPOLL_CTL_ADD , fd , &event);   //add epoll table
    setnonblocking(fd);            //set non blocking 
}

// LT mode 
void lt(epoll_event* events ,int number, int epollfd , int listenfd ){
    char buf[BUFFER_SIZE];
    for (int i = 0 ; i < number ; i++){
        int socketfd = events[i].data.fd;
        if (socketfd == listenfd){      // listen to new connection
            struct sockaddr_in client_addr;
            socklen_t  client_addr_len = sizeof(client_addr);
            int connfd = accept(listenfd, (struct sockaddr*)&client_addr , &client_addr_len);

            addfd(epollfd , connfd , false);
        }else if (events[i].events & EPOLLIN){  // connected socket have data to read
            // as long as the socket have data to read , there will be trigger
            printf("event trigger once\n");

            memset(buf , '\0' , BUFFER_SIZE ) ;
            int size = recv(socketfd , buf , BUFFER_SIZE -1 , 0);
            if (size <= 0){         //all data had read
                close(socketfd);
                continue;
            }
            printf("receive %d byte of data: \n %s\n" , size , buf);
        }else {
            printf("happen something else\n");
        }
    }
}

// ET mode
void et(epoll_event* events , int number , int epollfd , int listenfd ){
    char buf[BUFFER_SIZE];
    for (int i = 0 ; i < number ; i++){
        int socketfd = events[i].data.fd;
        if (socketfd  == listenfd){
            struct sockaddr_in client_addr;
            socklen_t  client_addr_len = sizeof(client_addr);
            int connfd = accept(listenfd, (struct sockaddr*)&client_addr , &client_addr_len);

            addfd(epollfd , connfd , true);
        }else if(events[i].events & EPOLLIN){
            // there will be trigger once , use while to loop read all data
            printf("event trigger once\n");
            
            while(1){
                memset(buf , '\0' , BUFFER_SIZE);
                int size = recv(socketfd , buf , BUFFER_SIZE -1 , 0);
                if (size < 0){         
                    if ((errno == EAGAIN ) || (errno == EWOULDBLOCK)){      // no data to read ,and socket fd already close
                        printf("read later\n");
                        break;
                    }
                    close(socketfd);                //other error , so close socker fd
                    break;
                }else if (size == 0 ){              //all data had read , so close in advance
                    close(socketfd);
                }else {
                    printf("receive %d byte of data: \n %s\n" , size , buf);
                }
            }
        }else {
            printf("happen something else\n");
        }
    }
}
