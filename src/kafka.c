/*
 * =====================================================================================
 *
 *       Filename:  kafka.c
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2017/04/10 22时06分18秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  吴帅 (腿哥), imsiren@163.com
 *   Organization:  
 *
 * =====================================================================================
 */
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <sys/time.h>
#include <signal.h>

#include "mkc.h"
#include "cJSON.h"

#include "kafka.h"
#include "logger.h"
#include "http.h"
#include "librdkafka/rdkafka.h"

static void rebalance_cb(rd_kafka_t *rk,rd_kafka_resp_err_t err,rd_kafka_topic_partition_list_t *partitions,void *opaque){

    mkc_write_log(MKC_LOG_NOTICE, "%% Consumer  rebalanced: ");

    switch (err){
        case RD_KAFKA_RESP_ERR__ASSIGN_PARTITIONS:
            mkc_write_log(MKC_LOG_NOTICE, "assigned:");
            kafka_print_partition_list(stderr, partitions);
            rd_kafka_assign(rk, partitions);
            wait_eof += partitions->cnt;
            break;

        case RD_KAFKA_RESP_ERR__REVOKE_PARTITIONS:
            mkc_write_log(MKC_LOG_NOTICE, "revoked:");
            kafka_print_partition_list(stderr, partitions);
            rd_kafka_assign(rk, NULL);
            wait_eof = 0;
            break;

        default:
            mkc_write_log(MKC_LOG_NOTICE, "failed: %s",
                    rd_kafka_err2str(err));
            rd_kafka_assign(rk, NULL);
            break;
    }

}

static int msg_consume(rd_kafka_message_t *rkmessage ,void *opaque){

    if(rkmessage->err ){
        if(rkmessage->err == RD_KAFKA_RESP_ERR__PARTITION_EOF){

            mkc_write_log(MKC_LOG_NOTICE,
                    "Consumer reached end of %s [%"PRId32"]"
                    "message queue at offset %"PRId64" %s",
                    rd_kafka_topic_name(rkmessage->rkt),
                    rkmessage->partition,
                    rkmessage->offset,
                    rkmessage->payload
                    );
            return -1;
        }
        if (rkmessage->rkt){
            mkc_write_log(MKC_LOG_NOTICE, "Consume error for "
                    "topic \"%s\" [%"PRId32"] "
                    "offset %"PRId64": %s ",
                    rd_kafka_topic_name(rkmessage->rkt),
                    rkmessage->partition,
                    rkmessage->offset,
                    rd_kafka_message_errstr(rkmessage));
        }else{
            mkc_write_log(MKC_LOG_ERROR, "%% Consumer error: %s: %s",
                    rd_kafka_err2str(rkmessage->err),
                    rd_kafka_message_errstr(rkmessage));
        }

        if (rkmessage->err == RD_KAFKA_RESP_ERR__UNKNOWN_PARTITION ||
                rkmessage->err == RD_KAFKA_RESP_ERR__UNKNOWN_TOPIC)
            kafka_run = 0;

        return -1;
    }

    //if(rkmessage->key_len){

    mkc_write_log(MKC_LOG_NOTICE,"payload len[%ld]: %s ",rkmessage->len,rkmessage->payload);
    //}

    cjson *root = cjson_parse(rkmessage->payload);

    //判断是否为json格式.

    if(root->type == cJSON_False || root->type == cJSON_NULL){

        mkc_write_log(MKC_LOG_WARNING,"invalid json data :[%s]",rkmessage->payload);
        return -1;
    }	

    int command = cjson_get_item(root,"commandId")->valueint;

    mkc_write_log(MKC_LOG_NOTICE,"commandId[%d] ",command);

    char command_id[128];
    sprintf(command_id,"%d",command);

    list *module_conf = hash_find(server_config.modules, command_id,strlen(command_id));
    list_node *current = NULL;

    if(!module_conf){

        mkc_write_log(MKC_LOG_NOTICE,"can not found module with commandId [%d]",command_id);
        return -1;
    }
    current = module_conf->head;


    while(current != NULL){

        http_response_t *response  = NULL;

        char *header = HTTP_POST;

        module_conf_t * conf = (module_conf_t*)current->value;

        char *url = conf->uri;

        //如果指定了延迟
        if(conf->delay > 0){

            usleep(conf->delay);
        }
        if(rkmessage->len == 0){

            return 0;
        }

        int retry_num = 0;
http_client_post:{
                     response = http_client_post(url,header, rkmessage->payload,rkmessage->len);

                     if(response && response->http_code != 200){

                         mkc_write_log(MKC_LOG_ERROR,"post error url[%s] data[%s] httpcode[%d]",url,rkmessage->payload,response->http_code);

                         zfree(response);
                        mkc_write_log(MKC_LOG_ERROR,"::::::::::%d\t%d\n",conf->retrynum,retry_num);
                         //如果指定了了重试次数或者为0，则一直重试
                         //如果一直失败会阻塞
                         if(conf->retrynum == 0 || (conf->retrynum > 0 && retry_num ++ < conf->retrynum)){
                             usleep(1000);
                             goto http_client_post;
                         }
                     }
                 }
                 current = current->next;
    }
    return 0;
}
static int stats_cb(rd_kafka_t *rk, char *json,size_t json_len ,void *opaque){

    mkc_write_log(MKC_LOG_NOTICE,"%s",json);
    return 0;
}
static void logger (const rd_kafka_t *rk, int level,
        const char *fac, const char *buf) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    fprintf(stdout, "%u.%03u RDKAFKA-%i-%s: %s: %s\n",
            (int)tv.tv_sec, (int)(tv.tv_usec / 1000),
            level, fac, rd_kafka_name(rk), buf);
}

