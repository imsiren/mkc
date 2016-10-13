/*
 * =====================================================================================
 *
 *       Filename:  hash.c
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2016/05/25 15时05分28秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  YOUR NAME (), 
 *   Organization:  
 *
 * =====================================================================================
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "zmalloc.h"
#include "hash.h"

hash_table *hash_init(size_t size){

    hash_table *ht = zmalloc(sizeof(hash_table));

    ht->data    =   zmalloc(sizeof(list) * size);
    memset(ht->data,0,sizeof(list) * size);
    ht->size    =   size;
    ht->element_num = 0;
    return ht;
}
int hash_index(const char *key){

    return 0;
}
unsigned long hashpjw(char *key, unsigned int key_len)
{
    unsigned long h = 0, g;
    char *end = key + key_len; 

    while (key < end ) {
        h = (h << 4) + *key++;
        if ((g = (h & 0xF0000000))) {
            h = h ^ (g >> 24);
            h = h ^ g;
        }
    }
    return h;
}
hash_table *hash_add(hash_table *table,char *key, void *data,void (*list_handler)(void *ptr)){

    unsigned int k = hashpjw(key,strlen(key));

    int index = k % table->size;

    list *p = table->data[index]; 

    if(p == NULL){
        p = list_create();
    }

    p = list_add_node_tail(p,key,data);
    p->free = list_handler;
    table->data[index] = p;

    table->element_num ++;

    return table;
}
void hash_dump(hash_table *table){
    int i = 0;
    list *p = NULL;
    for(i = 0;i < table->size; i++){
        if(table->data[i] != NULL){
            list_dump(table->data[i]);
        }
    }
}
list *hash_find(hash_table *table,char *key,int key_len){
    list *p = NULL;
    unsigned int k = hashpjw(key,key_len);
    int index = k % table->size;
    p = table->data[index];
    if(p == NULL){
        return NULL;
    }
    return p;
}

void hash_free(hash_table *table){
    int i = 0;

    if(table == NULL){
        return ;
    }
    for(i = 0;i <table->size; i++){
        list_release(table->data[i]);
    }
    zfree(table->data);
    zfree(table);
}


