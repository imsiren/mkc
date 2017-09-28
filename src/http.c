/*
 * =====================================================================================
 *
 *       Filename:  http.c
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2016/10/10 09时58分34秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  吴帅 (腿哥), imsiren@163.com
 *   Organization:  
 *
 * =====================================================================================
 */
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <syslog.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <errno.h>
#include "http.h"
#include "zmalloc.h"
#include "logger.h"

int http_client_create(const char *host,int port, int timeout){

    struct hostent *he;
    struct sockaddr_in server_addr;
    int socket_fd;

    if((he = gethostbyname(host)) == NULL){

        mkc_write_log(MKC_LOG_WARNING, "get host [%s:%d] byname error:%d %s\n",host,port,errno,strerror(errno));
        return -1;
    }
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr = *((struct in_addr *)he->h_addr);

    if((socket_fd = socket(AF_INET,SOCK_STREAM,0)) == -1){

        mkc_write_log(MKC_LOG_WARNING, "create socket error:%d %s\n",errno,strerror(errno));
        return -1;
    }

    struct timeval netTimeout = {3, 0}; //1s
    
    setsockopt(socket_fd, SOL_SOCKET, SO_SNDTIMEO, (char*)&netTimeout, sizeof(int));

    setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, (char*)&netTimeout, sizeof(int));

    if(connect(socket_fd,(struct sockaddr*)&server_addr,sizeof(struct sockaddr)) == -1){

        mkc_write_log(MKC_LOG_WARNING, "connect error:%d %s\n",errno,strerror(errno));
        return -1;
    }
    return socket_fd;
}


/**
 * @brief 发送数据
 *
 * @param socket_fd
 * @param data 发送的数据
 * @param size 发送的数据大小
 *
 * @return 
 * */
static int http_client_send(int socket_fd, char *data, int size){

    int send_num = 0 , ret_num = 0;

    while(send_num < size){

        ret_num = send(socket_fd, data + send_num, size - send_num,0);
        if(ret_num == -1){

            mkc_write_log(MKC_LOG_ERROR,"send error %d %s",errno,strerror(errno));
            return -1;
        }
        send_num += ret_num;
    }
    return send_num;
}

static int http_client_recv(int socket_fd, char *buff, int tvout){

    fd_set fds;
    struct timeval timeout = {tvout,0};

    FD_ZERO(&fds);
    FD_SET(socket_fd, &fds);

    int err = select(socket_fd + 1, &fds, NULL, NULL ,&timeout);

    int recv_num = 0;
    switch(err){
        case -1:
            mkc_write_log(MKC_LOG_WARNING, "select error %d [%s]", errno, strerror(errno));
            break;
        case 0:

            mkc_write_log(MKC_LOG_WARNING, "select timeout ");
            break;
        default:
            if(FD_ISSET(socket_fd, &fds)){
                //这里只接收一个BUFFER_SIZE长度的数据，因为只需要解析header信息
                recv_num = recv(socket_fd, buff, BUFFER_SIZE, 0);
            }
    }
    if(recv_num <= 0){

        //mkc_write_log(MKC_LOG_ERROR,"recv %d %d %s.",recv_num,errno,strerror(errno));
        return -1;
    }
    return recv_num;
}

static http_response_t *http_client_parse_result(const char *result){

    char *p = NULL;

    http_response_t *response = zmalloc(sizeof(http_response_t));

    if(!response){

        mkc_write_log(MKC_LOG_WARNING,"zmalloc() error");
        return NULL;
    }

    if((p = strstr(result,"HTTP/1.1"))){

        response->http_code = atoi(p + 9);
    }
    return response;
}

static int http_client_parse_file(const char *url,char *file,char *host){

    char *p = (char*)url;
    if(!strncmp(url,"http://",7)){
        p += 7;
    }else{

        return -1;
    }
    char *p2 = strstr(p,"/");

    if(p2){
        int len = strlen(p) - strlen(p2);

        memcpy(host,p,len);
        host[len] = '\0';
        if(len > 0){

            memcpy(file,p2 + 1, strlen(p2) - 1);
            file[strlen(p2) - 1] = '\0';
        }

    }else{

        memcpy(host,p,strlen(p));
    }

    mkc_write_log(MKC_LOG_NOTICE, "host [%s] file [%s]",host,file);
    return 0;
}

void http_client_closed(int socket_fd){

    close(socket_fd);
}

http_response_t *http_client_post(char *url,const char *header,char *post_data, int post_len, int timeout){

    char file[512] = {0};
    char host[512] = {0};

    http_client_parse_file(url ,file,host);
    http_response_t *response = NULL;
    int _port = 80;

    int socket_fd = http_client_create(host, _port, timeout);
    if(socket_fd <= 0){

        mkc_write_log(MKC_LOG_ERROR,"client create error %d %s",errno,strerror(errno));
        return NULL;
    }

    int size = post_len * 1024;
    char *post_buf = zmalloc(size);
    if(!post_buf){

        mkc_write_log(MKC_LOG_ERROR,"zmalloc() error.");
        return NULL;
    }

    memset(post_buf,0,size);

    sprintf(post_buf,HTTP_POST,file,host,post_len,post_data);

    //mkc_write_log(MKC_LOG_NOTICE,"post data %s\n",post_buf);
    if((http_client_send(socket_fd, post_buf,strlen(post_buf)) <= 0)){

        mkc_write_log(MKC_LOG_ERROR,"http_client_send error");
        zfree(post_buf);
        return NULL;
    }
    post_buf = 0;

    char recv_buffer[1024] = {0};

    if(http_client_recv(socket_fd,recv_buffer, timeout) < 0){

        mkc_write_log(MKC_LOG_NOTICE,"recv failed:url[%s]",url);
        goto done;
    }

    response = http_client_parse_result(recv_buffer);

    mkc_write_log(MKC_LOG_NOTICE,"recv success:url[%s] code[%d]",url,response->http_code);

done:
    if(socket_fd){
        http_client_closed(socket_fd);
    }
    zfree(post_buf);
    //printf("done\n");
    return response;
}

        
