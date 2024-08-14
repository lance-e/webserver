#ifndef __HTTP_CONN_H
#define __HTTP_CONN_H

#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/socket.h>
#include <arpa/inet.h>

//http connetion
class http_conn{
public:
    static const int FILENAME_LEN = 200;    //max filename length
    static const int READ_BUFFER_SIZE = 2048;       //max read buf size  
    static const int WRITE_BUFFER_SIZE = 1024;      //max write buf size
    //http method ,(we only support GET)
    enum METHOD{GET = 0  ,POST , HEAD , PUT , DELETE , TRACE , OPTIONS , CONNECT , PATCH};
    //main state machine's state
    enum CHECK_STAT {CHECK_STAT_REQUESTLINE , CHECK_STAT_HEADER , CHECK_STAT_CONTENT};
    //http result
    enum HTTP_CODE {NO_REQUEST , GET_REQUEST , BAD_REQUEST ,  NO_RESOURCE , FORBIDDEN_REQUEST, FILE_REQUEST ,INTERNAL_ERROR ,CLOSED_CONNECTION};
    //state of line
    enum LINE_STATUS {LINE_OK = 0 , LINE_BAD , LINE_OPEN};

public:
    http_conn(){};
    ~http_conn(){};

public:
    //initial new connection
    void init(int socketfd , struct sockaddr_in& client_addr);  
    //close connection
    void close_conn(bool real_close = true);
    //handle request
    void process();
    //no block read
    bool read();
    //no block write
    bool write();

private:
    //initial connection
    void init();
    //parse request ,(main state mathine)
    HTTP_CODE process_read();
    //write response
    bool process_write(HTTP_CODE ret);

    //these used by process_read()
    HTTP_CODE parse_request_line(char* text);
    HTTP_CODE parse_header(char* text);
    HTTP_CODE parse_content(char* text);
    HTTP_CODE do_request();
    char* get_line(){
        return m_read_buf + m_start_line;
    }
    LINE_STATUS parse_line();

    //these used by process_write()
    void unmap();
    bool add_response(const char* format ,...);
    bool add_content(const char* content);
    bool add_status_line(int status ,const char* title);
    bool add_header(int content_len); 
    bool add_content_type(const char* type);
    bool add_content_len(int content_len);
    bool add_linger();
    bool add_blank_line();

public:
    static int m_epollfd;       //all socket regiter in one epoll
    static int m_user_count;    //user count

private:
    int m_socketfd;
    struct sockaddr_in m_address;


    //read buffer
    char m_read_buf[READ_BUFFER_SIZE];
    int m_read_idx;

    //write buffer
    char m_write_buf[WRITE_BUFFER_SIZE];
    int m_write_idx;

    int m_start_line;               //index of the parsing line

    int m_checked_idx;              //index of the parsing charactor

    CHECK_STAT m_check_state;       //main state machine's state

    METHOD m_method;            //http method
    
    char m_real_file[FILENAME_LEN]; 

    char* m_url;

    char* m_version;        //http version , we only support http/1.1

    char* m_host;

    int m_content_length;    //length of request 

    bool m_linger;          //is this request need stay connection

    char* m_file_address;       //target file in memory

    struct stat m_file_stat;    //state of file

    struct iovec m_iv[2];
    int m_iv_count;     //memory block count
};


int setnonblocking(int fd);
void addfd(int epollfd , int fd ,bool one_shot);
void removefd(int epollfd , int fd);
void modfd(int epollfd , int fd , int ev);


#endif
