/*
 * =====================================================================================
 *
 *       Filename:  config.c
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2016/05/20 15时53分50秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  YOUR NAME (), 
 *   Organization:  
 *
 * =====================================================================================
 */
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "sds.h"
#include "consumer.h"
#include "config.h"
#include "hash.h"
#include "list.h"
#include "zmalloc.h"
#include "logger.h"

#include "librdkafka/rdkafka.h"

void module_conf_free(void *module_conf);
/* *
 * @desc 解析服务配置
 * */
int parse_server_conf(char *file_name){

    char line[1024] = {0};
    int totalline = 0, i = 0 ,j = 0;
    FILE *fp    =   fopen(file_name,"r");

    if(!fp){
	    mkc_write_log(MKC_LOG_ERROR,"fopen file [%s] %s",file_name,strerror(errno));
        return -1;
    }
    sds config   =   sdsnew("");

    while((fgets(line,1024,fp)) != NULL){

        config  =   sdscat(config,line);   
    }
    fclose(fp);

    sds *lines  =   sdssplitlen(config,strlen(config),"\n",1,&totalline);
    sds *vector;
    int argc = 0,cmd_num = 1;
    //module_cmd_t **cmd_t;

    for(i = 0;i < totalline ;i ++){
        argc = 0; 
        if(lines[i][0] == '#' || lines[i][0] == '\0'){
            continue;
        }
        vector = sdssplitargs(lines[i],&argc);
        if(argc == 0){
            continue;
        }
        if(!strcasecmp(vector[0],"brokers")){

            server_config.brokers = sdscat(server_config.brokers,vector[1]);
            server_config.brokers = sdscat(server_config.brokers,",");

        }else if(!strcasecmp(vector[0],"log-file")){

            FILE *fp = fopen(server_config.logfile,"a+"); 

            if(!fp){
                //todo 待完善
                mkc_write_log(MKC_LOG_ERROR,"open log-file[%s] error :%s",server_config.logfile,strerror(errno));
                continue;
            }
            server_config.logfile = sdsdup(vector[1]);
            fclose(fp);

        }else if(!strcasecmp(vector[0],"daemonize")){
            server_config.daemonize = 0;
            if(!strcasecmp(vector[1],"on")){
               server_config.daemonize = 1; 
            }
        }else if(!strcasecmp(vector[0],"conf-path")){

            server_config.confpath =  zstrdup(vector[1]);

        }else if(!strcasecmp(vector[0],"pid-path")){

            server_config.pidpath = zstrdup(vector[1]);
        }else if(!strcasecmp(vector[0],"pid-file")){

            server_config.pidfile = zstrdup(vector[1]);

        }else if(!strcasecmp(vector[0],"timeout")){

            server_config.timeout = atoi(vector[1]);
        }else if(!strcasecmp(vector[0],"log-level")){

            if(!strcasecmp(vector[1],"warning")){

                server_config.loglevel    =   MKC_LOG_WARNING;
            }else if(!strcasecmp(vector[1],"notice")){

                server_config.loglevel    =   MKC_LOG_NOTICE;
            }else if(!strcasecmp(vector[1],"error")){

                server_config.loglevel    =   MKC_LOG_ERROR;
            }else {

                server_config.loglevel    =   MKC_LOG_WARNING;
            }
            /*  
        }else if(!strcasecmp(vector[0],"queuelogfile")){

            server_config.queuelogfile =   zstrdup(vector[1]);

        }else if(!strcasecmp(vector[0],"append-queue-file")){

            server_config.appendqueuelog   =   atoi(vector[1]);
            */

        }else if(!strcasecmp(vector[0],"topic")){


            mkc_topic *topic = zmalloc(sizeof(mkc_topic));

            topic->partition = RD_KAFKA_PARTITION_UA;
            topic->offset = 0;

            topic->name = sdsnew(vector[1]);

            if(argc == 3){

                topic->partition = atoi(vector[2]);
            }
            if(argc == 4){
                topic->offset = atoll(vector[3]);
            }

            list_add_node_tail(server_config.topics,vector[1],topic);

        }else if(!strcasecmp(vector[0],"filters")){

            int command = atoi(vector[1]);

            list_add_node_tail(server_config.commands,zstrdup(vector[1]),(void*)zstrdup(vector[1]));

            if(!hash_find(server_config.modules,vector[1],strlen(vector[1]))){
                mkc_write_log(MKC_LOG_NOTICE, "add filters [%s]",vector[1]);
            }
        }else if(!strcasecmp(vector[0],"module")){

            int command = atoi(vector[1]);
            module_conf_t *module= parse_module_conf(vector[1]);

            if(module != NULL){
                //server_config.cmd_t = hash_add(server_config.cmd_t,module->name,module,module_conf_free);
            }
        }

        sdsfreesplitres(vector,argc);
    }
    if(!server_config.confpath){

        mkc_write_log(MKC_LOG_ERROR,"there is no confpath in server conf.");
        exit(1);
    }
    if(server_config.commands->len == 0){

        mkc_write_log(MKC_LOG_ERROR,"there is no filters num in conf.");

        return -1;
    }
    sdsfree(config);
    sdsfreesplitres(lines,totalline);
    return 0;
}

