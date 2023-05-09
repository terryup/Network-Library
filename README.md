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

## 开发环境

- 操作系统：`Ubuntu 18.04.2`
- 编译器：`g++ 7.5.0`
- 编译器：`vscode`
- 版本控制：`git`
- 项目构建：`cmake 3.10.2 `

## 并发模型

![](/Users/zixuanhuang/Desktop/webserver_explain/68747470733a2f2f63646e2e6e6c61726b2e636f6d2f79757175652f302f323032322f706e672f32363735323037382f313636333332343935353132362d33613830373866652d663237312d346131622d383263372d6237356564666633636461382e706e6723617665726167654875653d253233663465.png)

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

![](/Users/zixuanhuang/Desktop/webserver_explain/截屏2023-05-07 18.46.41.png)

## 项目讲解

