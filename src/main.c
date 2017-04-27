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
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

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


char *mkc_signal ;
int mkc_process;

server_conf_t server_config;

static int mkc_daemon(){

    int fd;
    
    switch(fork()){
        case -1 :

            break;
        case 0:

            break;
        default:
            exit(0);
    }

    int pid = getpid();

    if(setsid() == -1){

        mkc_write_log(MKC_LOG_ERROR,"setsid() error.");
        return 1;
    }
    umask(0);

    fd = open("/dev/null",O_RDWR);

    if(fd == -1){

        mkc_write_log(MKC_LOG_ERROR,"open() error.");
        return 1;
    }
    if(dup2(fd,STDIN_FILENO) == -1){

        mkc_write_log(MKC_LOG_ERROR,"dup2(stdin) error.");
        return 1;
    }
    if(dup2(fd,STDOUT_FILENO) == -1){

        mkc_write_log(MKC_LOG_ERROR,"dup2(stdout) error.");
        return 1;
    }

    if(fd > STDERR_FILENO){

        mkc_write_log(MKC_LOG_ERROR,"dup2() error %d > %d. ",fd, STDERR_FILENO);

        if(close(fd) == -1){

            return 1;
        }
    }

    return 0;
}

static void init_server_conf(){

    server_config.brokers = sdsnew("");
    server_config.daemonize = 1;
    server_config.pidfile = "./logs/mkc.pid";
    server_config.loglevel = 1 ;//warning
    server_config.logfile = "./logs/mkc.log";
    server_config.confpath = "./conf";
    server_config.conffile = "conf/server.conf";

    server_config.timeout = 100;
    //server_config.cmd_t = hash_init(SERVER_COMMAND_NUM);

    server_config.topics = list_create();
    server_config.commands = list_create();
    //server_config.modules = zmalloc(sizeof(list) * SERVER_COMMAND_NUM);
    server_config.modules = hash_init(SERVER_COMMAND_NUM);
    server_config.mkc_run = 1;
    server_config.fallback = NULL;
    server_config.groupid = NULL;
}

static void logger(const rd_kafka_t *rk,int level, const char *fac, const char *buf){

    fprintf(stderr,"%s\n",buf);
}

static void usage(){
    fprintf(stderr,
            "Usage: bin [options] \n"
            "\n"
            "librdkafka version %s (0x%08x)\n"
            "\n"
            " Options:\n"
            "  -c <>      config file\n"
            "  -s stop|reload"
            "\n",
            rd_kafka_version_str(), rd_kafka_version()
           );

}

static int mkc_create_pid(){

    FILE *fp = NULL;

    char pid_file[1024] = {0};

    pid_t pid = getpid();

    sprintf(pid_file,"%s/%s",server_config.pidpath,server_config.pidfile);

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

    while((opt = getopt(argc, argv,"c:ds:")) != -1){

        switch(opt){
            case 'c':
                server_config.conffile = optarg;
                break;

            case 'd': //daemon

                server_config.daemon = 1;
                break;
            case 's':
                if(!strcmp("reload",optarg)
                 || !strcmp("stop",optarg)){

                    mkc_signal = optarg;
                }
                break;
            default:
                exit(1);
        }
    }

    if(!server_config.conffile ){

        usage();
        exit(1);
    }
    fprintf(stderr,"load conf file:%s\n",server_config.conffile);

    if(parse_server_conf(server_config.conffile) == -1){
        usage();
        exit(1);
    }
    fprintf(stderr,"logfile :%s\n",server_config.logfile);
/*
    if(mkc_save_argv(argc, argv) != 0){

        return 1;
    }
*/

    spt_init(argc,argv);
    server_config.procs = zmalloc(sizeof(mkc_process_t) * server_config.topics->len);
    int i ;
    for(i = 0;i < server_config.topics->len; i ++){

        server_config.procs[i] =  zmalloc(sizeof(mkc_process_t));
        memset(server_config.procs[i],0,sizeof(mkc_process_t));
    }

    //服务以信号模式启动
    if(mkc_signal){

        mkc_signal_process(mkc_signal);
        exit(0);
     }

    if(server_config.daemonize == 1 && mkc_daemon() != 0){

        fprintf(stderr,"daemon() error.");
        exit(1);
    }

    setproctitle("mkc:%s","master process");
    //创建多进程
    mkc_spawn_worker_process();

    mkc_create_pid();

    mkc_master_process();

    exit(0);
}


