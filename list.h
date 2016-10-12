/*
 * =====================================================================================
 *
 *       Filename:  list.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2016/05/26 16时03分41秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  YOUR NAME (), 
 *   Organization:  
 *
 * =====================================================================================
 */
#ifndef __LIST_H_
#define __LIST_H_

typedef void (*list_free_handler) (void *ptr);

typedef void (*list_deep_callback) (void *node);

typedef struct list_node{
    struct list_node *prev;
    struct list_node *next;
    void *value;
    char *key;
}list_node;

typedef struct list{
    list_node *head;
    list_node *tail;
    void (*free)(void *ptr);
    size_t len;
}list;

list *list_create();

void list_release(list *list);

list *list_add_node_head(list *list ,char *key,void *value);

list *list_add_node_tail(list *list ,char *key,void *tail);

void list_deep(list *list ,list_deep_callback callback);

list_node *list_find_node(list *list,char *key);
void list_dump(list *list);

#endif
