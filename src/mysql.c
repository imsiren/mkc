#include <stdio.h>
#include <string.h>
#include "mysql.h"
#include "logger.h"

extern server_conf_t *server_conf;

void mkc_mysql_ping(MYSQL *conn){
    if(!mysql_ping(conn)){
        mkc_mysql_close(conn);
        mkc_mysql_init(conn);
    }
}

/*
MYSQL mkc_mysql_init(MYSQL conn){
    if (!mysql_init(&conn)) {
        printf("Error %u: %s\n", mysql_errno(&conn), mysql_error(&conn));
        exit(1);
    }
    if (mysql_real_connect(&conn, "172.18.5.187", "meitu",
          "meitu", "testdb", 0, NULL, 0) == NULL) {
        printf("Error %u: %s\n", mysql_errno(&conn), mysql_error(&conn));
        exit(1);
    } else {
        char value = 1; 
        mysql_options(&conn, MYSQL_OPT_RECONNECT, (char*)&value);
    }
    return conn;
}
*/

MYSQL* mkc_mysql_init(MYSQL *conn){
    if (!mysql_init(conn)) {
        mkc_write_log(MKC_LOG_ERROR,"Error %u: %s\n", mysql_errno(conn), mysql_error(conn));
        exit(1);
    }

    char value = 1;
    mysql_options(conn, MYSQL_OPT_RECONNECT, (char*)&value);

    if (mysql_real_connect(conn,
            server_conf->mysql->host,
            server_conf->mysql->user_name,
            server_conf->mysql->password,
            server_conf->mysql->db_name,
            server_conf->mysql->port,
            NULL, 0) == NULL) {
        mkc_write_log(MKC_LOG_ERROR, "Error %u: %s\n", mysql_errno(conn), mysql_error(conn));
        exit(1);
    }
    return conn;
}

int mkc_mysql_exec(MYSQL *conn,const char *sql) {
    int ret = mysql_real_query(conn, sql,strlen(sql));
    if(ret){
        mkc_write_log(MKC_LOG_ERROR , "c Error %u: %s\n", mysql_errno(conn), mysql_error(conn));
    }
    return ret;
}

int mkc_commitid_is_skiped(MYSQL *conn,int commit_id, int command_id){
    int ret = 0;
    MYSQL_RES *res;

    int len = strlen(IS_SKIP_COMMAND) + 64;
    char *sql = (char *)zmalloc(len);
    sprintf(sql,IS_SKIP_COMMAND,commit_id,command_id);
    mkc_mysql_exec(conn,sql);

    res = mysql_store_result(conn);
    int res_num = mysql_num_rows(res);
    if(res_num){
        ret = 1;
        mysql_free_result(res);
    }
    zfree(sql);
    return ret;
}

void mkc_mysql_close(MYSQL *conn){
    mysql_close(conn);
}

int insert_mkc_queue_log(MYSQL *conn, int commit_id, int command_id, char *content, int status, int retry_num,char *topic_name){
    int last_insert_id = 0;
    time_t now;
    time(&now);

    int len = strlen(INSERT_COMMAND) + 128;
    sds s_content = addslashes(content,strlen(content));
    char *sql = (char *)zmalloc(sdslen(s_content) + len); 

    sprintf(sql,INSERT_COMMAND,commit_id,command_id,s_content,status,retry_num,now,now,topic_name);
    int end = strlen(sql);
    sql[end+1] = '\0';

    mkc_write_log(MKC_LOG_NOTICE,sql);
    int ret = mkc_mysql_exec(conn,sql);

    if(!ret){

        last_insert_id = mysql_insert_id(conn);
    }
    sdsfree(s_content);
    zfree(sql);

    return last_insert_id;
}

int update_mkc_queue_log(MYSQL *conn, int commit_id, int command_id, int status){
    int num_rows = 0;
    time_t now;
    time(&now);
    int len = strlen(UPDATE_COMMAND) + 128;
    char *sql = (char *)zmalloc(len);
    sprintf(sql,UPDATE_COMMAND,now,commit_id,command_id,status);
    //mkc_write_log(MKC_LOG_NOTICE,sql);
    int ret = mkc_mysql_exec(conn,sql);
    if(!ret){
        num_rows = mysql_affected_rows(conn);
    }
    zfree(sql);
    return num_rows;
}

int save_mkc_queue_log(MYSQL *conn, int commit_id, int command_id, char *content, int status, int retry_num, char *topic_name){
    int ret;
    //mkc_write_log(MKC_LOG_NOTICE ,"mkc saved queue log.");
    if(select_mkc_queue_log(conn, commit_id, command_id) > 0){

        ret = update_mkc_queue_log(conn,commit_id,command_id,status);
    } else{

        ret = insert_mkc_queue_log(conn,commit_id,command_id,content,status,retry_num,topic_name);
    }
    return ret;
}

int select_mkc_queue_log(MYSQL *conn, int commit_id, int command_id){
    int res_num = 0;
    MYSQL_RES *res;

    int len = strlen(SELECT_COMMAND) + 64;
    char *sql = (char *)zmalloc(len);
    sprintf(sql,SELECT_COMMAND,commit_id,command_id);
    mkc_mysql_exec(conn,sql);

    res = mysql_store_result(conn);
    res_num = mysql_num_rows(res);
    mysql_free_result(res);
    zfree(sql);
    return res_num;
}
sds addslashes(char *src , int len){

    char *target, *source, *end;


    sds new_str = sdsnewlen("", len * 2);

    end = src + len;

    target = new_str;

    source = src;

    while(source < end){
        switch(*source){
            case '\0':
                *target++ = '\\';
                *target++ = '0';
                break;
            case '\'':
            case '\"':
            case '\\':
                *target++ = '\\';
            default:
                *target++ = *source;
                break;
        }
        source ++;
    }

    *target = '\0';
    return new_str;
}
