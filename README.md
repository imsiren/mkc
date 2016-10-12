# MDP队列kafka消费队列服务

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
    commandId:1000042,
    data : {
        ...... 
    }
}
```
拿到commandId后，会发起http请求,将commandId+元数据一同发给/commit接口
commit接口根据不同的commandId进行处理
# 怎么使用？

# 配置相关

配置分为两层，server.conf和module.conf
## server.conf
```

#守护模式
daemonize on

#配置路径
confpath /Users/wushuai/Web/kafka-consumer/consumer/conf

#监听kafka最大的阻塞时间,ms
timeout 100

#开启debug模式
debug yes

#日志级别
loglevel verbose 

#日志路径
logfile /web/nmq/logs/nmq.log

#队列数据记录日志
append-queue-file /web/nmq/logs/queue.log

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
module moduleA.conf
module moduleA.conf
module moduleA.conf


```
## module.conf
```

#模块名称
name test

#延迟时间，毫秒
delay 3000

#失败重试次数 0一直重试，它依赖HTTP的code，200为成功，500为失败
retrynum 0

#提交方式
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
