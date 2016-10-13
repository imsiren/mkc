/*
 * =====================================================================================
 *
 *       Filename:  consumer.c
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2016/10/10 17时29分24秒
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
#include "consumer.h"
#include "config.h"

void mmqLogRaw(int level,const char *msg){

    FILE *fp    =   fopen(server_config.logfile,"a");
    if(!fp){

        fprintf(stderr,"%s\n",msg);
        exit(-1);
    }

    struct timeval tv;

    gettimeofday(&tv,NULL);

    char buf[64];

    int off = strftime(buf,sizeof(buf),"%d %b %H:%M:%S.",localtime(&tv.tv_sec));

    fprintf(fp,"%d:%s %c %s\n",(int)getpid(),buf,level,msg);

    fflush(fp);

    fclose(fp);
    if(server_config.loglevel == LOG_VERBOSE){

        fprintf(stderr,"%d:%s %c %s\n",(int)getpid(),buf,level,msg);
    }
}

void mmqLog(int level,const char *fmt,...){
    va_list ap;
    char msg[MAX_LOGMSG_LEN] = {0};
    va_start(ap,fmt);
    vsnprintf(msg,sizeof(msg),fmt,ap);
    va_end(ap);
    mmqLogRaw(level,msg);
}

