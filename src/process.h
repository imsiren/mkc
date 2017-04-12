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

typedef struct mkc_process_t{

    pid_t pid;
    int exited;
    int exiting;

} mkc_process_t;


sig_atomic_t mkc_sigalrm;
sig_atomic_t mkc_sigterm;
sig_atomic_t mkc_sigquit;
sig_atomic_t mkc_sigreload;
sig_atomic_t mkc_sigint;
sig_atomic_t mkc_sigchld;

static int run = 0;

void mkc_process_daemon();

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
