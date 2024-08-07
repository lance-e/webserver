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
#include "epoll/epoll.h"

#define MAX_EVENTS_NUMBER 1024


int main(int argc , char* argv[]){
    if (argc <=2 ){
        printf("usage: %s ip_address port_name\n", basename(argv[0]));
        return -1;
    }

    const char* ip = argv[1];
    int port = atoi(argv[2]);
    
    //handle the address
    struct sockaddr_in addr;
    bzero(&addr , sizeof(addr));        //set number of byte to 0
    addr.sin_family = AF_INET;
    inet_pton(AF_INET , ip , &addr.sin_addr);
    addr.sin_port = htons(port);        //transfer to networt byte order


    int ret = 0 ;
    int listenfd = socket(PF_INET , SOCK_STREAM , 0);
    assert(listenfd >= 0 );

    ret = bind(listenfd, (struct sockaddr*)&addr , sizeof(addr));
    assert(ret != -1);

    ret = listen(listenfd, 5);
    assert(ret != -1 );

    epoll_event events[MAX_EVENTS_NUMBER];     //to storage the effective event
    int epollfd = epoll_create(5);
    assert( epollfd != -1);

    addfd(epollfd , listenfd , true);

    while(1){
        int ret = epoll_wait(epollfd ,events ,MAX_EVENTS_NUMBER , -1);
        if (ret < 0 ){
            printf("epoll failure\n");
            return -1;
        }
        et(events , ret , epollfd , listenfd);      
        //lt(events , ret , epollfd , listenfd);
    }
    close(listenfd);
    return 0;
}

