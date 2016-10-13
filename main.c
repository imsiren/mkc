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

#define SERVER_COMMAND_NUM 200

static int run = 1;

static int wait_eof = 0;
static rd_kafka_t * rk;

server_conf_t server_config;

static int process_running(int argc, char **argv);

static void shutdown(){

    list_release(server_config.commands);

    //TODO 释放modules
}

static void stop(int sig){
    if(!run){
        exit(1);
    }
    run = 0;
    shutdown();
    fclose(stdin);
}

static void sig_usr1 (int sig) {
    //	rd_kafka_dump(stdout, rk);
    stop(sig);
}

static void init_server_conf(){

    server_config.brokers = sdsnew("");
    server_config.daemonize = 1;
    server_config.pidfile = "./mmq.pid";
    server_config.loglevel = 1 ;//warning
    server_config.logfile = "./logs/mmq.log";
    server_config.confpath = "./conf";
    server_config.appendqueuelog= 0;
    server_config.queuelogfile= "./logs/queue.log";

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
                fprintf(stderr, "%s %s [%"PRId32"] offset %"PRId64" ",
                        i > 0 ? ",":"",
                        partitions->elems[i].topic,
                        partitions->elems[i].partition,
			partitions->elems[i].offset
            );
        }
        fprintf(stderr, "\n");

}


static void rebalance_cb(rd_kafka_t *rk,rd_kafka_resp_err_t err,rd_kafka_topic_partition_list_t *partitions,void *opaque){

	fprintf(stderr, "%% Consumer  rebalanced: ");

    switch (err){
        case RD_KAFKA_RESP_ERR__ASSIGN_PARTITIONS:
            fprintf(stderr, "assigned:\n");
            print_partition_list(stderr, partitions);
            rd_kafka_assign(rk, partitions);
            wait_eof += partitions->cnt;
            break;

        case RD_KAFKA_RESP_ERR__REVOKE_PARTITIONS:
            fprintf(stderr, "revoked:\n");
            print_partition_list(stderr, partitions);
            rd_kafka_assign(rk, NULL);
            wait_eof = 0;
            break;

        default:
            fprintf(stderr, "failed: %s\n",
                    rd_kafka_err2str(err));
            rd_kafka_assign(rk, NULL);
            break;
    }

}

static int stats_cb(rd_kafka_t *rk, char *json,size_t json_len ,void *opaque){

    fprintf(stderr,"%s\n",json);
    return 0;
}

static void module_deep_process(void *node){

}