int kafka_init_server(){

    const char *debug = "debug";
    rd_kafka_resp_err_t err;
    rd_kafka_conf_t *conf;

    rd_kafka_topic_conf_t *topic_conf;

    char errstr[512];
    char tmp[16];

    snprintf(tmp,sizeof(tmp), "%i",SIGIO);

    conf = rd_kafka_conf_new();

    if(rd_kafka_conf_set(conf, "metadata.broker.list",server_config.brokers,errstr,sizeof(errstr) != RD_KAFKA_CONF_OK)){

        mkc_write_log(MKC_LOG_ERROR,"Failed to set brokers:%s",errstr);
        exit(1);
    }

    //设置日志记录callback
    rd_kafka_conf_set_log_cb(conf,logger);

    rd_kafka_conf_set(conf,"internal.termination.signal", tmp, NULL, 0);

    topic_conf = rd_kafka_topic_conf_new();

    char mode = 'C';
    int daemon = 0,verbose = 0;


    mkc_topic *topic = NULL;
    char *conf_file = "";


    if(rd_kafka_conf_set(conf,"debug",debug,errstr,sizeof(errstr)) != RD_KAFKA_CONF_OK){

        mkc_write_log(MKC_LOG_NOTICE,"%%Debug configuration failed:%s :%s",errstr,debug);
    }

    //rd_kafka_conf_set_stats_cb(conf,stats_cb);
    if(strchr("CO",mode)){
        if(!server_config.group){

            server_config.group = "rdkafka_default";
        }
        if(rd_kafka_conf_set(conf,"group.id",server_config.group,errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK){

            mkc_write_log(MKC_LOG_ERROR,"%% %s",errstr);
            exit(1);
        }
        //支持断点续传
        if(rd_kafka_topic_conf_set(topic_conf,"offset.store.path",server_config.log_path,errstr,sizeof(err) != RD_KAFKA_CONF_OK)){

            mkc_write_log(MKC_LOG_ERROR,"%% %s",errstr);
            exit(1);
        }
        if(rd_kafka_topic_conf_set(topic_conf,"offset.store.sync.interval.ms","100",errstr,sizeof(err)) != RD_KAFKA_CONF_OK){

            mkc_write_log(MKC_LOG_ERROR,"%% %s",errstr);
            exit(1);
        }
        if(rd_kafka_topic_conf_set(topic_conf,"offset.store.method","broker",errstr,sizeof(err)) != RD_KAFKA_CONF_OK){

            mkc_write_log(MKC_LOG_ERROR,"%% %s",errstr);
            exit(1);
        }
        rd_kafka_conf_set_default_topic_conf(conf,topic_conf);

        rd_kafka_conf_set_rebalance_cb(conf,rebalance_cb);
    }

    if(!(rk = rd_kafka_new(RD_KAFKA_CONSUMER,conf,errstr, sizeof(errstr)))){

        mkc_write_log(MKC_LOG_ERROR," Failed to create new consumer:%s",errstr);
        exit(1);
    }

    if(mode == 'D'){
        int r;
        /*  
            r = describe_s(rk,group);
            rd_kafka_destroy(rk);
            exit(r == -1 ? 1 : 0);
            */
    }

    rd_kafka_poll_set_consumer(rk);

    //绑定信号
    mkc_set_worker_process_handler();
    return 0;
}

void kafka_consume(mkc_topic *topic){

    rd_kafka_resp_err_t err;
    topics = rd_kafka_topic_partition_list_new(server_config.argc - optind);

    int partition = -1;
    int i = 0;
    int is_subscription = 1;

    list_node *node;
    node = server_config.topics->head;

    if(server_config.topics->len > 1){

        is_subscription = 1;
    }
    //for(i = 0 ; i < server_config.topics->len ;i ++){

    //mkc_topic *mtopic = 0;
    //   topic = (mkc_topic*)node->value;
    mkc_write_log(MKC_LOG_NOTICE, "node value :%s partition:%d\n",topic->name,topic->partition);

    node = node->next;
    rd_kafka_topic_partition_list_add(topics, topic->name,topic->partition);

    if(topic->offset > 0){

        if(rd_kafka_topic_partition_list_set_offset(topics,topic->name,topic->partition,topic->offset) != RD_KAFKA_RESP_ERR_NO_ERROR){

            mkc_write_log(MKC_LOG_ERROR,"topic [%s] partition was not found [%s]",topic->name,topic->partition);
        }
    }
    //}

    if(is_subscription){

        //订购topic
        mkc_write_log(MKC_LOG_NOTICE,"Subscribing to %d topics",topics->cnt);
        if((err = rd_kafka_subscribe(rk,topics))){

            mkc_write_log(MKC_LOG_NOTICE,"Failed to assign consuming topics:%s",rd_kafka_err2str(err));
            exit(1);
        }
    }else{
        if((err = rd_kafka_assign(rk,topics))){

            mkc_write_log(MKC_LOG_NOTICE,"Failed to assign partitions:%s",rd_kafka_err2str(err));
        }
    }

    mkc_write_log(MKC_LOG_NOTICE,"goto recv consume data...");
    while(kafka_run){

        rd_kafka_message_t *rkmessage;
        rkmessage = rd_kafka_consumer_poll(rk, server_config.timeout);

        if(rkmessage){

            msg_consume(rkmessage, NULL);
            rd_kafka_message_destroy(rkmessage);
            continue;
        }
        err = rd_kafka_last_error();
        //mkc_write_log(MKC_LOG_NOTICE,"[%d] no message with error : %s ",getpid(),rd_kafka_err2str(err));
    }

}
void kafka_consume_close(){

    rd_kafka_resp_err_t err;

    err = rd_kafka_consumer_close(rk);

    if(err){
        mkc_write_log(MKC_LOG_NOTICE, "%% Failed to close consumer: %s",
                rd_kafka_err2str(err));
    }else{
        mkc_write_log(MKC_LOG_NOTICE,"Consumer closed");
    }
    rd_kafka_topic_partition_list_destroy(topics);

    kafka_run = 5;
    while (kafka_run-- > 0 && rd_kafka_wait_destroyed(1000) == -1){

        mkc_write_log(MKC_LOG_NOTICE, "Waiting for librdkafka to decommission\n");
    }

    if (kafka_run <= 0){

        FILE *fp = fopen(server_config.logfile,"a+");

        if(!fp){

            mkc_write_log(MKC_LOG_WARNING,"Failed to open file [%s].", server_config.logfile);
            rd_kafka_dump(stdout, rk);
        }else{

            rd_kafka_dump(fp, rk);
            fclose(fp);
        }
    }
    rd_kafka_destroy(rk);

}

static void kafka_print_partition_list (FILE *fp,
        const rd_kafka_topic_partition_list_t
        *partitions) {
    int i;
    for (i = 0 ; i < partitions->cnt ; i++) {
        mkc_write_log(MKC_LOG_WARNING, "%s %s [%"PRId32"] offset %"PRId64" ",
                i > 0 ? ",":"",
                partitions->elems[i].topic,
                partitions->elems[i].partition,
                partitions->elems[i].offset
                );
    }

}
