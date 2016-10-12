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

#include <stdio.h>
#include <string.h>

#include "sds.h"

#include "consumer.h"
#include "config.h"
#include "hash.h"
#include "list.h"
#include "zmalloc.h"

void module_conf_free(void *module_conf);
/* *
 * @desc 解析服务配置
 * */
void parse_server_conf(char *file_name){

    char line[1024] = {0};
    int totalline = 0, i = 0 ,j = 0;
    FILE *fp    =   fopen(file_name,"r");

    if(!fp){
        //mmqLog(LOG_VERBOSE,"file of %s open failed.",file_name);
        fprintf(stderr, "file of %s open failed.",file_name);
        exit(1);
    }
    sds config   =   sdsnew("");

    while((fgets(line,1024,fp)) != NULL){

        config  =   sdscatlen(config,line,strlen(line));   
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
        if(!strcasecmp(vector[0],"port")){

            server_config.port = zstrdup(vector[1]);

        }else if(!strcasecmp(vector[0],"logfile")){
            FILE *fp = fopen(server_config.logfile,"a"); 
            if(!fp){
                //todo
            }
            fclose(fp);
            server_config.logfile = zstrdup(vector[1]);
        }else if(!strcasecmp(vector[0],"daemonize")){
            server_config.daemonize = 0;
            if(!strcasecmp(vector[1],"on")){
               server_config.daemonize = 1; 
            }
        }else if(!strcasecmp(vector[0],"confpath")){

            server_config.confpath =   zstrdup(vector[1]);

        }else if(!strcasecmp(vector[0],"timeout")){

            server_config.timeout = atoi(vector[1]);
        }else if(!strcasecmp(vector[0],"loglevel")){

            /* 
            if(!strcasecmp(vector[1],"warning")){
                server_config.loglevel    =   LOG_WARNING;
            }else if(!strcasecmp(vector[1],"notice")){
                server_config.loglevel    =   LOG_NOTICE;
            }else if(!strcasecmp(vector[1],"verbose")){
                server_config.loglevel    =   LOG_VERBOSE;
            }else if(!strcasecmp(vector[1],"debug")){
                server_config.loglevel    =   LOG_DEBUG;
            }else {
                server_config.loglevel    =   LOG_WARNING;
            }
            */
        }else if(!strcasecmp(vector[0],"debug")){

            server_config.debug = 0;

            if(!strcasecmp(vector[1],"yes")){

                server_config.debug = 1;
            }

        }else if(!strcasecmp(vector[0],"queuelogfile")){

            server_config.queuelogfile =   zstrdup(vector[1]);

        }else if(!strcasecmp(vector[0],"append-queue-file")){

            server_config.appendqueuelog   =   atoi(vector[1]);

        }else if(!strcasecmp(vector[0],"topic")){

            //server_config.topics = zstrdup(vector[1]);
            list_add_node_tail(server_config.topics,zstrdup(vector[1]),zstrdup(vector[1]));
        }else if(!strcasecmp(vector[0],"filters")){

            int command = atoi(vector[1]);

            list_add_node_tail(server_config.commands,zstrdup(vector[1]),(void*)zstrdup(vector[1]));

            if(!hash_find(server_config.modules,vector[1],strlen(vector[1]))){
                printf("add filters [%s]\n",vector[1]);
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

        fprintf(stderr,"there is no confpath in server conf.");
        exit(1);
    }
    if(server_config.commands->len == 0){

        fprintf(stderr,"there is no filters num in conf.");

        exit(1);
    }
    sdsfree(config);
    sdsfreesplitres(lines,totalline);
}

/* *
 * @desc 解析模块配置
 * */
module_conf_t *parse_module_conf(const char *filename){

    char module_conf_file[1024] = {0};

    sprintf(module_conf_file,"%s/%s",server_config.confpath,filename);

    FILE *fp     =  fopen(module_conf_file,"r");

    if(!fp){
        //    mmqLog(LOG_WARNING,"module conf file [%s] open failed.",module_conf_file);
        fprintf(stderr,"module conf file [%s] open failed.",module_conf_file);
        exit(1);
        return NULL;
    }

    char buf[1024]  =   {0};
    int totalline    =   0,i;
    sds config  ,*lines,*vector;
    config =   sdsnew("");

    while((fgets(buf,1024,fp))){

        config  =   sdscatlen(config,buf,strlen(buf));
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
        if(lines[i][0] == '#' || lines[i][0] == '\n' || lines[i][0] == '\0'){
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

                printf("parse moduelconf %s\n",vector[1]);

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
        sdsfreesplitres(vector,argc);
    }

    if(!module_conf->name){

        fprintf(stderr,"there is no name in conf file [%s]\n",module_conf_file);

        exit(1);
    }
    if(server_config.modules->element_num == 0){

        fprintf(stderr,"there is no filters in conf file [%s]\n",module_conf_file);

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
