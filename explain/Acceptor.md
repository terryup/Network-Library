# Acceptor

## InetAddress类

`InetAddress`类是一个用于表示IP地址和端口号的类。他只是吧我们平时网络编程的时候的socket地址类型给封装了一下，具体见代码：

```c++
class InetAddress : public copyable{
public:
    explicit InetAddress(uint16_t port = 0, std::string ip = "127.0.0.1");
    explicit InetAddress(const sockaddr_in &addr)
        : addr_(addr){}
    
    std::string toIp() const;
    std::string toIpPort() const;
    uint16_t toPort() const;

    const sockaddr_in* getSockAddr() const {return &addr_;}
    void setSockAddr(const sockaddr_in &addr) {addr_ = addr;}

private:
    sockaddr_in addr_;
};
```

`explicit InetAddress(uint16_t port = 0, std::string ip = "127.0.0.1")`：是一个构造函数，他可以通过传入一个端口号和一个IP地址字符串来构造一个 `InetAddress` 对象。我们给他默认的IP地址设置为`127.0.0.1`也就是本机地址。

```c++
InetAddress::InetAddress(uint16_t port, std::string ip){
    bzero(&addr_, sizeof(addr_));
    addr_.sin_family = AF_INET;
    addr_.sin_port = ::htons(port);
    addr_.sin_addr.s_addr = inet_addr(ip.c_str());
}
```

剩下的这些函数都是对网络地址信息 `sockaddr_in` 类型进行处理和转换的成员函数。

```c++
std::string InetAddress::toIp() const{
    //  addr_
    char buf[64] = {0};
    ::inet_ntop(AF_INET, &addr_.sin_addr, buf, sizeof(buf));
    return buf;
}

std::string InetAddress::toIpPort() const{
    //  ip:port
    char buf[64] = {0};
    ::inet_ntop(AF_INET, &addr_.sin_addr, buf, sizeof(buf));
    size_t end = strlen(buf);
    uint16_t port = ntohs(addr_.sin_port);
    sprintf(buf + end, ":%u", port);
    return buf;

}

uint16_t InetAddress::toPort() const{
    return ::ntohs(addr_.sin_port);
}
```

## Socket类

`Socket` 类是一个封装了套接字文件描述符 `sockfd_` 的类，提供了与 TCP/IP 套接字相关的接口。

这里有个Nagel算法会有点陌生，这个算法通过将一定时间内需要传输的多个小数据包合并成一个更大的数据包，从而减少因TCP/IP协议本身引起的网络负载，提高网络的利用率。具体的可以单独查阅。

```c++
class Socket : noncopyable{
public:
    explicit Socket(int sockfd) 
        : sockfd_(sockfd){}
    
    ~Socket();

    //  获取sockfd
    int fd() const { return sockfd_; }
    //  绑定sockfd
    void bindAddress(const InetAddress& localaddr);
    //  使sockfd为可接受连接状态
    void listen();
    //  接受连接
    int accept(InetAddress *peeraddr);

    //  设置半关闭
    void shutdownWrite();

    void setTcpNoDelay(bool on); //  设置Nagel算法 
    void setReuseAddr(bool on); //  设置地址复用
    void setReusePort(bool on); //  设置端口复用
    void setKeepAlive(bool on); //  设置长连接
   
private:
    const int sockfd_;
};
```

这里几个成员函数比较常规，仅贴代码：

```c++
void Socket::bindAddress(const InetAddress& localaddr){
    if (bind(sockfd_, (sockaddr*)localaddr.getSockAddr(), sizeof(sockaddr_in)) != 0){
        LOG_FAT("bind sockfd:%d fail \n", sockfd_);
    }
}

void Socket::listen(){
    if (::listen(sockfd_, 1024) != 0){
        LOG_FAT("listen sockfd:%d fail \n", sockfd_);
    }
}

int Socket::accept(InetAddress *peeraddr){
    /*
        1.accept函数的参数不合法
        2.对返回的connfd没有设置非阻塞
        Reactor模型 one loop per thread
        poller + non-blockingio
    */
    sockaddr_in addr;
    socklen_t len = sizeof(addr);
    bzero(&addr, sizeof(addr));
    int connfd = ::accept4(sockfd_, (sockaddr*)&addr, &len, SOCK_NONBLOCK | SOCK_CLOEXEC);
    if (connfd >= 0){
        peeraddr->getSockAddr();
    }
    return connfd;
}
```

##### shutdownWrite函数

这个函数其实就是用到了半关闭的技术。半关闭指的是关闭连接的其中一端的写操作，使得另一端仍然能够接收数据。在半关闭之后，连接的另一端可以继续发送数据，但是已经关闭的一端不再接收数据。`Socket::shutdownWrite()`就是将`sockfd_`所代表的连接的写端关闭，使得另一端仍能接收数据。

