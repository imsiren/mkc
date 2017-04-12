/*
 * =====================================================================================
 *
 *       Filename:  tpoll.h
 *
 *    Description: 线程池 
 *
 *        Version:  1.0
 *        Created:  2017/03/31 17时24分50秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  吴帅 (腿哥), imsiren@163.com
 *   Organization:  
 *
 * =====================================================================================
 */

#ifndef _TPOLL_H_
#define _TPOLL_H_


typedef struct {

    void (*routine)(void *);

    void    *arg;
    struct tpoll_work *next;
}tpoll_work_t;

typedef struct tpoll {
    //是否销毁
    int shutdown;

    //最大线程池
    int max_thread_num;

    //线程ID数组
    pthread_t *tids;

    tpoll_work_t *queue_head;

    pthread_mutex_t queue_lock;
    
    pthread_cond_t queue_ready;
}tpoll_t;

int tpoll_create(int max_thread_num);

void tpoll_add_work(void *(routine)(void *),void *arg);


#endif
