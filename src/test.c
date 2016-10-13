/*
 * =====================================================================================
 *
 *       Filename:  test.c
 *
 *    Description:  i#
 *
 *        Version:  1.0
 *        Created:  2016/10/12 10时08分55秒
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

typedef struct list{

    int key;
}list;

int main(int argc, char **argv){

    list *ll = malloc(sizeof(list) * 10);
    int i;
    for(i = 90;i < 100;i++){
        ll[i].key = i;
    }

    for(i = 90; i <100 ;i++){

        printf("%d\n",ll[i].key);
    }

    free(ll);
}
