/*
 * =====================================================================================
 *
 *       Filename:  hash.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2016/05/25 14时37分57秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  YOUR NAME (), 
 *   Organization:  
 *
 * =====================================================================================
 */
#ifndef __HASH_H_
#define __HASH_H_

#include "zmalloc.h"
#include "list.h"

#define HASH_MAX_SIZE 757 

typedef struct {

    int size;
    int free;
    int element_num;
    list **data;
}hash_table;

hash_table *hash_init(size_t size);

list *hash_find(hash_table *table,char *key,int key_len);

hash_table *hash_add(hash_table *table,char *key,void *data,void (*list_handler)(void *ptr));

unsigned long hashpjw(char *key, unsigned int key_len);

int hash_delete(hash_table *table,const char *key);

void hash_free(hash_table *table);

void hash_dump(hash_table *table);

#endif
