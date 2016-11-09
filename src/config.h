/*
 * =====================================================================================
 *
 *       Filename:  config.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2016/05/20 15时46分25秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  YOUR NAME (), 
 *   Organization:  
 *
 * =====================================================================================
 */

#ifndef __CONF_H_
#define __CONF_H_


#include "sds.h"
#include "hash.h"

typedef struct {

    char *name;

    char *uri;
    char *method;
    int delay;  //延迟时间
    int retrynum; // 0 失败一直重试，值到uri的code为200.
    sds *commands;
    int command_len;
}module_conf_t ;

typedef struct {
    int command_no; // 命令号
    module_conf_t **module_conf;
}module_cmd_t ;

typedef struct mkc_topic {

    sds name;
    int partition;
    int64_t offset;
}mkc_topic;

typedef struct server_conf_t {

    int daemonize;

    int timeout;

    int loglevel;

    int daemon;

    int zookeeper_debug;

    int verbose;

    sds brokers;

    sds zookeeper;

    char *group;

    list *topics;

    char *domain;


    sds confpath;

    char *conffile;

    sds pidpath;

    char *port;

    char *log_path;

    char *logfile;

    //日志句柄
    FILE *logfp;

    char *pidfile;

    int sockfd;

    int process_num;

    int pipe_fd[2];

    list *commands;
    hash_table *modules;
    //hash_table *cmd_t;
    //module_conf_t **command_module_map;

}server_conf_t ;

int parse_server_conf(char *filename);

module_conf_t *parse_module_conf(const char *file);

#endif