static void msg_consume(rd_kafka_message_t *rkmessage ,void *opaque){

    if(rkmessage->err ){
        if(rkmessage->err == RD_KAFKA_RESP_ERR__PARTITION_EOF){

            fprintf(stderr,
                    "Consumer reached end of %s [%"PRId32"]"
                    "message queue at offset %"PRId64" %s\n",
                    rd_kafka_topic_name(rkmessage->rkt),
                    rkmessage->partition,
                    rkmessage->offset,
                    rkmessage->payload
                   );
            return;
        }
        if (rkmessage->rkt){
            fprintf(stderr, "%% Consume error for "
                    "topic \"%s\" [%"PRId32"] "
                    "offset %"PRId64": %s\n",
                    rd_kafka_topic_name(rkmessage->rkt),
                    rkmessage->partition,
                    rkmessage->offset,
                    rd_kafka_message_errstr(rkmessage));
        }else{
            fprintf(stderr, "%% Consumer error: %s: %s\n",
                    rd_kafka_err2str(rkmessage->err),
                    rd_kafka_message_errstr(rkmessage));
        }

        if (rkmessage->err == RD_KAFKA_RESP_ERR__UNKNOWN_PARTITION ||
                rkmessage->err == RD_KAFKA_RESP_ERR__UNKNOWN_TOPIC)
            run = 0;

        return;
    }

    //if(rkmessage->key_len){

        printf("payload len[%ld]: %s \n",rkmessage->len,rkmessage->payload);
    //}

        cjson *root = cjson_parse(rkmessage->payload);
        int command = cjson_get_item(root,"commit_id")->valueint;

        printf("commandId:%d\n",command);

        char command_id[128];
        sprintf(command_id,"%d",command);

        list *module_conf = hash_find(server_config.modules, command_id,strlen(command_id));

        list_node *current = NULL;

        current = module_conf->head;

        while(current){

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
                         //如果制定了重试次数
                         if(conf->retrynum > 0 && retry_num ++ <= conf->retrynum){
                             goto http_client_post;
                         }else{
                             //没有指定一直重试,这会造成队列阻塞
                            goto http_client_post;
                         }
                     }
                 }
                 current = current->next;
        }
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

    while((opt = getopt(argc, argv,"c::u:b:g:v:d:DO")) != -1){

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

    if(parse_server_conf(server_config.conffile) == -1){
        usage();
        exit(1);
    }

    if(sdslen(server_config.brokers) == 0){

        fprintf(stderr,"brokers is empty.");
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

            fprintf(stderr,"fork child process error %d %s.", errno ,strerror(errno));
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

    rd_kafka_conf_t *conf;

    rd_kafka_topic_conf_t *topic_conf;

    rd_kafka_resp_err_t err;
    rd_kafka_topic_partition_list_t *topics;

    char errstr[512];
    char tmp[16];

    snprintf(tmp,sizeof(tmp), "%i",SIGIO);

    conf = rd_kafka_conf_new();

    //设置日志记录callback
    rd_kafka_conf_set_log_cb(conf,logger);

    rd_kafka_conf_set(conf,"internal.termination.signal", tmp, NULL, 0);

    topic_conf = rd_kafka_topic_conf_new();

    char mode = 'C';
    int daemon = 0,verbose = 0;

    const char *debug = "debug";

    char *topic = NULL;
    char *conf_file = "";


    if(server_config.debug > 0 && rd_kafka_conf_set(conf,"debug",debug,errstr,sizeof(errstr)) != RD_KAFKA_CONF_OK){

        fprintf(stderr,"%%Debug configuration failed:%s :%s",errstr,debug);
    }

    //rd_kafka_conf_set_stats_cb(conf,stats_cb);
    if(strchr("CO",mode)){
        if(!server_config.group){

            server_config.group = "rdkafka_default";
        }
        if(rd_kafka_conf_set(conf,"group.id",server_config.group,errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK){

            fprintf(stderr,"%% %s\n",errstr);
            exit(1);
        }
        if(rd_kafka_topic_conf_set(topic_conf,"offset.store.path",server_config.log_path,errstr,sizeof(err) != RD_KAFKA_CONF_OK)){

            exit(1);
        }
        if(rd_kafka_topic_conf_set(topic_conf,"offset.store.sync.interval.ms","100",errstr,sizeof(err)) != RD_KAFKA_CONF_OK){

            exit(1);
        }
        if(rd_kafka_topic_conf_set(topic_conf,"offset.store.method","broker",errstr,sizeof(err)) != RD_KAFKA_CONF_OK){

            fprintf(stderr,"%% %s\n",errstr);
            exit(1);
        }
        rd_kafka_conf_set_default_topic_conf(conf,topic_conf);

        rd_kafka_conf_set_rebalance_cb(conf,rebalance_cb);
    }

    if(!(rk = rd_kafka_new(RD_KAFKA_CONSUMER,conf,errstr, sizeof(errstr)))){

        fprintf(stderr," Failed to create new consumer:%s\n",errstr);
        exit(1);
    }

    if(server_config.debug > 0){

        rd_kafka_set_log_level(rk,LOG_DEBUG);
    }

    if(rd_kafka_brokers_add(rk, server_config.brokers) == 0){

        fprintf(stderr,"No valid brokers.\n");
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

        topic = node->value;
        printf("node value :%s partition:%d\n",topic,partition);

        node = node->next;
        rd_kafka_topic_partition_list_add(topics, topic,partition);
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

        fprintf(stderr,"topic:%s\n",topic);
        rd_kafka_topic_partition_list_add(topics, topic,partition);
    }
    */

    if(is_subscription){

        //订购topic
        fprintf(stderr,"Subscribing to %d topics\n",topics->cnt);
        if((err = rd_kafka_subscribe(rk,topics))){

            fprintf(stderr,"Failed to assign consuming topics:%s\n",rd_kafka_err2str(err));
            exit(1);
        }
    }else{
        if((err = rd_kafka_assign(rk,topics))){

            fprintf(stderr,"Failed to assign partitions:%s\n",rd_kafka_err2str(err));
        }
    }

    fprintf(stderr,"goto recv consume data...\n");
    while(run){

        rd_kafka_message_t *rkmessage;
        rkmessage = rd_kafka_consumer_poll(rk, server_config.timeout);

        if(rkmessage){

            msg_consume(rkmessage, NULL);
            rd_kafka_message_destroy(rkmessage);
            continue;
        }
        err = rd_kafka_last_error();
        fprintf(stderr,"no message  %s\r",rd_kafka_err2str(err));
    }

done:
    err = rd_kafka_consumer_close(rk);
    if(err){
        fprintf(stderr, "%% Failed to close consumer: %s\n",
                rd_kafka_err2str(err));
    }else{

        fprintf(stderr,"Consumer closed\n");
    }
    rd_kafka_topic_partition_list_destroy(topics);

    run = 5;
    while (run-- > 0 && rd_kafka_wait_destroyed(1000) == -1)
        printf("Waiting for librdkafka to decommission\n");
    if (run <= 0)
        rd_kafka_dump(stdout, rk);
    rd_kafka_destroy(rk);

    return 0;
}
