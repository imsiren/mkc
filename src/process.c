/*
 * =====================================================================================
 *
 *       Filename:  process.c
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2017/04/10 21时59分55秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  吴帅 (腿哥), imsiren@163.com
 *   Organization:  
 *
 * =====================================================================================
 */
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include "kafka.h"
#include "config.h"
#include "process.h"
#include "logger.h"
#include "kafka.h"
#include "mkc.h"

void mkc_set_worker_process_handler(){

    signal(SIGQUIT, mkc_worker_process_handler);
    signal(SIGHUP,  mkc_worker_process_handler);
}

//子进程信号处理程序
void mkc_worker_process_handler(){

    kafka_consume_close();

    if(!run){
        exit(1);
    }

    run = 0;

    fclose(stdin);
}

void mkc_signal_handler(int sig){

    switch(sig){
        case SIGHUP: //reload

            mkc_sigreload = 1;
            break;
        case SIGINT:
        case SIGKILL:
        case SIGQUIT:
            mkc_sigquit = 1;
            break;

        case SIGCHLD:
            mkc_sigchld = 1;
            break;
        case SIGALRM:
            mkc_sigalrm = 1;
            break;
        case SIGTERM:
            mkc_sigterm = 1;
            break;
    }
}

//创建子进程处理kafka数据
int mkc_spawn_worker_process(){
    int i;
    list_node *node;

    node = server_config.topics->head;

    for(i = 0; i < server_config.topics->len ; i++){
        pid_t pid = 0;
        pid = fork();
        
        mkc_topic *topic = 0;
        switch(pid){
            case 0:
                mkc_setproctitle("mkc: worker process");
                kafka_init_server();

                topic = (mkc_topic*)node->value;

                mkc_write_log(MKC_LOG_NOTICE, "mkc spawn work proces [%d] with topic[%s] .", getpid(),topic->name);

                kafka_consume(topic);

                break;
            case -1:

                mkc_write_log(MKC_LOG_ERROR,"fork child process error.");
                break;
            default:

                break;
        }
        server_config.procs[i]->pid = pid;
        server_config.procs[i]->exited = 0;
        server_config.procs[i]->exiting = 0;
        node = node->next;
    }
    return 0;
}
/**
 * @brief 发信号通知所有子进程
 *
 * @param sig
 * */
void mkc_signal_worker_process(int sig){

    int i, ret;
    pid_t pid = 0;
    for(i = 0; i < server_config.topics->len ; i++){

        if(server_config.procs[i]->exited == 0){

            pid = server_config.procs[i]->pid;
            server_config.procs[i]->exited = 0;
            server_config.procs[i]->exiting = 1;
            mkc_write_log(MKC_LOG_NOTICE,"mkc will close pid [%d] ." ,pid);
            if(kill(pid,sig) == 0){

                server_config.procs[i]->exited = 1;
                server_config.procs[i]->exiting = 0;
                continue;
            }
            mkc_write_log(MKC_LOG_NOTICE, "mkc stoped worker process [%d] error, errno[%d],error [%s]", pid, errno,strerror(errno));
        }
    }
}
void mkc_master_process_exit(){

    //waitpid(-1,NULL,0);
    exit(0);
}

void mkc_init_signal(){
    struct sigaction sa;

    sigemptyset(&sa.sa_mask);

    sa.sa_flags = 0;
    sa.sa_handler = mkc_signal_handler;

    sigaction(SIGINT,&sa,NULL);
    sigaction(SIGCHLD,&sa,NULL);
    sigaction(SIGALRM,&sa,NULL);
    sigaction(SIGQUIT,&sa,NULL);
    sigaction(SIGHUP,&sa,NULL);
}

//统计活跃的进程个数
int mkc_reap_children(){

    int i ;
    int live = 0;

    for(i = 0; i < server_config.topics->len ;i++){

        //正在退出的进程也算活跃进程
        if(server_config.procs[i]->exited == 0){

            live ++;
        }
    }
    return live;
}
void mkc_master_process(){


    mkc_init_signal();

    sigset_t set;
    sigemptyset(&set);
    //sigaddset(&set,SIGINT);
    sigaddset(&set,SIGCHLD);
    sigaddset(&set,SIGALRM);

    sigaddset(&set,SIGQUIT);
    //reload
    sigaddset(&set,SIGHUP);

    if(sigprocmask(SIG_BLOCK,&set,NULL) == -1){

        mkc_write_log(MKC_LOG_ERROR,"sigprocmask() error.");
    }

    sigemptyset(&set);

    int live = 0;
    while(1){


        live = mkc_reap_children();

        if(!live && (mkc_sigterm || mkc_sigquit)){

            //wait worker exited.
            sleep(3);

            mkc_master_process_exit();
        }

        sigsuspend(&set);

        if(mkc_sigreload == 1){

            //mkc_signal_worker_process(SIGHUP);
            continue;
        }
        if(mkc_sigterm == 1){

            mkc_signal_worker_process(SIGKILL);
            continue;
        }
        if(mkc_sigquit == 1){

            mkc_signal_worker_process(SIGQUIT);
            continue;
        }
    }
}
int mkc_init_setproctitle(char **envp){

    int i;
    for(i = 0;envp[i] != NULL;i++){

        continue;
    }
    environ = zmalloc(sizeof(char*) + (i + 1));
    if(!environ){

        mkc_write_log(MKC_LOG_ERROR,"zmalloc() error");
        return 1;
    }

    for(i = 0;envp[i] != NULL;i++){

        environ[i] = zmalloc(sizeof(char) + strlen(envp[i]));

        strcpy(environ[i], envp[i]);

    }
    environ[i] = NULL;
    return 0;
}

void mkc_setproctitle(const char *title){
    char *tmp = NULL;

    int len = strlen(mkc_os_argv[0]);

    tmp = mkc_os_argv[0];

    memset(tmp,0,len);

    mkc_os_argv[1] = NULL;

    strncpy(tmp,title,len);
}
