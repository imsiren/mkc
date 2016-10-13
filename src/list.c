/*
 * =====================================================================================
 *
 *       Filename:  list.c
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2016/05/26 16时06分30秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  YOUR NAME (), 
 *   Organization:  
 *
 * =====================================================================================
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "list.h"
#include "zmalloc.h"

list *list_create(){
    list *l = zmalloc(sizeof(list));
    l->head = NULL;
    l->tail = NULL;
    l->free = NULL;

    l->len  = 0;
    return l;
}
void list_release(list *list){
    if(list == NULL)
        return;
    size_t len = list->len;
    list_node *current,*next;
    current = list->head;
    while(len --){
        next = current->next;
        if(list->free) list->free(current->value);    
        zfree(current);
        current = next;
    }
    zfree(list);
}
list *list_add_node_head(list *list,char *key,void *value){
    list_node *node = zmalloc(sizeof(list_node));
    node->prev = NULL;
    node->next = NULL;

    node->value = value;
    node->key   = key;
    if(list->len == 0){
        list->head = node;
        list->tail = node;
        node->prev = node->next = NULL;
    }else{
        node->prev = NULL;
        node->next = list->head;
        list->head->prev = node;
        list->head = node;
    }
    list->len ++;
    return list;
}
list *list_add_node_tail(list *list,char *key,void *value){
    list_node *node = zmalloc(sizeof(list_node));
    node->prev = NULL;
    node->next = NULL;
    node->value = value;
    node->key   = key;
    if(list ->len == 0){
        node->next = node->prev = NULL;
        list->head = node;
        list->tail = node;
    }else{
        node->prev = list->tail;
        node->next = NULL;
        list->tail->next = node;
        list->tail = node;
    }
    list->len++;
    return list;
}
list_node *list_find_node(list *list,char *key){

    if(list == NULL || list->len == 0){
        return NULL;
    }
    int len = list->len;

    list_node *current,*next,*node = NULL;
    current = list->head;
    while(len--){
        next = current->next;
        if(!strcmp(current->key,key)){
            node = current;
            break;
        }
        current = next;
    }
    return node;
}

void list_deep(list *list,list_deep_callback deep_call){

    if(list->len == 0){

        return;
    }

    list_node *current;

    current = list->head;

    while(current){

        if(deep_call){

            deep_call((void*)current);
        }
        current = current->next;
    }

}
//test
void list_dump(list *list){
    int len = list->len;
    list_node *current,*next;
    current = list->head;
    printf("%ld\n",list->len);
    while(current){
         next = current->next;
         printf("data:%s\n",current->value);
         current = next;
    }
}
