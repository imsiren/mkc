/*
 * =====================================================================================
 *
 *       Filename:  kafka.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2017/04/10 22时04分47秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  吴帅 (腿哥), imsiren@163.com
 *   Organization:  
 *
 * =====================================================================================
 */
#ifndef _KAFKA_H
#define _KAFKA_H

#include "config.h"
#include "librdkafka/rdkafka.h"

static rd_kafka_t * rk;
rd_kafka_topic_partition_list_t *topics;

static int kafka_run = 1;
static int wait_eof = 0;

int kafka_init_server();

static int msg_consume(rd_kafka_message_t *rkmessage ,void *opaque);

static int stats_cb(rd_kafka_t *rk, char *json,size_t json_len ,void *opaque);


void kafka_consume(mkc_topic *topic);
void kafka_consume_close();

static void kafka_print_partition_list (FILE *fp,const rd_kafka_topic_partition_list_t *partitions) ;

#endif
