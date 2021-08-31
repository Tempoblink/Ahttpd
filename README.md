## 面临的问题

- [ ] 目前仅支持HTTP/1.0的短连接，待支持长连接
- [ ] log日志并发性的问题待解决
- [ ] 选择使用了kevent处理事件监听，待追加epoll模式
- [ ] HTTP头的状态检测只写了简单的业务逻辑，待利用有限状态机框架重构。



## 初衷

阅读完apue后需要做个东西消化一下知识。利用线程池和I/O多路转接实现半同步/半反应堆模型，对网络socket库的简单再封装。



## 压力测试

利用Webbench1.5进行的测试，平台CPU: Intel i5-5287U (4) @ 2.90GHz。

```bash
Copyright (c) Radim Kolar 1997-2004, GPL Open Source Software.

Request:
GET / HTTP/1.0
User-Agent: WebBench 1.5
Host: localhost
Connection: close


Runing info: 10 clients, running 30 sec.

Speed=32760 pages/min, 152334 bytes/sec.
Requests: 16380 susceed, 0 failed.
```


