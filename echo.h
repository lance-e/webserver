#ifndef __ECHO_H
#define __ECHO_H

#include <stdio.h>
#include <string>
#include <sys/socket.h>
#include <assert.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include "process_pool.h"

class echo_conn{
public:
    echo_conn(){};
    ~echo_conn(){};
    void init(int epollfd , int socketfd, struct sockaddr_in client_addr){
        echo_epollfd = epollfd;
        echo_socketfd  = socketfd;
        echo_client_addr = client_addr;
        memset(buf , 0 , sizeof(buf));
        read_idx = 0;
    }
    void process(){
        int idx = 0;
        int ret = -1;
        while(true){
            idx = read_idx;
            ret = recv(echo_socketfd , buf + idx  , MAX_BUFF_SIZE -1 - idx , 0);
            if (ret < 0 ){
                if (errno != EAGAIN){       // something went wrong , so close the connection
                    removefd(echo_epollfd , echo_socketfd);
                }
                break;
            }else if (ret == 0){
                removefd(echo_epollfd , echo_socketfd);
                break;
            }else {
                read_idx += ret;
                for (; idx < read_idx ; ++idx){
                    if ((idx >= 1) && (buf[idx-1] == '\r' ) && (buf[idx] == '\n')){  //have "\r\n"
                        break;
                    }
                }
                if (idx == read_idx){     //no "\r\n", need to read more data
                    continue;
                }
                buf[idx -1] = '\0';

                 const char *response =
                            "HTTP/1.1 200 OK\r\n"
                            "Content-Type: application/json\r\n"
                            "Cache-Control: max-age=60\r\n"
                            "X-Request-Id: 12345abcde\r\n"
                            "\r\n"
                            "["
                            "{\"id\":1,\"name\":\"John Doe\",\"email\":\"john@example.com\"},"
                            "{\"id\":2,\"name\":\"Jane Smith\",\"email\":\"jane@example.com\"},"
                            "{\"id\":3,\"name\":\"Bob Johnson\",\"email\":\"bob@example.com\"}"
                            "]";



                //echo read data
                send(echo_socketfd , response , strlen(response) , 0);
                printf("send response\n");
            }
        }
    }

private:
    static const int MAX_BUFF_SIZE = 1024;
    static inline int echo_epollfd = -1;
    int echo_socketfd;
    sockaddr_in echo_client_addr;
    char buf[MAX_BUFF_SIZE];
    int read_idx;

};



#endif
