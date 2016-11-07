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

#include "zmalloc.h"
#include "logger.h"
#include "sds.h"

#define MKC_LOG_BUFFER_SIZE 1024

int mkc_write_log(int log_level, const char *format,...){

    time_t now;

    time(&now);

    struct tm *tm_now;

    tm_now = localtime(&now);

    char date_time[128] = {0};

    sds log_buffer = sdsnewlen("", MKC_LOG_BUFFER_SIZE);

    sprintf(date_time,"%d-%d-%d %d:%d:%d\t",tm_now->tm_year + 1900 ,tm_now->tm_mon + 1,tm_now->tm_mday,tm_now->tm_hour,tm_now->tm_min,tm_now->tm_sec);

    sds log = sdsnew(date_time);

    switch(log_level){
        case MKC_LOG_WARNING:

            log = sdscatlen(log,("E_WARNING\t"), strlen("E_WARNING\t"));
            break;
        case MKC_LOG_NOTICE:

            log = sdscatlen(log,("E_NOTICE\t"), strlen("E_NOTICE\t"));
            break;
        case MKC_LOG_ERROR:

            log = sdscatlen(log,("E_WARNING\t"), strlen("E_WARNING\t"));
            break;
            /*  
        case MKC_LOG_DEBUG:

            log = sdscatlen(log,("E_DEBUG\t"), strlen("E_DEBUG\t"));
            break;
            */
        default:

            log = sdscatlen(log,("E_WARNING\t"), strlen("E_WARNING\t"));
            break;
    }

    
    int ret = 0;

    va_list ap;

    va_start(ap, format);

    vsprintf(log_buffer ,format,ap);

    va_end(ap);

    sds output_log = sdscatlen(log,log_buffer,strlen(log_buffer));

//    output_log = sdscatlen(output_log, "\n",1);

    FILE *log_fp = fopen(server_config.logfile,"a+");

    if(log_fp){// &&  (server_config.loglevel & log_level)){

        fputs(output_log,log_fp);
    }else{

        fprintf(stderr,"%s\n", output_log);
    }

    sdsfree(output_log);
    sdsfree(log_buffer);
    //sdsfree(log);
    fclose(log_fp);
    return 0;
}
