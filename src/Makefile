
PREFIX?=/usr/local/mkc
INSTALL_BIN=$(PREFIX)/bin
INSTALL=install

CONSUMER_NAME=mkc
$(CC)=gcc;

all : main.o http.o config.o cjson.o sds.o zmalloc.o hash.o list.o logger.o process.o kafka.o
	$(CC) -g -o $(CONSUMER_NAME) main.o http.o config.o cJSON.o sds.o hash.o list.o zmalloc.o logger.o process.o kafka.o -lrdkafka -lz -lpthread -lm -lzookeeper_mt -ljansson
main.o: main.c consumer.h mkc.h
	$(CC)  -c -g  main.c
http.o: http.c http.c consumer.h
	$(CC)  -c -g  http.c
config.o: config.c config.h consumer.h
	$(CC)  -c -g  config.c
cjson.o: cJSON.c cJSON.h consumer.h
	$(CC)  -c -g  cJSON.c
sds.o: sds.c sds.h consumer.h
	$(CC)  -c -g  sds.c
hash.o: hash.c hash.h consumer.h
	$(CC)  -c -g  hash.c
list.o: list.c list.h consumer.h
	$(CC)  -c -g  list.c
zmalloc.o: zmalloc.c zmalloc.h
	$(CC)  -c -g  zmalloc.c
logger.o: logger.c logger.h
	$(CC) -c -g logger.c
process.o: process.c process.h
	$(CC) -c -g process.c
kafka.o: kafka.c kafka.h
	$(CC) -c -g kafka.c
install:all
	@mkdir -p $(INSTALL_BIN)

	cp $(CONSUMER_NAME) $(INSTALL_BIN)/$(CONSUMER_NAME)
clean:
	rm -rf $(CONSUMER_NAME) main.o http.o config.o cJSON.o sds.o hash.o list.o zmalloc.o logger.o process.o kafka.o

