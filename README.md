version 0.1

# MDP队列kafka消费队列服务MKC。

# 为什么存在？
很简单，因为绝大多数队列消费服务都是用离线crontab脚本的形式
对队列服务进行消费，这种方式业务复杂的情况下很难维护，也很难
监测，非常不容易管理，为了能够有统一的消费方式，因此有了你。

# 怎样运行的？
你只是一个队列消费服务，生产方式保持不变，zookeeper和kafka服务
都可以，只要能入kafka队列就可以，监听kafka服务，读到数据时你
将拿到队列数据，该数据是json形式，每个队列数据中都有一个commandID，
命名为命令号，然后携带其他传递数据，格式如下
```
{
    commitId:1239120498,
    commandId:1000042,
    data : {
        ...... 
    }
}
```
拿到commandId后，会发起http请求,将commandId+元数据一同发给/commit接口
commit接口根据不同的commandId进行处理
# 怎么使用？
## 部署
### 安装librdkafka

https://github.com/edenhill/librdkafka

### 安装jansson

git clone https://github.com/akheron/jansson

autoreconf -i

./configure

make

make install
make && make install

### 安装 zookeeper --多线程版本已经弃用

wget http://apache.org/dist/zookeeper/zookeeper-3.4.9/zookeeper-3.4.9.tar.gz

tar zxvf ./zookeeper-3.4.9.tar.gz

cd ./zookeeper-3.4.9/src/c

./configure && make && make install

### 安装consumer

安装路径固定为 /usr/local/mkc/bin/mkc

git clone https://gitlab.meitu.com/mdp/mkc

cd mkc/

make

make install

./mkc -c server.conf

#### 信号模式 ,用于重启服务

./mkc -s stop|reload

# 配置相关

配置分为两层，server.conf和module.conf

## server.conf
```
#守护模式
daemonize on

#全局配置[TODO]
#...

#brokers 172.18.6.10:9094,172.18.6.10:9093,172.18.6.10:9097
brokers 172.18.6.10:9094,172.18.6.10:9093,172.18.6.10:9097

#只有高版本才支持group 0.9+，如果kafka server 版本更低，请不要配置此项
groupid  rdkafka_default

#设置broker.version.fallback 为低版本 ,需要librdkafka > 0.9
#fallback 0.8.2

#debug 模式 cgrp,topic,fetch

#kafka-debug all 

#topic 配置
property auto.offset.rest smallest
property offset.store.path  /web/kafka-consumer/mkc/offset
property offset.store.method file
property offset.store.sync.interval.ms 100


#配置路径
conf-path /web/kafka-consumer/mkc/conf

timeout 1000

#日志级别 warning notice error
log-level verbose 

#日志路径
log-file /web/kafka-consumer/mkc/logs/mkc.log

#进程ID目录
pid-path /web/kafka-consumer/mkc/


#topicName [每个consumer的进程个数,不要大于partition]
topic test  3
topic siren  2

filters 10303023
filters 10209392
filters 10102939 
filters 10103030
filters 11000001

module test-module.conf
module moduleA.conf
module moduleB.conf
module moduleC.conf


#数据库监控配置
mysql host 172.18.5.187
mysql port 3306
mysql user_name meitu
mysql password meitu
mysql db_name testdb

```
## module.conf
```

#模块名称
name test

#队列数据处理延迟时间，毫秒
delay 3000

#失败重试次数 0一直重试，它依赖HTTP的code，200为成功，500为失败
retrynum 0

#数据处理失败重试延迟时间
retry_num 3000

#提交方式 （默认&&推荐）
method post

#接收的命令号
filters 10303023
filters 10209392
filters 10102939

#接收命令号发起的请求接口
uri  http://delivery.meitu.com/commit/commit

```

# 性能压测

# 日后优化点

1、增加嵌入lua脚本功能，让消费数据更加灵活

2、完善日志及监控方式

3、日后支持非阻塞模式，从而加快消费速度，队列消费有两种情况

    A、 顺序强一致
        有些业务场景发送的命令号是有先后顺序的，这种情况就要保证强一致性

        如

    B、非一致性
        也有一些业务存在例外，数据与数据之间完全没有依赖关系，那就不必等待
        一条条执行，开启批量消费方式，加快消费速度
        如

4、增加人为干预干预机制
    在强一致情况下，某个命令因为数据错误（比如上游验证不严密）一直消费失败，那
    就会堵住后面队列消费的情况，而这种情况我们是已知的，该消息是可以忽略的，
    因此我们需要人工干预跳过这条消费继续执行后面的数据消费

5、增加zookeeper支持[done] #已经废弃

6、增加kafka消费的高级配置项 [done]

#注意

注意MKC使用的时候 要用 file_get_contents('php://input','r')方式获取输入流，因为 post请求传参不是key => val的，
是一个独立的完整的json串，$_POST拿不到或者拿到的是错误的数据，建议这里封装一个基类专门处理这块逻辑，然后定义
一个process使用

# bug fix.

20161201 修复传输数据大的情况下，导致内存异常的bug.

20170410 增加多进程，增加信号平滑控制机制

20170703 完善reload机制

20170901 支持topic多进程的consumer,提升消费性能

...

