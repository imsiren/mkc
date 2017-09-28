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
#include <ctype.h>
#include <errno.h>
#include <string.h>
#include <mysql.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <sys/types.h>

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

    //读取主进程ID
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
    //主进程发送信号
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
    mkc_write_log(MKC_LOG_NOTICE," ========== %d Exit .==========" , getpid());
    sleep(3);
    fclose(stdin);
    exit(0);
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
/*
        case SIGTERM:
            mkc_sigterm = 1;
            break;
*/
    }

    //通知子进程
    if(mkc_sigreload == 1){

        //发送reload
        mkc_signal_worker_process(SIGHUP);
    } 
    if(mkc_sigterm == 1){

        mkc_signal_worker_process(SIGKILL);
    }
    if(mkc_sigquit == 1){

        mkc_signal_worker_process(SIGQUIT);
    }
}

//创建子进程处理kafka数据
int mkc_spawn_worker_process(){

    int i, j;

    list_node *node;

    node = server_conf->topics->head;
    
    for(i = 0; i < server_conf->topics->len ; i++){

        mkc_topic *topic = 0;
        topic = (mkc_topic*)node->value;

        for(j = 0; j < topic->consumer_num ;j++){
            int exited = 0;
            pid_t pid = 0;
            pid = fork();

            switch(pid){
                case 0:
                    if(kafka_init_server() < 0){
                        exited = 1;
                    }
                    if(exited != 1){

                        mkc_do_worker_process(topic);
                    }

                    break;
                case -1:

                    mkc_write_log(MKC_LOG_ERROR,"fork child process error.");
                    break;
                default:

                    break;
            }
            server_conf->procs[i]->pid = pid;
            server_conf->procs[i]->exited = exited;
            server_conf->procs[i]->exiting = 0;
            //保存topic信息
            server_conf->procs[i]->topic = topic;
        }
        node = node->next;
    }
    return 0;
}

//当子进程异常退出时用它重启子进程
void mkc_restart_worker_process(int exited_pid){

    int i = 0;
    int pip[2];
    char buffer[128] = {0};
    if(pipe(pip) != 0){

        mkc_write_log(MKC_LOG_ERROR,"create pipe error :%s", strerror(errno));
        exit(1);
    }
    mkc_topic *topic = 0;
    for(i ; i < server_conf->topics->len; i++){

        if(server_conf->procs[i]->pid == exited_pid){

            topic = server_conf->procs[i]->topic;
            break;
        }
    }
    if(topic == 0){

        mkc_write_log(MKC_LOG_NOTICE ,"pid [%d] worker process exited and restart failed. ",exited_pid);
        return ;
    }
    mkc_write_log(MKC_LOG_NOTICE ,"worker process will restart by topic[%s]",topic->name);
    int pid = fork();
    int work_pid = 0;
    switch(pid){
        case 0:
            work_pid = getpid();
            close(pip[0]);
            sprintf(buffer,"%d",work_pid);
            write(pip[1],buffer, strlen(buffer));
            topic = server_conf->procs[i]->topic;
            mkc_do_worker_process(topic); 
            break;
        case -1:

            break;
    }

    //等待子进程 
    sleep(1);
    close(pip[1]);
    if(read(pip[0],buffer,128) < 0){

    }
    work_pid = atoi(buffer);
    server_conf->procs[i]->pid = work_pid;
}
//阻塞模式获取数据
void mkc_do_worker_process(mkc_topic * topic){

    kafka_init_server(topic);
    mkc_write_log(MKC_LOG_NOTICE, "mkc spawn work proces [%d] with topic[%s] .", getpid(),topic->name);
    setproctitle("mkc:%s [%s]", "worker process",topic->name);
    kafka_consume(topic);
}
/**
 * @brief 发信号通知所有子进程
 *
 * @param sig
 * */
void mkc_signal_worker_process(int sig){

    int i, j, ret;
    pid_t pid = 0;
    list_node *node;

    node = server_conf->topics->head;

    for(j = 0; j < server_conf->topics->len ; i++){

	    mkc_topic *topic = (mkc_topic*) node->value;

	    for(i = 0; i < topic->consumer_num ;i ++){
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
	    node = node->next;
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
void mkc_master_process_bury(){

    int err_code;
    int status, child ,live;
    pid_t pid;

    err_code = MKC_LOG_WARNING;
    while((pid = waitpid(-1,&status, WUNTRACED)) > 0){

        char buf[128];

        child = mkc_reap_children();

        if(WIFEXITED(status)){

            snprintf(buf,sizeof(buf), "pid [%d] exited with code %d",(int)pid ,WEXITSTATUS(status));

        }else if(WIFSIGNALED(status)){
            
            const char *have_core = WCOREDUMP(status) ? " - core dumped" :"";
            snprintf(buf,sizeof(buf), "pid [%d] on signal %d(%s)",(int)pid, WTERMSIG(status),have_core);
            mkc_restart_worker_process(pid);

        }else if(WIFSTOPPED(status)){

            snprintf(buf,sizeof(buf), "child %d stoped for tracing", (int)pid);
            err_code = MKC_LOG_NOTICE;
        }
        mkc_write_log(err_code,buf);

    }

    //wait for child exited
    sleep(5);
    
    char *mkc_binfile = "/usr/local/mkc/bin/mkc";
    //reload
    if(mkc_sigreload){
        mkc_sigreload = 0;
        mkc_write_log(MKC_LOG_NOTICE, "mkc will reload", (int)pid);
        mkc_write_log(MKC_LOG_NOTICE ,"reloading:execl(%s,\"%s\",\"%s\",\"%s\")", mkc_binfile, "mkc", mkc_os_argv[1],mkc_os_argv[2]);
        if(execl(mkc_binfile,"mkc", mkc_os_argv[1],mkc_os_argv[2],(char*)0 ) < 0){

            mkc_write_log(MKC_LOG_ERROR,"call execl errno[%d] error [%s]",errno,strerror(errno));
        }
    }
    mkc_write_log(MKC_LOG_NOTICE, "bye bye ...");

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

            //发送reload
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

