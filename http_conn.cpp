#include "http_conn.h"
#include <stdio.h>
#include <cstdlib>
#include <assert.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <string.h>
#include <sys/mman.h>
#include <errno.h>
#include <stdarg.h>



const char* ok_200_title = "OK";
const char* error_400_title = "Bad Request";
const char* error_400_form = "Your request has bad syntax or is inherently impossible to satisfy\n";
const char* error_403_title = "Forbidden";
const char* error_403_form = "You don't have permission to get file from this server\n";
const char* error_404_title = "Not Found";
const char* error_404_form = "The requested file not found in this server\n";
const char* error_500_title = "Internel Error";
const char* error_500_form = "There was an unusal problem serving the requested file\n";

const char* doc_root= "/root/html";

// set fd none blocking 
int setnonblocking(int fd){
    int old_option = fcntl(fd , F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd , F_SETFL, new_option);
    return old_option;
}

// add target fd into epoll table 
void addfd(int epollfd , int fd ,bool one_shot){
    epoll_event event;
    event.data.fd  = fd;
    event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    if (one_shot){
        event.events |= EPOLLONESHOT;
    }
    epoll_ctl(epollfd , EPOLL_CTL_ADD , fd , &event);   //add epoll table
    setnonblocking(fd);            //set non blocking 
}

// remove target fd from epoll table
void removefd(int epollfd , int fd){
    epoll_ctl(epollfd , EPOLL_CTL_DEL , fd , 0);
    close(fd);
}

// modify target fd from epoll table
void modfd(int epollfd , int fd , int ev){
    epoll_event event; 
    event.data.fd = fd;
    event.events = ev | EPOLLONESHOT | EPOLLRDHUP ;
    epoll_ctl(epollfd , EPOLL_CTL_MOD , fd , & event);
}


int http_conn::m_user_count  = 0 ;
int http_conn::m_epollfd = -1;


void http_conn::init(int socketfd , struct sockaddr_in& client_addr){
    m_socketfd = socketfd;
    m_address = client_addr;
    addfd(m_epollfd , socketfd , true);
    m_user_count ++;

    init();
}

void http_conn::init(){
    m_read_idx = 0 ; 
    m_write_idx = 0 ;
    m_start_line = 0 ;
    m_checked_idx = 0;
    m_method = GET;
    m_url = 0;
    m_version = 0;
    m_host = 0;
    m_content_length = 0 ; 
    m_linger = false;
    
    memset(m_read_buf , 0 , READ_BUFFER_SIZE);
    memset(m_write_buf, 0 , WRITE_BUFFER_SIZE);
    memset(m_real_file, 0 , FILENAME_LEN);
    
}

void http_conn::close_conn(bool real_close){
    if (real_close && (m_socketfd != -1)){
        removefd(m_epollfd , m_socketfd);
        m_socketfd = -1;
        m_user_count--;
    }
}



bool http_conn::read(){
    if (m_read_idx >= READ_BUFFER_SIZE){
        return false;
    }

    int bytes = 0;
    while(true){
        bytes = recv(m_socketfd , m_read_buf , READ_BUFFER_SIZE - m_read_idx , 0);
        if (bytes < 0 ){
            if ((errno == EAGAIN ) || (errno == EWOULDBLOCK)){
                break;
            }
            return false;
        }else if (bytes ==  0 ){
            return false;
        }
        m_read_idx += bytes;
    }
    return true;
}


//main state mathine
http_conn::HTTP_CODE http_conn::process_read(){
    LINE_STATUS line_state = LINE_OK;     
    int ret = NO_REQUEST;
    char* text;

    while(((m_check_state == CHECK_STAT_CONTENT ) && (line_state == LINE_OK)) ||((line_state = parse_line())== LINE_OK) ){
        text = get_line();     
        m_start_line = m_checked_idx;
        printf("get 1 http line :%s\n" , text);
        switch (m_check_state){
            case CHECK_STAT_REQUESTLINE:
                {
                    ret = parse_request_line(text);
                    if (ret == BAD_REQUEST){
                        return BAD_REQUEST;
                    }
                    break;
                }
            case CHECK_STAT_HEADER:
                {
                    ret = parse_request_line(text);
                    if (ret == BAD_REQUEST){
                        return BAD_REQUEST;
                    }else if (ret == GET_REQUEST){
                        return do_request();
                    }
                    break;
                }
            case CHECK_STAT_CONTENT:
                {
                    ret = parse_request_line(text);
                    if (ret == GET_REQUEST){
                        return do_request();
                    }
                    line_state = LINE_OPEN;
                    break;
                }
            default:
                {
                    return INTERNAL_ERROR;
                }
        }
    }
    return NO_REQUEST;

}

