#include <stdio.h>
#include <sys/socket.h>
#include <assert.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <stdlib.h>


int main(int argc , char* argv[]){
    if (argc <=2 ){
        printf("usage: %s ip_address port_name\n", basename(argv[0]));
        return -1;
    }

    //handle the address
    
    inet_ntop();
    inet_pton();

    int socketfd = socket(PF_INET , SOCK_STREAM , 0);
    assert(socketfd != -1);

    int ret = bind(socketfd , (struct sockaddr*)&addr , strlen(addr));

    

}

