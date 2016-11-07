/*
 * =====================================================================================
 *
 *       Filename:  logger.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2016/10/26 10时39分44秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  吴帅 (腿哥), imsiren@163.com
 *   Organization:  
 *
 * =====================================================================================
 */

#ifndef __LOGGER_H_
#define __LOGGER_H_

#define MKC_LOG_ERROR   1  // 01
#define MKC_LOG_NOTICE  2  // 10
#define MKC_LOG_WARNING 3  // 11

#include "config.h"

extern server_conf_t server_config;

int mkc_write_log(int log_level,const char *format,...);


#endif
