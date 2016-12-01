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

### 安装 zookeeper

wget http://apache.org/dist/zookeeper/zookeeper-3.4.9/zookeeper-3.4.9.tar.gz

tar zxvf ./zookeeper-3.4.9.tar.gz

cd ./zookeeper-3.4.9/src/c

./configure && make && make install

### 安装consumer

git clone https://gitlab.meitu.com/mdp/mkc

cd mkc/

make

make install

# 配置相关

配置分为两层，server.conf和module.conf

## server.conf
```

#守护模式
daemonize on

#zookeeper 主机名称和端口名

zookeeper 172.18.6.10:2181

#配置路径
confpath /web/kafka-consumer/consumer/conf

#监听kafka最大的阻塞时间,ms
timeout 100

#开启debug模式
debug yes

#日志级别
loglevel verbose 

#日志路径
logfile /web/kafka-consumer/consumer/logs/mkc.log

#监听的topic

topic imsiren
topic siren
topic memcachedelete

#要消费的命令号，注意，这里配置了module里面才会生效，只在module配置是不成效的
filters 10303023
filters 10209392
filters 10102939 

#加载模块配置，支持多个

module moduleA.conf
module moduleB.conf
module moduleC.conf
module moduleD.conf

```
## module.conf
```

#模块名称
name test

#延迟时间，毫秒
delay 3000

#失败重试次数 0一直重试，它依赖HTTP的code，200为成功，500为失败
retrynum 0

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

5、增加zookeeper支持[done]

6、增加kafka消费的高级配置项

# bug fix.
20161201 修复传输数据大的情况下，导致内存异常的bug.
...

