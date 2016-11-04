/*
 * =====================================================================================
 *
 *       Filename:  main.c
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2016/10/09 16时07分59秒
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
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <syslog.h>
#include <signal.h>
#include <ctype.h>
#include "librdkafka/rdkafka.h"
#include "cJSON.h"
#include "http.h"
#include "config.h"
#include "sds.h"
#include "logger.h"

//#include <librdkafka/rdkafka.h>  /* for Kafka driver */

#include <zookeeper/zookeeper.h>
#include <zookeeper/zookeeper.jute.h>
#include <jansson.h>

#define SERVER_COMMAND_NUM 200

#define BROKER_PATH "/brokers/ids"

static int run = 1;

static int wait_eof = 0;
static rd_kafka_t * rk;

server_conf_t server_config;

/**
 * @brief 查找zookeeper节点
 *
 * @param zzh
 * @param brokers
 * */
static void set_brokerlist_from_zookeeper(zhandle_t *zzh, char *brokers){

    if(zzh){

        struct String_vector brokerlist;
        if(zoo_get_children(zzh, BROKER_PATH,1,&brokerlist) != ZOK){

            mkc_write_log(MKC_LOG_ERROR,"No brokers found on path %s",BROKER_PATH);
            return;
        }

        int i ;
        char *brokerptr = brokers;
        for(i = 0; i < brokerlist.count;i++){

            char path[255], cfg[1024];
            sprintf(path,"/brokers/ids/%s",brokerlist.data[i]);
            int len = sizeof(cfg);
            zoo_get(zzh, path,0,cfg,&len ,NULL);

            if(len > 0){

                cfg[len] = '\0';
                json_error_t jerror;
                json_t *jobj = json_loads(cfg, 0 , &jerror);

                if(jobj){
                    json_t *jhost = json_object_get(jobj,"host");
                    json_t *jport = json_object_get(jobj,"port");

                    if(jhost && jport){

                        const char *host = json_string_value(jhost);
                        const int port   = json_integer_value(jport);
                        sprintf(brokerptr,"%s:%d",host,port);
                        brokerptr += strlen(brokerptr);

                        if(i < brokerlist.count - 1){

                            *brokerptr++ = ',';
                        }
                    }
                    json_decref(jobj);

                }
            }
        }
        deallocate_String_vector(&brokerlist);
        mkc_write_log(MKC_LOG_NOTICE,"Found brokers %s\n",brokers);
    }
}

static void watcher(zhandle_t *zh,int type,int state, const char *path, void *watcherCtx){

    char brokers[1024];

    if(type == ZOO_CHILD_EVENT && strncmp(path,BROKER_PATH, sizeof(BROKER_PATH) - 1) == 0){

        brokers[0] = '\0';

        set_brokerlist_from_zookeeper(zh,brokers);

        if(brokers[0] != '\0' && rk != NULL){

            rd_kafka_brokers_add(rk,brokers);
            rd_kafka_poll(rk,10);
        }
    }

}

static zhandle_t* initialize_zookeeper(const char *zookeeper,const int debug){

    zhandle_t *zh;

    if(debug){
        zoo_set_debug_level(ZOO_LOG_LEVEL_DEBUG);
    }

    zh = zookeeper_init(zookeeper,watcher, 10000,0,0,0);

    if(zh == NULL){

        mkc_write_log(MKC_LOG_ERROR,"%s","Zookeeper connection not established.");
        exit(1);
    }

    return zh;
}

static int process_running(int argc, char **argv);

static void shut_down(){

    list_release(server_config.commands);

    //TODO 释放modules
}

static void stop(int sig){
    if(!run){
        exit(1);
    }
    run = 0;
    shut_down();
    fclose(stdin);
}

static void sig_usr1 (int sig) {
    //	rd_kafka_dump(stdout, rk);
    stop(sig);
}

static void init_server_conf(){

    server_config.brokers = sdsnew("");
    server_config.zookeeper = sdsnew("");
    server_config.daemonize = 1;
    server_config.pidfile = "./mmq.pid";
    server_config.loglevel = 1 ;//warning
    server_config.logfile = "./logs/mkc.log";
    server_config.confpath = "./conf";

    server_config.timeout = 100;
    //server_config.cmd_t = hash_init(SERVER_COMMAND_NUM);

    server_config.topics = list_create();
    server_config.commands = list_create();
    //server_config.modules = zmalloc(sizeof(list) * SERVER_COMMAND_NUM);
    server_config.modules = hash_init(SERVER_COMMAND_NUM);
}

