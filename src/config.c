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
#include "config.h"
#include "hash.h"
#include "list.h"
#include "zmalloc.h"
#include "logger.h"

#include "librdkafka/rdkafka.h"

extern server_conf_t *server_conf;

void module_conf_free(void *module_conf);
/* *
 * @desc 解析服务配置
 * */
int parse_server_conf(char *file_name){

    char line[1024] = {0};
    int totalline = 0, i = 0 ,j = 0;
    FILE *fp    =   fopen(file_name,"r");

    if(!fp){
	    fprintf(stderr,"fopen file [%s] %s",file_name,strerror(errno));
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

            server_conf->brokers = sdscat(server_conf->brokers,vector[1]);
            server_conf->brokers = sdscat(server_conf->brokers,",");

        }else if(!strcasecmp(vector[0],"log-file")){

            server_conf->logfp = 0;
            FILE *fp = fopen(server_conf->logfile,"a+"); 

            if(!fp){
                //todo 待完善
                fprintf(stderr, "open log-file[%s] error :%s",server_conf->logfile,strerror(errno));
                exit(0);
            }
            server_conf->logfile = sdsdup(vector[1]);
            //fclose(fp);
            server_conf->logfp = fp;

        }else if(!strcasecmp(vector[0],"daemonize")){

            server_conf->daemonize = 0;
            if(!strcasecmp(vector[1],"on")){

               server_conf->daemonize = 1; 
            }
        }else if(!strcasecmp(vector[0],"conf-path")){

            server_conf->confpath =  zstrdup(vector[1]);

        }else if(!strcasecmp(vector[0],"pid-path")){

            server_conf->pidpath = zstrdup(vector[1]);
        }else if(!strcasecmp(vector[0],"pid-file")){

            server_conf->pidfile = zstrdup(vector[1]);

        }else if(!strcasecmp(vector[0],"timeout")){

            server_conf->timeout = atoi(vector[1]);
        }else if(!strcasecmp(vector[0],"log-level")){

            if(!strcasecmp(vector[1],"warning")){

                server_conf->loglevel    =   MKC_LOG_WARNING;
            }else if(!strcasecmp(vector[1],"notice")){

                server_conf->loglevel    =   MKC_LOG_NOTICE;
            }else if(!strcasecmp(vector[1],"error")){

                server_conf->loglevel    =   MKC_LOG_ERROR;
            }else {

                server_conf->loglevel    =   MKC_LOG_WARNING;
            }
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

            list_add_node_tail(server_conf->topics,vector[1],topic);

        }else if(!strcasecmp(vector[0],"filters")){

            int command = atoi(vector[1]);

            list_add_node_tail(server_conf->commands,zstrdup(vector[1]),(void*)zstrdup(vector[1]));

            if(!hash_find(server_conf->modules,vector[1],strlen(vector[1]))){
                mkc_write_log(MKC_LOG_NOTICE, "add filters [%s]",vector[1]);
            }
        }else if(!strcasecmp(vector[0],"module")){

            int command = atoi(vector[1]);
            module_conf_t *module= parse_module_conf(vector[1]);

            if(module != NULL){
                //server_conf->cmd_t = hash_add(server_conf->cmd_t,module->name,module,module_conf_free);
            }
        }else if(!strcasecmp(vector[0],"groupid")){

            server_conf->groupid=   zstrdup(vector[1]);

        }else if(!strcasecmp(vector[0],"fallback")){

            server_conf->fallback =   zstrdup(vector[1]);

        }else if(!strcasecmp(vector[0],"kafka-debug")){

            server_conf->kafkadebug =   zstrdup(vector[1]);
        }else if(!strcasecmp(vector[0],"mysql")){

            if(!strcasecmp(vector[1],"host")){

                server_conf->mysql->host = zstrdup(vector[2]);
            }else if(!strcasecmp(vector[1],"port")){

                server_conf->mysql->port= atoi(vector[2]);
            }else if(!strcasecmp(vector[1],"user_name")){

                server_conf->mysql->user_name= zstrdup(vector[2]);
            }else if(!strcasecmp(vector[1],"password")){

                server_conf->mysql->password = zstrdup(vector[2]);
            }else if(!strcasecmp(vector[1],"db_name")){

                server_conf->mysql->db_name = zstrdup(vector[2]);

            }
        }else if(!strcasecmp(vector[0],"property")){

            if(vector[1] && vector[2]){

                list_add_node_tail(server_conf->properties,zstrdup(vector[1]) ,zstrdup(vector[2]));
            }

        }
        sdsfreesplitres(vector,argc);
    }
    if(!server_conf->confpath){

        mkc_write_log(MKC_LOG_ERROR,"there is no confpath in server conf.");
        exit(1);
    }
    if(server_conf->commands->len == 0){

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

    sprintf(module_conf_file,"%s/%s",server_conf->confpath,filename);

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

    //默认重试延迟时间
    module_conf->retry_delay = 1000;

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
            if(list_find_node(server_conf->commands,vector[1])){

                mkc_write_log(MKC_LOG_NOTICE, "parse moduelconf %s",vector[1]);

                /*
                list *node = hash_find(server_conf->modules,vector[1],strlen(vector[1]));
                if(!node){

                    printf("command num [%s] not exists in server.conf\n",vector[1]);
                    continue;
                }
                */

                hash_add(server_conf->modules,vector[1],(void*)module_conf, NULL);
            }

        }else if(!strcasecmp(vector[0],"retry_delay")){

            module_conf->retry_delay = atoi(vector[1]);

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
    if(server_conf->modules->element_num == 0){

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
