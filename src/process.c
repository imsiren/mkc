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
#include <mysql.h>

#include "kafka.h"
#include "config.h"
#include "process.h"
#include "logger.h"
#include "kafka.h"
#include "mkc.h"

extern server_conf_t *server_conf;

int mkc_signal_process(char *sig){

    fprintf(stderr,"mkc signal process start\n");
    char pid_file[1024] = {0}; 

    sprintf(pid_file,"%s/%s",server_conf->pidpath,server_conf->pidfile);
    
    FILE *fp = fopen(pid_file,"r");

    if(!fp){

        mkc_write_log(MKC_LOG_ERROR,"read pid file error [%s], error[%s]",pid_file,strerror(errno));
        return 0;
    }

    mkc_write_log(MKC_LOG_NOTICE,"read pid file [%s].",pid_file);

    char buf[1024] = {0};
    fread(buf,1024,1,fp);
    if(fclose(fp)){

        mkc_write_log(MKC_LOG_ERROR,"close pid file error [%s], error[%s]",pid_file,strerror(errno));
        return 1;
    }

    pid_t pid = atoi(buf);
    if(!pid){

        mkc_write_log(MKC_LOG_ERROR,"invalid PID num \"%s\" in \"%s\".",pid,pid_file);
        return 1;
    }
    int signo = 0;
    if(!strcmp("stop",sig)){

        signo = SIGQUIT;
    }else if(!strcmp("reload",sig)){

        signo = SIGHUP;
    }else{

        mkc_write_log(MKC_LOG_ERROR,"invalid signo. ");
        return -1;
    }
    if(kill(pid,signo) != 0){

        mkc_write_log(MKC_LOG_ERROR,"kill pid \"%d\" signo \"%s\" error[%s].",pid,sig,strerror(errno));
        return -1;
    }
    //删除pid文件

    if(unlink(server_conf->pidfile) != 0){

        return 1;
    }
    return 0;
}
void mkc_set_worker_process_handler(){

    signal(SIGQUIT, mkc_worker_process_handler);
    signal(SIGHUP,  mkc_worker_process_handler);
}

//子进程信号处理程序
void mkc_worker_process_handler(int signo){

    mkc_mysql_close(&server_conf->mkc_mysql_pconnect);
    
    kafka_consume_close();

    if(!run){
        //restart
        /*if(signo == SIGHUP){

            mkc_write_log(MKC_LOG_NOTICE ,"reloading :execvp(%s,{\"%s\",\"%s\",\"%s\"})",mkc_os_argv[0], mkc_os_argv[0],mkc_os_argv[1],mkc_os_argv[2]);
            execvp(mkc_os_argv[0], mkc_os_argv);
        }
*/

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

    node = server_conf->topics->head;

    for(i = 0; i < server_conf->topics->len ; i++){
        pid_t pid = 0;
        pid = fork();
        
        mkc_topic *topic = 0;
        switch(pid){
            case 0:

                topic = (mkc_topic*)node->value;

                kafka_init_server(topic);

                mkc_write_log(MKC_LOG_NOTICE, "mkc spawn work proces [%d] with topic[%s] .", getpid(),topic->name);

                setproctitle("mkc:%s [%s]", "worker process",topic->name);
                kafka_consume(topic);

                break;
            case -1:

                mkc_write_log(MKC_LOG_ERROR,"fork child process error.");
                break;
            default:

                break;
        }
        server_conf->procs[i]->pid = pid;
        server_conf->procs[i]->exited = 0;
        server_conf->procs[i]->exiting = 0;
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
    for(i = 0; i < server_conf->topics->len ; i++){

        if(server_conf->procs[i]->exited == 0){

            pid = server_conf->procs[i]->pid;
            server_conf->procs[i]->exited = 0;
            server_conf->procs[i]->exiting = 1;
            mkc_write_log(MKC_LOG_NOTICE,"mkc will close pid [%d] ." ,pid);
            if(kill(pid,sig) == 0){

                server_conf->procs[i]->exited = 1;
                server_conf->procs[i]->exiting = 0;
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

    for(i = 0; i < server_conf->topics->len ;i++){

        //正在退出的进程也算活跃进程
        if(server_conf->procs[i]->exited == 0){

            live ++;
        }
    }
    return live;
}

void mkc_pctl_execv(){

    mkc_write_log(MKC_LOG_NOTICE , "reloading:execvp(\"%s\",\"%s\",\"%s\")",
        mkc_os_argv[0],
        mkc_os_argv[1],
        mkc_os_argv[2]
    );

    execvp(mkc_os_argv[0], mkc_os_argv);
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

            if(mkc_sigreload){
                mkc_sigreload = 0;
                mkc_pctl_execv();
            }
            mkc_master_process_exit();
        }

        sigsuspend(&set);

        if(mkc_sigreload == 1){

            mkc_signal_worker_process(SIGHUP);
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
char *mkc_cpystrn(char *dst,const char *src, size_t n)
{
    if (n == 0) {
        return dst;
    }

    while (--n) {
        *dst = *src;

        if (*dst == '\0') {
            return dst;
        }

        dst++;
        src++;
    }

    *dst = '\0';

    return dst;
}

/*
int mkc_init_setproctitle(char **envp){

    int i;
    int size = 0;
    char *p = 0;
    for(i = 0;envp[i] != NULL;i++){

        size += strlen(envp[i]);
        continue;
    }

    mkc_os_argv_last = mkc_os_argv[0];

    for(i = 0;i < server_conf->argc; i++){
           if(mkc_os_argv_last == mkc_os_argv[i]){
                mkc_os_argv_last = mkc_os_argv[i] + strlen(mkc_os_argv[i]) + 1;
           }
           
    }
    p = zmalloc(size);
    if(!p){
        mkc_write_log(MKC_LOG_ERROR,"zmalloc() error");
        return 1;
    }
    for(i = 0;envp[i] != NULL;i++){
        if(mkc_os_argv_last = envp[i]){
            p = strcpy(p,envp[i]);
            environ[i] = p;
        }
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
*/

void mkc_setproctitle(const char *title){
/*
    char *p = NULL;

    mkc_os_argv[1] = NULL;

    p = mkc_cpystrn(mkc_os_argv[0], "mkc: ",mkc_os_argv_last - mkc_os_argv[0]);
    p = mkc_cpystrn(p , title,mkc_os_argv_last - (char*)p);
    int i,size;

    for(i = 0;i < server_conf->argc;i++){

        p = mkc_cpystrn(p , mkc_os_argv[i], mkc_os_argv_last - (char *)p); 
        p = mkc_cpystrn(p , " ", mkc_os_argv_last - (char*)p);
    }
    if(mkc_os_argv_last - (char*)p){

        memset( p ,0, mkc_os_argv_last - (char*)p);
    }
*/
}
