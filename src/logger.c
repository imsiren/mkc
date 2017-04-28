/*
 * =====================================================================================
 *
 *       Filename:  logger.c
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2016/10/26 10时42分47秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  吴帅 (腿哥), imsiren@163.com
 *   Organization:  
 *
 * =====================================================================================
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>

#include "zmalloc.h"
#include "logger.h"
#include "sds.h"

#define MKC_LOG_BUFFER_SIZE 1024 * 1024

int mkc_write_log(int log_level, const char *format,...){

    time_t now;

    time(&now);

    struct tm *tm_now;

    tm_now = localtime(&now);

    char date_time[128] = {0};

    //sds log_buffer = sdsnewlen("", MKC_LOG_BUFFER_SIZE);

    sprintf(date_time,"%d-%d-%d %d:%d:%d [%d]\t",tm_now->tm_year + 1900 ,tm_now->tm_mon + 1,tm_now->tm_mday,tm_now->tm_hour,tm_now->tm_min,tm_now->tm_sec,getpid());

    sds log = sdsnew(date_time);

    int tm_len = sdslen(log);
    switch(log_level){
        case MKC_LOG_WARNING:

            log = sdscat(log,"E_WARNING\t");
            break;
        case MKC_LOG_NOTICE:

            log = sdscat(log,"E_NOTICE\t");
            break;
        case MKC_LOG_ERROR:

            log = sdscat(log,"E_WARNING\t");
            break;
            /*  
        case MKC_LOG_DEBUG:

            log = sdscatlen(log,("E_DEBUG\t"), strlen("E_DEBUG\t"));
            break;
            */
        default:

            log = sdscat(log,"E_WARNING\t");
            break;
    }

    
    int ret = 0;

    char *buffer = zmalloc(MKC_LOG_BUFFER_SIZE);
    va_list ap;

    va_start(ap, format);

    vsprintf(buffer,format,ap);

    va_end(ap);

    log = sdscat(log,buffer);
    log = sdscat(log,"\n");

    FILE *log_fp = fopen(server_config.logfile,"a+");
//    FILE *log_fp = server_config.logfp;

    if(log_fp){// &&  (server_config.loglevel & log_level)){

        int ret = fputs(log,log_fp);

        if(ret == EOF){

            fprintf(stderr,"mkc error:%s %s\n",server_config.logfile,strerror(errno));
        }
        fclose(log_fp);
    }else{

        fprintf(stderr,"open file [%s] error with [%s]\n", server_config.logfile,strerror(errno));
        fprintf(stderr,"%s\n", log);
    }

    zfree(buffer);
    sdsfree(log);
    return 0;
}