static void logger(const rd_kafka_t *rk,int level, const char *fac, const char *buf){

}
static void print_partition_list (FILE *fp,
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


static void rebalance_cb(rd_kafka_t *rk,rd_kafka_resp_err_t err,rd_kafka_topic_partition_list_t *partitions,void *opaque){

    mkc_write_log(MKC_LOG_NOTICE, "%% Consumer  rebalanced: \n");

    switch (err){
        case RD_KAFKA_RESP_ERR__ASSIGN_PARTITIONS:
            mkc_write_log(MKC_LOG_NOTICE, "assigned:\n");
            print_partition_list(stderr, partitions);
            rd_kafka_assign(rk, partitions);
            wait_eof += partitions->cnt;
            break;

        case RD_KAFKA_RESP_ERR__REVOKE_PARTITIONS:
            mkc_write_log(MKC_LOG_NOTICE, "revoked:\n");
            print_partition_list(stderr, partitions);
            rd_kafka_assign(rk, NULL);
            wait_eof = 0;
            break;

        default:
            mkc_write_log(MKC_LOG_NOTICE, "failed: %s\n",
                    rd_kafka_err2str(err));
            rd_kafka_assign(rk, NULL);
            break;
    }

}

static int stats_cb(rd_kafka_t *rk, char *json,size_t json_len ,void *opaque){

    mkc_write_log(MKC_LOG_NOTICE,"%s\n",json);
    return 0;
}

static void module_deep_process(void *node){

}

static int msg_consume(rd_kafka_message_t *rkmessage ,void *opaque){

    if(rkmessage->err ){
        if(rkmessage->err == RD_KAFKA_RESP_ERR__PARTITION_EOF){

            mkc_write_log(MKC_LOG_NOTICE,
                    "Consumer reached end of %s [%"PRId32"]"
                    "message queue at offset %"PRId64" %s\n",
                    rd_kafka_topic_name(rkmessage->rkt),
                    rkmessage->partition,
                    rkmessage->offset,
                    rkmessage->payload
                   );
            return -1;
        }
        if (rkmessage->rkt){
            mkc_write_log(MKC_LOG_NOTICE, "%% Consume error for "
                    "topic \"%s\" [%"PRId32"] "
                    "offset %"PRId64": %s\n",
                    rd_kafka_topic_name(rkmessage->rkt),
                    rkmessage->partition,
                    rkmessage->offset,
                    rd_kafka_message_errstr(rkmessage));
        }else{
            mkc_write_log(MKC_LOG_ERROR, "%% Consumer error: %s: %s\n",
                    rd_kafka_err2str(rkmessage->err),
                    rd_kafka_message_errstr(rkmessage));
        }

        if (rkmessage->err == RD_KAFKA_RESP_ERR__UNKNOWN_PARTITION ||
                rkmessage->err == RD_KAFKA_RESP_ERR__UNKNOWN_TOPIC)
            run = 0;

        return -1;
    }

    //if(rkmessage->key_len){

    mkc_write_log(MKC_LOG_NOTICE,"payload len[%ld]: %s \n",rkmessage->len,rkmessage->payload);
    //}

    cjson *root = cjson_parse(rkmessage->payload);

    //判断是否为json格式.

    if(root->type == cJSON_False || root->type == cJSON_NULL){

        mkc_write_log(MKC_LOG_WARNING,"invalid json data :[%s]\n",rkmessage->payload);
        return -1;
    }	

    int command = cjson_get_item(root,"commandId")->valueint;

    printf("commandId:%d\n",command);

    char command_id[128];
    sprintf(command_id,"%d",command);

    list *module_conf = hash_find(server_config.modules, command_id,strlen(command_id));
    list_node *current = NULL;

    if(!module_conf){

        mkc_write_log(MKC_LOG_NOTICE,"can not found module with commandId [%d] \n",command_id);
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

        int retry_num = 0;
http_client_post:{
                     response = http_client_post(url,header, rkmessage->payload,rkmessage->len);

                     if(response && response->http_code != 200){

                         zfree(response);
                         //如果指定了了重试次数或者为0，则一直重试
                         //如果一直失败会造成队列阻塞
                         if(conf->retrynum == 0 || (conf->retrynum > 0 && retry_num ++ < conf->retrynum)){
                             mkc_write_log(MKC_LOG_ERROR,"post error url[%s] data[%s]\n",url,rkmessage->payload);
                             goto http_client_post;
                         }
                     }
                 }
                 current = current->next;
    }
    return 0;
}

static void usage(){
    fprintf(stderr,
            "Usage: bin [options] <topic[:part]> <topic[:part]>..\n"
            "\n"
            "librdkafka version %s (0x%08x)\n"
            "\n"
            " Options:\n"
            "  -c <>      config file\n"
            "  -b <brokers>    Broker address \n"
            "  topic "
            "\n",
            rd_kafka_version_str(), rd_kafka_version()
           );

}

int main(int argc, char **argv){

    init_server_conf();

    int opt;


    while((opt = getopt(argc, argv,"c:u:b:g:v:d:DO")) != -1){

        switch(opt){
            case 'c':
                server_config.conffile = optarg;
                break;

            case 'u': //urls

                server_config.url = optarg;
                break;
            case 'b': //brokers

                server_config.brokers= optarg;
                break;
            case 'g': //

                server_config.group = optarg;
                break;
            case 'v'://verbose

                server_config.verbose = 1;
                break;
            case 'd': //daemon

                server_config.daemon = 1;
                break;
            case 'D':
            case 'O':

                server_config.mode = opt;
                break;
            default:
                usage();
                exit(1);
                break;
        }
    }
    signal(SIGTTOU, SIG_IGN);

    signal(SIGINT,stop);
    signal(SIGKILL,stop);
    signal(SIGUSR1,sig_usr1);

    if(!server_config.conffile ){

        usage();
        exit(1);
    }else{
        fprintf(stderr,"load conf file:%s\n",server_config.conffile);
    }
    if(parse_server_conf(server_config.conffile) == -1){
        usage();
        exit(1);
    }

    if(sdslen(server_config.brokers) == 0){

        mkc_write_log(MKC_LOG_ERROR,"brokers is empty.");
        exit(1);
    }

    process_running(argc, argv);

    exit(0);
    /*   后续加入守护进程。
         int i ;
         int process_num =1;// server_config.commands->len;
         pid_t pid;
         for(i = 0;i < process_num ;i ++){

         if((pid = fork()) < 0){

         mkc_write_log(MKC_LOG_NOTICE,"fork child process error %d %s.", errno ,strerror(errno));
         exit(-1);
         }else if(pid > 0){ //parent process

         process_running(argc, argv);
         }else if(pid == 0){ //child process

         while(1);
         }
         }
         */
}
static int process_running(int argc, char **argv){

    const char *debug = "debug";

    zhandle_t *zh = NULL;

    rd_kafka_conf_t *conf;

    rd_kafka_topic_conf_t *topic_conf;

    rd_kafka_resp_err_t err;
    rd_kafka_topic_partition_list_t *topics;

    //char brokers[1024];
    //初始化zookeeper
    zh = initialize_zookeeper(server_config.zookeeper,debug != NULL);

    //添加节点
    set_brokerlist_from_zookeeper(zh,server_config.brokers);

    char errstr[512];
    char tmp[16];

    snprintf(tmp,sizeof(tmp), "%i",SIGIO);

    conf = rd_kafka_conf_new();

    if(rd_kafka_conf_set(conf, "metadata.broker.list",server_config.brokers,errstr,sizeof(errstr) != RD_KAFKA_CONF_OK)){

        mkc_write_log(MKC_LOG_ERROR,"Failed to set brokers:%s\n",errstr);
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


    if(server_config.debug > 0 && rd_kafka_conf_set(conf,"debug",debug,errstr,sizeof(errstr)) != RD_KAFKA_CONF_OK){

        mkc_write_log(MKC_LOG_NOTICE,"%%Debug configuration failed:%s :%s\n",errstr,debug);
    }

    //rd_kafka_conf_set_stats_cb(conf,stats_cb);
    if(strchr("CO",mode)){
        if(!server_config.group){

            server_config.group = "rdkafka_default";
        }
        if(rd_kafka_conf_set(conf,"group.id",server_config.group,errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK){

            mkc_write_log(MKC_LOG_ERROR,"%% %s\n",errstr);
            exit(1);
        }
        if(rd_kafka_topic_conf_set(topic_conf,"offset.store.path",server_config.log_path,errstr,sizeof(err) != RD_KAFKA_CONF_OK)){

            exit(1);
        }
        if(rd_kafka_topic_conf_set(topic_conf,"offset.store.sync.interval.ms","100",errstr,sizeof(err)) != RD_KAFKA_CONF_OK){

            exit(1);
        }
        if(rd_kafka_topic_conf_set(topic_conf,"offset.store.method","broker",errstr,sizeof(err)) != RD_KAFKA_CONF_OK){

            mkc_write_log(MKC_LOG_ERROR,"%% %s\n",errstr);
            exit(1);
        }
        rd_kafka_conf_set_default_topic_conf(conf,topic_conf);

        rd_kafka_conf_set_rebalance_cb(conf,rebalance_cb);
    }

    if(!(rk = rd_kafka_new(RD_KAFKA_CONSUMER,conf,errstr, sizeof(errstr)))){

        mkc_write_log(MKC_LOG_ERROR," Failed to create new consumer:%s\n",errstr);
        exit(1);
    }

    if(server_config.debug > 0){

        rd_kafka_set_log_level(rk,LOG_DEBUG);
    }

    if(rd_kafka_brokers_add(rk, server_config.brokers) == 0){

        mkc_write_log(MKC_LOG_ERROR,"No valid brokers.\n");
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

    topics = rd_kafka_topic_partition_list_new(argc - optind);

    int partition = -1;
    int i = 0;
    int is_subscription = 1;

    list_node *node;
    node = server_config.topics->head;

    if(server_config.topics->len > 1){

        is_subscription = 1;
    }
    for(i = 0 ; i < server_config.topics->len ;i ++){

        //mkc_topic *mtopic = 0;
        topic = (mkc_topic*)node->value;
        printf("node value :%s partition:%d\n",topic->name,topic->partition);

        node = node->next;
        rd_kafka_topic_partition_list_add(topics, topic->name,topic->partition);

        if(topic->offset > 0){

            if(rd_kafka_topic_partition_list_set_offset(topics,topic->name,topic->partition,topic->offset) != RD_KAFKA_RESP_ERR_NO_ERROR){

                mkc_write_log(MKC_LOG_ERROR,"topic [%s] partition was not found [%s]",topic->name,topic->partition);
            }
        }
    }
    /* 

       for(i = optind; i < argc; i++){
       char *topic = argv[i];

       char *t;
       int32_t partition = -1;
       if((t = strstr(topic,":"))){

     *t = '\0';
     partition = atoi(t+1);
     is_subscription = 0;
     }
     printf("partition:%d\n",partition);

     mkc_write_log(MKC_LOG_NOTICE,"topic:%s\n",topic);
     rd_kafka_topic_partition_list_add(topics, topic,partition);
     }
     */

    if(is_subscription){

        //订购topic
        mkc_write_log(MKC_LOG_NOTICE,"Subscribing to %d topics\n",topics->cnt);
        if((err = rd_kafka_subscribe(rk,topics))){

            mkc_write_log(MKC_LOG_NOTICE,"Failed to assign consuming topics:%s\n",rd_kafka_err2str(err));
            exit(1);
        }
    }else{
        if((err = rd_kafka_assign(rk,topics))){

            mkc_write_log(MKC_LOG_NOTICE,"Failed to assign partitions:%s\n",rd_kafka_err2str(err));
        }
    }

    mkc_write_log(MKC_LOG_NOTICE,"goto recv consume data...\n");
    while(run){

        rd_kafka_message_t *rkmessage;
        rkmessage = rd_kafka_consumer_poll(rk, server_config.timeout);

        if(rkmessage){

            msg_consume(rkmessage, NULL);
            rd_kafka_message_destroy(rkmessage);
            continue;
        }
        err = rd_kafka_last_error();
        mkc_write_log(MKC_LOG_DEBUG,"no message with error : %s \n",rd_kafka_err2str(err));
    }

done:
    err = rd_kafka_consumer_close(rk);
    if(err){
        mkc_write_log(MKC_LOG_NOTICE, "%% Failed to close consumer: %s\n",
                rd_kafka_err2str(err));
    }else{

        mkc_write_log(MKC_LOG_NOTICE,"Consumer closed\n");
    }
    rd_kafka_topic_partition_list_destroy(topics);

    run = 5;
    while (run-- > 0 && rd_kafka_wait_destroyed(1000) == -1)
        printf("Waiting for librdkafka to decommission\n");
    if (run <= 0)
        rd_kafka_dump(stdout, rk);
    rd_kafka_destroy(rk);
    zookeeper_close(zh);

    return 0;
}
