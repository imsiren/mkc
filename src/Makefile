
PREFIX?=/usr/local/rdkafka-consumer
INSTALL_BIN=$(PREFIX)/bin
INSTALL=install

CONSUMER_NAME=consumer

all : main.o http.o config.o cjson.o sds.o zmalloc.o hash.o list.o
	cc -g -o $(CONSUMER_NAME) main.o http.o config.o cJSON.o sds.o hash.o list.o zmalloc.o -lrdkafka -lz -lpthread -lm
main.o: main.c consumer.h
	cc  -c -g  main.c
http.o: http.c http.c consumer.h
	cc  -c -g  http.c
config.o: config.c config.h consumer.h
	cc  -c -g  config.c
cjson.o: cJSON.c cJSON.h consumer.h
	cc  -c -g  cJSON.c
sds.o: sds.c sds.h consumer.h
	cc  -c -g  sds.c
hash.o: hash.c hash.h consumer.h
	cc  -c -g  hash.c
list.o: list.c list.h consumer.h
	cc  -c -g  list.c
zmalloc.o: zmalloc.c zmalloc.h
	cc  -c -g  zmalloc.c
install:all
	@mkdir -p $(INSTALL_BIN)

	cp $(CONSUMER_NAME) $(INSTALL_BIN)/$(CONSUMER_NAME)
clean:
	rm -rf $(CONSUMER_NAME) main.o http.o config.o cJSON.o sds.o hash.o list.o zmalloc.o
