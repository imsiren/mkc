
PREFIX?=/usr/local/mkc
INSTALL_BIN=$(PREFIX)/bin
INSTALL=install


CONSUMER_NAME=mkc

$(CC)=gcc;

MYSQL= -I/usr/local/mysql/include -L/usr/local/mysql/lib/ -lmysqlclient


all : mysql.o setproctitle.o main.o http.o config.o cjson.o sds.o zmalloc.o hash.o list.o logger.o process.o kafka.o 
	$(CC) -g -o $(CONSUMER_NAME) setproctitle.o main.o http.o config.o cJSON.o sds.o hash.o list.o zmalloc.o logger.o process.o kafka.o mysql.o -lrdkafka -lz -lpthread -lm -ljansson $(MYSQL)
main.o: main.c  mkc.h 
	$(CC)  -c -g  main.c $(MYSQL)
http.o: http.c http.c  
	$(CC)  -c -g  http.c $(MYSQL)
config.o: config.c config.h 
	$(CC)  -c -g  config.c $(MYSQL)
cjson.o: cJSON.c cJSON.h 
	$(CC)  -c -g  cJSON.c
sds.o: sds.c sds.h 
	$(CC)  -c -g  sds.c
hash.o: hash.c hash.h 
	$(CC)  -c -g  hash.c
list.o: list.c list.h 
	$(CC)  -c -g  list.c
zmalloc.o: zmalloc.c zmalloc.h
	$(CC)  -c -g  zmalloc.c $(MYSQL)
logger.o: logger.c logger.h
	$(CC) -c -g logger.c $(MYSQL)
process.o: process.c process.h
	$(CC) -c -g process.c $(MYSQL)
kafka.o: kafka.c kafka.h
	$(CC) -c -g kafka.c $(MYSQL)
setproctitle.o:
	$(CC) -c -g setproctitle.c
mysql.o: mysql.c mysql.h
	$(CC) -c -g mysql.c $(MYSQL)
install:all
	@mkdir -p $(INSTALL_BIN)

	cp $(CONSUMER_NAME) $(INSTALL_BIN)/$(CONSUMER_NAME)
clean:
	rm -rf $(CONSUMER_NAME) main.o http.o config.o cJSON.o sds.o hash.o list.o zmalloc.o logger.o process.o kafka.o setproctitle.o mysql.o

