#include <stdio.h>
#include <sys/socket.h>
#include <assert.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <string>



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
    if (ret == -1 ){
        printf("bind wrong, errno:%d\n", errno);
        return -1;
    }


    ret = listen(listenfd, 5);
    assert(ret != -1 );

    struct sockaddr_in client_addr;
    socklen_t  client_addr_len = sizeof(client_addr);
    int socketfd = accept(listenfd, (struct sockaddr*)&client_addr , &client_addr_len);
    
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET , &client_addr , client_ip , sizeof(client_ip));
    printf("client: address:%s\tport:%s\n" ,client_ip , std::to_string(ntohs(client_addr.sin_port)).c_str());

    char buf[1024];
    int size = recv(socketfd , buf , sizeof(buf),  0);
    printf("receive %d data: \n %s\n" , size , buf);
    size = send(socketfd , buf , size , 0);
    close(socketfd);
    close(listenfd);
    
}

