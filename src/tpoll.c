/*
 * =====================================================================================
 *
 *       Filename:  tpoll.c
 *
 *    Description: 
 *
 *        Version:  1.0
 *        Created:  2017/03/31 17时31分39秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  吴帅 (腿哥), imsiren@163.com
 *   Organization:  
 *
 * =====================================================================================
 */
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

#include "tpoll.h"

static tpoll_t *tpoll = NULL;

static void *thread_routine(void *arg){

    tpoll_work_t *work;

    while(1){

        pthread_mutex_lock(&tpoll->queue_lock);

        //等待现成空闲
        while(!tpoll->queue_head && !tpoll->shutdown){
            
            pthread_cond_wait(&tpoll->queue_ready , &tpoll->queue_lock);
        }
        if(tpoll->shutdown){

            pthread_mutex_unlock(&tpoll->queue_lock);
            pthread_exit(NULL);
        }

        work = tpoll->queue_head;
        tpoll->queue_head = tpoll->queue_head->next;

        pthread_mutex_unlock(&tpoll->queue_lock);

        work->routine(work->arg);
    }
}


