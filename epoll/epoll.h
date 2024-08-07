#ifndef __EPOLL_H
#define __EPOLL_H
#include <sys/epoll.h>

#define BUFFER_SIZE 10

int setnonblocking(int fd);
void addfd(int epollfd , int fd , bool enepollet);
void lt(epoll_event* events ,int number, int epollfd , int listenfd );
void et(epoll_event* events ,int number, int epollfd , int listenfd );

#endif