/* *
 * @desc 解析模块配置
 * */
module_conf_t *parse_module_conf(const char *filename){

    char module_conf_file[1024] = {0};

    sprintf(module_conf_file,"%s/%s",server_config.confpath,filename);

    mkc_write_log(MKC_LOG_NOTICE,"load module conf[%s]",module_conf_file);
    FILE *fp     =  fopen(module_conf_file,"r");

    if(!fp){
        //    mmqLog(LOG_WARNING,"module conf file [%s] open failed.",module_conf_file);
        mkc_write_log(MKC_LOG_ERROR,"module conf file [%s] open failed.",module_conf_file);
        exit(1);
    }

    char buf[1024]  =   {0};
    int totalline    =   0,i;
    sds config  ,*lines,*vector;
    config =   sdsnew("");

    while((fgets(buf,1024,fp))){

        config  =   sdscat(config,buf);
    }

    fclose(fp);

    lines   =   sdssplitlen(config,sdslen(config),"\n",1,&totalline);

    int argc = 0,cmd_num = 0;

    module_conf_t  *module_conf    =   zmalloc(sizeof(module_conf_t));

    memset(module_conf,0,sizeof(module_conf_t));

    if(!module_conf){

        return NULL;
    }
    for(i = 0;i < totalline; i++){

        //注释跳过
        if(lines[i][0] == '#' || lines[i][0] == '\n' || lines[i][0] == '\0' || lines[i][0] == ' '){

            continue;
        }
        vector = sdssplitargs(lines[i],&argc);

        if(!strcasecmp(vector[0],"delay")){

            module_conf->delay  =   atoi(vector[1]);

        }else if(!strcasecmp(vector[0],"retrynum")){

            module_conf->retrynum   =   atoi(vector[1]);

        }else if(!strcasecmp(vector[0],"name")){

            module_conf->name   =   zstrdup(vector[1]);

        }else if(!strcasecmp(vector[0],"uri")){

            module_conf->uri    =   zstrdup(vector[1]);

        }else if(!strcasecmp(vector[0],"filters")){


            /*  
            sds *commands =  zrealloc(module_conf->commands,(cmd_num + 1) * sizeof(sds));
            commands[cmd_num] = t;
            cmd_num++;
            module_conf->commands = commands;
            module_conf->command_len = cmd_num;
            */
            int command = atoi(vector[1]);
            if(list_find_node(server_config.commands,vector[1])){

                mkc_write_log(MKC_LOG_NOTICE, "parse moduelconf %s",vector[1]);

                /*
                list *node = hash_find(server_config.modules,vector[1],strlen(vector[1]));
                if(!node){

                    printf("command num [%s] not exists in server.conf\n",vector[1]);
                    continue;
                }
                */

                hash_add(server_config.modules,vector[1],(void*)module_conf, NULL);
            }

        }else if(!strcasecmp(vector[0],"method")){

            module_conf->method = zstrdup(vector[1]);
        }
        if(argc > 0){
            sdsfreesplitres(vector,argc);
        }
    }

    if(!module_conf->name){

        mkc_write_log(MKC_LOG_ERROR,"there is no name in conf file [%s]",module_conf_file);

        exit(1);
    }
    if(server_config.modules->element_num == 0){

        mkc_write_log(MKC_LOG_ERROR,"there is no filters in conf file [%s]",module_conf_file);

        exit(1);
    }

    sdsfree(config);
    sdsfreesplitres(lines,totalline);
    

    return module_conf;
}
void module_conf_free(void *module_conf){

    module_conf_t *conf_t = (module_conf_t*)module_conf;
    if(conf_t == NULL){

        return;
    }
    int len = conf_t->command_len;

    sdsfreesplitres(conf_t->commands,len);
    zfree(conf_t->uri);
    zfree(conf_t->name);
    zfree(conf_t->method);
    zfree(conf_t);
}
