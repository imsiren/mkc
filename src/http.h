/*
 * =====================================================================================
 *
 *       Filename:  http.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2016/10/10 09时54分14秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  吴帅 (腿哥), imsiren@163.com
 *   Organization:  
 *
 * =====================================================================================
 */

#ifndef _HTTP_H_
#define _HTTP_H_

#define BUFFER_SIZE 1024  

#define HTTP_POST "POST /%s HTTP/1.1\r\nHOST: %s\r\nAccept: */*\r\n"\
    "Accept-Encoding:gzip, deflate, sdch\r\n"\
    "User-Agent: kafka consumer\r\n"\
    "Content-Type:application/x-www-form-urlencoded\r\nContent-Length: %d\r\n\r\n%s" 

typedef struct http_response_t {

    int http_code;
}http_response_t;

http_response_t *http_client_post(char *url, const char *header,char *post_data,int len);

#endif
