# C++ Network-Library

## 项目介绍

本项目是参照muduo实现的基于Reactor模型的多线程网络库。使用c++11的新特性编写，去除了muduo对boost的依赖，内部实现了一个HTTP服务器，可支持GET请求、POST请求和静态资源的访问，且负有异步日志监控服务端情况。

项目已经实现了Channel模块、Poller模块、事件循环模块、HTTP模块、定时器模块、异步日志模块。

## 项目特点

- 底层使用Epoll + LT模式的I/O复用模型，并且结合非阻塞I/O实现主从Reactor模型。
- 采用「one loop per thread」线程模型，并向上封装线程池避免线程的创建和销毁带来的性能开销。
- 采用eventfd作为事件通知描述符，方便高效派发事件到其他线程执行异步任务。
- 基于红黑树实现定时器的管理结构，内部使用Linux的timerfd通知到期任务，高效管理定时任务。
- 遵循RAII手法使用智能指针管理内存，减少内存泄漏风险
- 参照Tcmalloc实现了三级缓存内存池模块，更好管理小块的内存空间，减少内存碎片

## 开发环境

- 操作系统：`Ubuntu 18.04.2`
- 编译器：`g++ 7.5.0`
- 编译器：`vscode`
- 版本控制：`git`
- 项目构建：`cmake 3.10.2 `

## 并发模型

![](https://github.com/terryup/Network-Library/blob/master/explain/68747470733a2f2f63646e2e6e6c61726b2e636f6d2f79757175652f302f323032322f706e672f32363735323037382f313636333332343935353132362d33613830373866652d663237312d346131622d383263372d6237356564666633636461382e706e6723617665726167654875653d253233663465.png)

## 构建项目

运行脚本，其会自动编译项目

```shell
sudo ./autobuild.sh
```

## 运行案例

这里以一个简单的回程服务器作为案例，`EchoServer`默认监听端口为`8080`。

```she
ubuntu% cd example 
ubuntu% ls
CMakeLists.txt	echoServer  Makefile  testserver  testserver.cc
ubuntu% ./echoServer 
```

```shel
ubuntu% nc 127.0.0.1 8080
hello
hello
^C
ubuntu% 
```

执行情况：

```she
2023/05/07 03:41:02func=updateChannel => fd=4 events=3 index=-1 
2023/05/07 03:41:02EventLoop created 0x7ffd244189f0 in thread 8391 
2023/05/07 03:41:02func=updateChannel => fd=5 events=3 index=-1 
2023/05/07 03:41:02func=updateChannel => fd=7 events=3 index=-1 
2023/05/07 03:41:02EventLoop 0x7ffd244189f0 start looping 
2023/05/07 03:41:02func=poll => fd total count:3 
2023/05/07 03:41:061 events happened 
2023/05/07 03:41:06channel handleEvent revents:1
2023/05/07 03:41:06TcpServer::newConnection [EchoServer-01] - new connection [EchoServer-01-127.0.0.1:8080#1] from 127.0.0.1:0
2023/05/07 03:41:06TcpConnection::ctor[EchoServer-01-127.0.0.1:8080#1]at fd = 8 
2023/05/07 03:41:06func=updateChannel => fd=8 events=3 index=-1 
2023/05/07 03:41:06connection UP : 127.0.0.1:0 
2023/05/07 03:41:06func=poll => fd total count:4 
2023/05/07 03:41:081 events happened 
2023/05/07 03:41:08channel handleEvent revents:1
2023/05/07 03:41:08func=poll => fd total count:4 
2023/05/07 03:41:111 events happened 
2023/05/07 03:41:11channel handleEvent revents:17
2023/05/07 03:41:11fd=8 state=3 
2023/05/07 03:41:11func=updateChannel => fd=8 events=0 index=1 
2023/05/07 03:41:11connection DOWN : 127.0.0.1:0 
2023/05/07 03:41:11TcpServer::removeConnectionInLoop [EchoServer-01] - connection EchoServer-01-127.0.0.1:8080#1
2023/05/07 03:41:11func=removeChannel => fd=8 
2023/05/07 03:41:11TcpConnection::dtor[EchoServer-01-127.0.0.1:8080#1]at fd = 8 state = 0 
2023/05/07 03:41:11func=poll => fd total count:3 
```

`http`模块有一个`http`服务器案例，也可以执行。其默认监听`8080`:

```she
ubuntu% cd src/http      
ubuntu% ls
CMakeLists.txt	    HttpResponse_test.cc  Makefile   WebServer.cc
HttpContext.cc	    HttpResponse_test.h   mian.cc    WebServer.h
HttpContext.h	    HttpServer_test.cc	  resources
HttpRequest_test.h  HttpServer_test.h	  WebServer
ubuntu% ./WebServer 
```

![](https://github.com/terryup/Network-Library/blob/master/explain/%E6%88%AA%E5%B1%8F2023-05-07%2018.46.41.png)

## 项目优化

### 增加内存池模块

增加了一个三级缓冲的内存池，内部实现对象池以及三层基数树优化PageCache的_pageId与span的映射关系。经过测试，效率在4个线程并发执行10轮次，每轮次申请 10000次时效率比malloc快快550%，而释放比free快29600%（因为使用了对象池管理），整体快了633%，效率非常高。

![]([/Users/zixuanhuang/Desktop/explain/截屏2023-05-20 16.33.04.png](https://github.com/terryup/Network-Library/blob/master/explain/%E6%88%AA%E5%B1%8F2023-05-20%2016.33.04.png))

## 项目讲解

[Channel模块](https://github.com/terryup/Network-Library/blob/master/explain/Channel.md)

[Poller模块](https://github.com/terryup/Network-Library/blob/master/explain/Poller.md)

[Event Loop模块](https://github.com/terryup/Network-Library/blob/master/explain/EventLoop.md)

[ThreadPool模块](https://github.com/terryup/Network-Library/blob/master/explain/ThreadPool.md)

[Acceptor模块](https://github.com/terryup/Network-Library/blob/master/explain/Acceptor.md)

[Buffer模块](https://github.com/terryup/Network-Library/blob/master/explain/Buffer.md)

[TcpConnection模块](https://github.com/terryup/Network-Library/blob/master/explain/TcpConnection.md)

[TcpServer模块](https://github.com/terryup/Network-Library/blob/master/explain/TcpServer.md)