//sub state machine
http_conn::LINE_STATUS http_conn::parse_line(){
    char temp;
    for (; m_checked_idx < m_read_idx ;m_checked_idx++){
        temp = m_read_buf[m_checked_idx];
        if (temp == '\r'){
            if (m_checked_idx + 1 == m_read_idx){
                return LINE_OPEN;
            }
            if (m_read_buf[m_checked_idx + 1 ] == '\n'){
                m_read_buf[m_checked_idx++] = '\0';
                m_read_buf[m_checked_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
        if (temp == '\n'){
            if ((m_checked_idx > 1) && (m_read_buf[m_checked_idx - 1] == '\r')){
                m_read_buf[m_checked_idx -1 ] = '\0';
                m_read_buf[m_checked_idx ++ ] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    return LINE_OPEN;
}


//parse request line : method , url , http version 
http_conn::HTTP_CODE http_conn::parse_request_line(char* text){
    m_url = strpbrk(text, " \t"); 
    if (!m_url){
        return BAD_REQUEST;
    }
    *m_url++ = '\0';

    char* method = text;
    if (strcasecmp(method , "GET") == 0){
        m_method = GET;
    }else {
        return BAD_REQUEST;
    }

    m_url += strspn(m_url , " \t");    
    m_version = strpbrk(m_url , " \t");
    if (!m_version){
        return BAD_REQUEST;
    }
    *m_version++ = '\0';

    m_version += strspn(m_version , " \t");
    if (strcasecmp(m_version , "HTTP/1.1") != 0){
        return BAD_REQUEST; 
    }
    
    if (strncasecmp(m_url , "http://" , 7 ) == 0 ){
        m_url += 7;
        m_url = strchr(m_url , '/');
    }

    if ( !m_url || m_url[0] != '/'){
        return BAD_REQUEST;
    }

    m_check_state = CHECK_STAT_HEADER;
    return NO_REQUEST;
}

//parse header
http_conn::HTTP_CODE http_conn::parse_header(char* text){
    if (text[0] == '\0'){
        if (m_content_length != 0 ){
            m_check_state  =  CHECK_STAT_CONTENT ; 
            return NO_REQUEST;
        }
        return GET_REQUEST;
    }else if (strncasecmp(text , "Connection:" , 11) == 0){
        text += 11;
        text += strspn(text , " \t");
        if (strcasecmp(text , "keep-alive") == 0 ){
            m_linger = true;
        }
    }else if (strncasecmp(text , "Content-Length:" , 15) == 0 ){
        text += 15; 
        text += strspn(text , " \t");
        m_content_length = atoi(text);
    }else if (strncasecmp(text , "Host:" , 5) == 0 ){
        text += 5;
        text += strspn(text , " \t");
        m_host = text;
    }else {
        printf("oop , unkonw header\n");
    }

    return NO_REQUEST;
}

//parse content( here we don't really parse content , just judge whether is it whole read)
http_conn::HTTP_CODE http_conn::parse_content(char* text){
    if (m_read_idx >= (m_content_length + m_checked_idx)){
        text[m_content_length] = '\0';
        return GET_REQUEST;
    }
    return NO_REQUEST;
}

//
http_conn::HTTP_CODE http_conn::do_request(){
    strcpy(m_real_file , doc_root);
    int len = strlen(doc_root);
    strncpy(m_real_file + len , m_url , FILENAME_LEN - len - 1);

    if (stat(m_real_file , &m_file_stat) < 0){
        return NO_RESOURCE;
    }else if (!(m_file_stat.st_mode & S_IROTH)){
        return FORBIDDEN_REQUEST;
    }else if (S_ISDIR(m_file_stat.st_mode)){
        return BAD_REQUEST;
    }

    int fd = open(m_real_file , O_RDONLY);
    m_file_address = (char*)mmap(0 , m_file_stat.st_size , PROT_READ, MAP_PRIVATE , fd , 0 );
    close(fd);
    return FILE_REQUEST;
}


//unmap
void http_conn::unmap(){
    if (m_file_address){
        munmap(m_file_address , m_file_stat.st_size);
        m_file_address = 0 ;
    }
}




//write 
bool http_conn::write(){
    int temp = 0;
    int bytes_have_send = 0 ;
    int bytes_need_send = m_write_idx;
    if (bytes_need_send == 0 ){
        modfd(m_epollfd , m_socketfd , EPOLLIN);
        init();
        return true;
    }
    while(true){
        temp = writev(m_socketfd , m_iv , m_iv_count);
        if (temp < 0 ){
            if (errno == EAGAIN){
                modfd(m_epollfd , m_socketfd , EPOLLIN);
                return true;
            }
            unmap();
            return false;
        }
        bytes_have_send += temp;
        bytes_need_send -= temp;

        if (bytes_need_send <= bytes_have_send){
            unmap();
            if (m_linger){
                init();
                modfd(m_epollfd ,m_socketfd , EPOLLIN);
                return true;
            }else {
                modfd(m_epollfd ,m_socketfd , EPOLLIN);
                return true;
            }
        }
    }
}


//write response data into buffer
bool http_conn::add_response(const char* format , ...){
    if (m_write_idx >= WRITE_BUFFER_SIZE){
        return false;
    }
    va_list arg_list;
    va_start(arg_list , format);
    int len = vsnprintf(m_write_buf + m_write_idx , WRITE_BUFFER_SIZE - 1 - m_write_idx , format , arg_list);
    if (len >= (WRITE_BUFFER_SIZE - m_write_idx - 1 )){
        return false;
    }

    m_write_idx += len;
    va_end(arg_list);
    return true;
}

bool http_conn::add_content(const char* content){
    return add_response("%s" , content);
}

bool http_conn::add_status_line(int status , const char* title){
    return add_response("%s %d %s\r\n" , "HTTP/1.1" , status , title);
}

bool http_conn::add_header(int content_len){
    add_content_len(content_len);
    add_linger();
    add_blank_line();
    return true;
}

bool http_conn::add_content_len(int content_len){
    return add_response("Content-Length: %d\r\n", content_len);
}

bool http_conn::add_linger(){
    return add_response("Connection: %s\r\n" , (m_linger == true)? "keep-alive" :"close"); 
}

bool http_conn::add_blank_line(){
    return add_response("%s" , "\r\n");
}

//the result of server handle the http request , will send to client
bool http_conn::process_write(HTTP_CODE ret){
    switch (ret){
        case INTERNAL_ERROR:
            {
                add_status_line(500 , error_500_title); 
                add_header(strlen(error_500_form));
                if (!add_content(error_500_form)){
                    return false;
                }
                break;
            }
        case BAD_REQUEST:
            {
                add_status_line(400 , error_400_title); 
                add_header(strlen(error_400_form));
                if (!add_content(error_400_form)){
                    return false;
                }
                break;
            }
        case NO_RESOURCE:
            {
                add_status_line(404 , error_404_title); 
                add_header(strlen(error_404_form));
                if (!add_content(error_404_form)){
                    return false;
                }
                break;
            }
        case FORBIDDEN_REQUEST:
            {
                add_status_line(403 , error_403_title); 
                add_header(strlen(error_403_form));
                if (!add_content(error_403_form)){
                    return false;
                }
                break;
            }
        case FILE_REQUEST:
            {
                add_status_line(200 , ok_200_title);
                if (m_file_stat.st_size != 0){
                    m_iv[0].iov_base = m_write_buf;
                    m_iv[0].iov_len = m_write_idx;
                    m_iv[1].iov_base = m_file_address;
                    m_iv[1].iov_len = m_file_stat.st_size;
                    m_iv_count = 2;
                    return true;
                }else {
                    const char* ok_string = "<html><body></body></html>";
                    add_header(strlen(ok_string));
                    if (!add_content(ok_string)){
                        return false;
                    }
                }
            }
        default:
            {
                return false;
            }
    }
    m_iv[0].iov_base = m_write_buf;
    m_iv[0].iov_len = m_write_idx;
    return true;
}




// the entrance function of http request , call by work thread
void http_conn::process(){
    HTTP_CODE read_ret = process_read();
    if (read_ret == NO_REQUEST){
        modfd(m_epollfd , m_socketfd , EPOLLIN);
        return ;
    }

    bool write_ret = process_write(read_ret);
    if (!write_ret){
        close_conn();
    }

    modfd(m_epollfd, m_socketfd , EPOLLOUT);
}
