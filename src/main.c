/*
 * =====================================================================================
 *
 *       Filename:  main.c
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2016/10/09 16时07分59秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  吴帅 (腿哥), imsiren@163.com
 *   Organization:  
 *
 * =====================================================================================
 */
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <syslog.h>
#include <stdio.h>
#include <signal.h>
#include <ctype.h>

#include "mkc.h"
#include "cJSON.h"
#include "http.h"
#include "config.h"
#include "sds.h"
#include "logger.h"
#include "process.h"

#include <librdkafka/rdkafka.h>  /* for Kafka driver */

#include <jansson.h>

#define SERVER_COMMAND_NUM 200

#define BROKER_PATH "/brokers/ids"



server_conf_t server_config;

static void init_server_conf(){

    server_config.brokers = sdsnew("");
    server_config.daemonize = 1;
    server_config.pidfile = "./mkc.pid";
    server_config.loglevel = 1 ;//warning
    server_config.logfile = "./logs/mkc.log";
    server_config.confpath = "./conf";

    server_config.timeout = 100;
    //server_config.cmd_t = hash_init(SERVER_COMMAND_NUM);

    server_config.topics = list_create();
    server_config.commands = list_create();
    //server_config.modules = zmalloc(sizeof(list) * SERVER_COMMAND_NUM);
    server_config.modules = hash_init(SERVER_COMMAND_NUM);
    server_config.mkc_run = 1;
}

static void logger(const rd_kafka_t *rk,int level, const char *fac, const char *buf){

    fprintf(stderr,"%s\n",buf);
}

static void usage(){
    fprintf(stderr,
            "Usage: bin [options] <topic[:part]> <topic[:part]>..\n"
            "\n"
            "librdkafka version %s (0x%08x)\n"
            "\n"
            " Options:\n"
            "  -c <>      config file\n"
            "\n",
            rd_kafka_version_str(), rd_kafka_version()
           );

}

static int write_pid(){

    FILE *fp = NULL;

    char pid_file[1024] = {0};

    pid_t pid = getpid();

    sprintf(pid_file,"%s/%d",server_config.pidpath,pid);

    fp = fopen(pid_file ,"w+");

    if(fp == NULL){

        mkc_write_log(MKC_LOG_ERROR,"%s [%s]",strerror(errno),pid_file);
        return -1;
    }
    char pidstr[128] = {0};

    sprintf(pidstr,"%d",pid);

    fputs(pidstr,fp);

    fclose(fp);
    return 0;
}

static int mkc_save_argv(int argc, char *const *argv){

    mkc_argc = argc;
    mkc_argv = zmalloc(sizeof(char) * (argc +1));

    int i ,len;
    for(i = 0; i < argc;i++){

        len = strlen(argv[i]) + 1;
        mkc_argv[i] = zmalloc(len);
        if(mkc_argv[i] == NULL){

            mkc_write_log(MKC_LOG_ERROR,"zmalloc() error");
            return 1;
        }
        memcpy(mkc_argv[i] ,(char *)argv[i], len);
    }

    mkc_argv[i] = NULL;
    return 0;
}

int main(int argc, char **argv){


    server_config.argc = argc;
    mkc_os_argv = argv;
    init_server_conf();

    int opt;

    while((opt = getopt(argc, argv,"c:u:b:g:v:d:DO")) != -1){

        switch(opt){
            case 'c':
                server_config.conffile = optarg;
                break;

            case 'd': //daemon

                server_config.daemon = 1;
                break;
            default:
                usage();
                exit(1);
                break;
        }
    }


    if(!server_config.conffile ){

        usage();
        exit(1);
    }else{
        fprintf(stderr,"load conf file:%s\n",server_config.conffile);
    }

    if(parse_server_conf(server_config.conffile) == -1){
        usage();
        exit(1);
    }
    if(mkc_save_argv(argc, argv) != 0){

        return 1;
    }

    if(mkc_init_setproctitle(environ) != 0){

        return 1;
    }

    server_config.procs = zmalloc(sizeof(mkc_process_t) * server_config.topics->len);
    int i ;
    for(i = 0;i < server_config.topics->len; i ++){

        server_config.procs[i] =  zmalloc(sizeof(mkc_process_t));
        memset(server_config.procs[i],0,sizeof(mkc_process_t));
    }
    mkc_setproctitle( "mkc:master process");

    //创建多进程
    mkc_spawn_worker_process();

    mkc_master_process();

    exit(0);
    /*   后续加入守护进程。 */
}


