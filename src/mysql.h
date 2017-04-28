#ifndef _MKC_MYSQL_H_
#define _MKC_MYSQL_H_

#include <mysql.h>
#include "config.h"

extern server_conf_t server_config;

#define REPLACE_COMMAND "REPLACE INTO mkc_queue_log SET commit_id = %d, command_id = %d, content = '%s', status = %d, retry_num = %d, gmt_create = %d,gmt_update = %d"
#define INSERT_COMMAND "INSERT INTO mkc_queue_log (commit_id,command_id,content,status,retry_num,gmt_create,gmt_update) VALUES (%d,%d,'%s',%d, %d, %d, %d)"
#define UPDATE_COMMAND "UPDATE mkc_queue_log SET retry_num = %d,gmt_update = %d  WHERE commit_id = %d AND  command_id = %d AND status = %d"
#define SELECT_COMMAND "SELECT * FROM mkc_queue_log WHERE commit_id = %d AND  command_id = %d"
#define IS_SKIP_COMMAND "SELECT is_skip FROM mkc_queue_log WHERE commit_id = %d AND command_id = %d AND status=1 AND is_skip=1"

MYSQL *mkc_mysql_init(MYSQL *conn);
void mkc_mysql_close(MYSQL *conn);

void mkc_mysql_ping(MYSQL *conn);
int mkc_mysql_exec(MYSQL *conn,const char *sql);

int save_mkc_queue_log(MYSQL *conn, int commit_id, int command_id, char *content, int status, int retry_num);
int update_mkc_queue_log(MYSQL *conn, int commit_id, int command_id, int status, int retry_num);
int insert_mkc_queue_log(MYSQL *conn,int commit_id,int command_id,char *content,int status,int retry_num);
int select_mkc_queue_log(MYSQL *conn, int commit_id, int command_id);
int mkc_commitid_is_skiped(MYSQL *conn,int commit_id, int command_id);
#endif