而为什么要留有半关闭这个函数？

半关闭技术可以用来解决TCP中的粘包问题。项目里的`TcpConnection`类中就使用了半关闭技术来解决粘包问题。当一个TCP连接的一端完成发送数据后，可以调用`Socket::shutdownWrite()`方法，关闭TCP连接的写一半，从而向另一端发送EOF。另一端接收到EOF后，仍然可以从套接字中读取数据，但是已经无法从此套接字中再发送数据。通过这种方式，可以在不关闭TCP连接的情况下，让接收端判断出一条消息已经传输完毕，从而解决粘包问题。

```c++
void Socket::shutdownWrite(){
    //  关闭写端，但是可以接受客户端数据
    if (::shutdown(sockfd_, SHUT_WR) < 0){
        LOG_ERR("sockets::shutdownWrite error!");
    }
}
```

剩下的成员函数：

```c++
//  不启动Nagle算法
void Socket::setTcpNoDelay(bool on){
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval));

}

//  设置地址复用，其实就是可以使用处于Time-wait的端口
void Socket::setReuseAddr(bool on){
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
}

//  通过改变内核信息，多个进程可以绑定同一个地址。通俗就是多个服务的ip+port是一样
void Socket::setReusePort(bool on){
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
}

void Socket::setKeepAlive(bool on){
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval));
}
```

## Acceptor类

`Acceptor`类主要实现了服务端的监听功能，接收客户端的连接请求。

##### 成员变量解释如下：

- `*loop_`：用户定义的 `baseLoop_` 对象
- `acceptSocket_`：监听套接字对象，用于接收连接请求
- `acceptChannel_`：描述 `acceptSocket_` 套接字的事件通道对象
- `newConnectionCallback_`：新连接到来时的回调函数
- `listenning_`：表示当前 `Acceptor` 对象是否正在监听连接请求的标志位

```c++
EventLoop *loop_; 
Socket acceptSocket_;
Channel acceptChannel_;
NewConnectionCallback newConnectionCallback_;
bool listenning_;
```

##### 构造函数：

这里`acceptSocket_`用了`createNonbloking()`函数进行初始化：

`AF_INET`：创建 IPv4 的套接字

`SOCK_STREAM`：创建流套接字

`SOCK_NONBLOCK`：创建的套接字非阻塞

`SOCK_CLOEXEC`：执行 exec 系统调用时会自动关闭套接字

```c++
static int createNonbloking(){
    int sockfd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_TCP);
    if (sockfd < 0){
        LOG_FAT("listen socket create err:%d \n", errno);
    }
    return sockfd;
}
```

```c++
Acceptor::Acceptor(EventLoop *loop, const InetAddress &listenAddr, bool reuseport)
    : loop_(loop)
    , acceptSocket_(createNonbloking())
    , acceptChannel_(loop, acceptSocket_.fd())
    , listenning_(false) {
    acceptSocket_.setReuseAddr(true);
    acceptSocket_.setReusePort(true);
    acceptSocket_.bindAddress(listenAddr);  //  bind绑定套接字
    //  TcpServer::start()  Acceptor.listen
    //  如果有新用户的连接，要执行一个回调(connfd => channel => ubloop)
    //  baseLoop_ => acceptChannel_(listenfd) => 
    acceptChannel_.setReadCallback(std::bind(&Acceptor::handleRead, this));

}
```

剩下的基本也跟我们平时网络编程的东西大差不差，只是进行了一层封装。

```c++
Acceptor::~Acceptor(){
    //  把从Poller中感兴趣的事件删除掉
    acceptChannel_.disableAll();
    //  调用EventLoop->removeChannel => Poller->removeChannel 把Poller的ChannelMap对应的部分删除
    acceptChannel_.remove();
}

void Acceptor::listen(){
    listenning_ = true;
    acceptSocket_.listen();
    //  将acceptChannel的读事件注册到poller
    acceptChannel_.enableReading();
}

//  listenfd有事件发生了，就是有新用户连接了
void Acceptor::handleRead(){
    InetAddress peerAddr;
    //  接受新连接
    int connfd = acceptSocket_.accept(&peerAddr);
    //  确实有新连接到来
    if (connfd >= 0){
        //  TcpServer::NewConnectionCallback_
        if (newConnectionCallback_){
            //  轮询找到subLoop 唤醒并分发当前的新客户端的Channel
            newConnectionCallback_(connfd, peerAddr);
        }
        else{
            ::close(connfd);
        }
    }
    else{
        LOG_ERR("accept err:%d \n", errno);

        //  当前进程的fd已经用完
        //  可以调整单个服务器的fd上限
        //  也可以分布式部署
        if (errno == EMFILE){
            LOG_ERR("sockfd reached limit \n");
        }
    }
}
```

