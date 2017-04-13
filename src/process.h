/*
 * =====================================================================================
 *
 *       Filename:  process.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2017/04/05 13时19分17秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  吴帅 (腿哥), imsiren@163.com
 *   Organization:  
 *
 * =====================================================================================
 */
#ifndef _PROCESS_H
#define _PROCESS_H
#include <stdio.h>

extern char **environ;
static char **os_argv;
static char *os_last_argv;

typedef struct mkc_process_t{

    pid_t pid;
    int exited;
    int exiting;

} mkc_process_t;


int mkc_sigalrm;
int mkc_sigterm;
int mkc_sigquit;
int mkc_sigreload;
int mkc_sigint;
int mkc_sigchld;

static int run = 0;

void mkc_process_daemon();

int mkc_init_setproctitle(char **envp);

void mkc_setproctitle(const char *title);

void mkc_master_process();

void mkc_worker_process();

int mkc_spawn_worker_process();

void mkc_set_worker_process_handler();

void mkc_worker_process_handler();

void mkc_signal_worker_process(int sig);

void mkc_signal_master_process(int sig);

void mkc_master_handler(int sig);

void mkc_master_process_exit();

#endif
