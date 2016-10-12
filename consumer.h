/*
 * =====================================================================================
 *
 *       Filename:  consumer.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2016/10/10 16时30分29秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  吴帅 (腿哥), imsiren@163.com
 *   Organization:  
 *
 * =====================================================================================
 */
#ifndef __CONSUMER_H_
#define __CONSUMER_H_
#include <unistd.h>
#include <time.h>
#include <stdarg.h>
#include <sys/time.h>
#include "config.h"

#define MAX_LOGMSG_LEN 1024
#define LOG_VERBOSE 3
#define LOG_ERROR 2
//#define LOG_WARNING 1

extern server_conf_t server_config;

void mmqLogRaw(int level,const char *msg);

void mmqLog(int level,const char *fmt,...);
#endif
