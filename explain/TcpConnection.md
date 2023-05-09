# TcpConnection

## 什么是TcpConnection？

`TcpConnection` 是一个表示 TCP 连接的类，它封装了一个 TCP 连接的基本操作，例如发送数据、接收数据、关闭连接等。在 Muduo 网络库中，`TcpConnection` 类主要负责处理连接的生命周期，包括连接的建立、断开、数据传输等事件的处理，以及事件回调的注册和调用。`TcpConnection`都是在「sub Loop」上面的。

`TcpConnection` 通常是由 `TcpServer` 类创建，每当有新的 TCP 连接建立时，`TcpServer` 就会创建一个新的 `TcpConnection` 对象，并将其加入到 `TcpConnection` 列表中，然后调用用户注册的 `ConnectionCallback` 回调函数。之后，`TcpConnection` 就可以处理该连接上的数据读写事件，并在需要时调用用户注册的相应回调函数，例如 `MessageCallback`、`WriteCompleteCallback` 等。当连接关闭时，`TcpConnection` 会从 `TcpConnection` 列表中删除自己，并调用用户注册的 `CloseCallback` 回调函数，然后自动析构。

## Callbacks

我们单独的吧网络库中的一些回调函数定义在一个头文件里，如下：

- `TcpConnectionPtr`：定义了一个指向`TcpConnection`对象的智能指针类型，方便使用和管理`TcpConnection`对象。
- `ConnectionCallback`：连接建立时的回调函数类型，接收一个`TcpConnectionPtr`对象，表示已建立连接的`TcpConnection`对象。
- `CloseCallback`：连接关闭时的回调函数类型，接收一个`TcpConnectionPtr`对象，表示已关闭连接的`TcpConnection`对象。
- `WriteCompleteCallback`：数据发送完成时的回调函数类型，接收一个`TcpConnectionPtr`对象，表示发送数据的TcpConnection对象。
- `MessageCallback`：接收到数据时的回调函数类型，接收一个`TcpConnectionPtr`对象、一个指向Buffer对象的指针和一个`Timestamp`对象，分别表示已接收数据的`TcpConnection`对象、接收到的数据缓冲区和数据接收时间戳。
- `HighWaterMarkCallback`：高水位标志回调函数类型，接收一个`TcpConnectionPtr`对象和一个size_t类型的参数，分别表示达到高水位标志的`TcpConnection`对象和高水位标志的数据大小。

```c++
using TcpConnectionPtr = std::shared_ptr<TcpConnection>;
using ConnectionCallback = std::function<void(const TcpConnectionPtr&)>;
using CloseCallback = std::function<void(const TcpConnectionPtr&)>;
using WriteCompleteCallback = std::function<void(const TcpConnectionPtr&)>;

using MessageCallback = std::function<void(const TcpConnectionPtr&, Buffer*, Timestamp)>;
using HighWaterMarkCallback = std::function<void(const TcpConnectionPtr&, size_t)>;
```

## TcpConnection

##### 成员变量：

```c++
		EventLoop *loop_;   //  这里绝对不是baseLoop_，因为TcpConnection都是在subloop里面管理的
    const std::string name_;
    std::atomic_int state_;     //  连接状态
    bool reading_;

    //  这里和Acceptor类似，Acceptor => mainLoop    TcpConnection => subLoop
    std::unique_ptr<Socket> socket_;
    std::unique_ptr<Channel> channel_;

    const InetAddress localAddr_;   // 本服务器地址
    const InetAddress peerAddr_;    // 对端地址

    ConnectionCallback connectionCallback_; //  有新连接时的回调
    MessageCallback messageCallback_;   //  有读写消息时的回调
    WriteCompleteCallback writeCompleteCallback_;   //  消息发送完成以后的回调
    CloseCallback closeCallback_;   //  客户端关闭连接的回调
    HighWaterMarkCallback highWaterMarkCallback_;   //  超出水位实现的回调
    size_t highWaterMark_;

    Buffer inputBuffer_;    //  读取数据的缓冲区
    Buffer oututBuffer_;    //  发送数据的缓冲区
```

##### 构造函数：

```c++
TcpConnection::TcpConnection(EventLoop *loop, 
        const std::string &nameArg, 
        int sockfd, 
        const InetAddress& localaddr, 
        const InetAddress& peeraddr)
    : loop_(CheckLoopNotNull(loop))
    , name_(nameArg) 
    , state_(kConnecting)
    , reading_(true)
    , socket_(new Socket(sockfd))
    , channel_(new Channel(loop, sockfd))
    , localAddr_(localaddr)
    , peerAddr_(peeraddr)
    , highWaterMark_(64 * 1024 * 1024) //   64M
{
    //  下面给channel设置相应的回调函数 poller给channel通知感兴趣的事件发生了 channel会回调相应的回调函数
    channel_->setReadCallback(std::bind(&TcpConnection::handleRead, this, std::placeholders::_1));
    channel_->setWriteCallback(std::bind(&TcpConnection::handleWrite, this));
    channel_->setCloseCallback(std::bind(&TcpConnection::handleClose, this));
    channel_->setErrorCallback(std::bind(&TcpConnection::handleError, this));

    LOG_INFO("TcpConnection::ctor[%s]at fd = %d \n", name_.c_str(), sockfd);
    socket_->setKeepAlive(true);
}
```

这里还需要调用`CheckLoopNotNull()`函数检查一下传入的 `baseLoop _`指针是否有意义

```c++
static EventLoop *CheckLoopNotNull(EventLoop *loop){
    if(loop == nullptr){
        LOG_FAT("TcpConnection is null! \n");
    }
    return loop;
}
```

## 发送数据函数

这里我们定义了两个`send()`以便于用户调用，一个是传入`Buffer`参数一个是传入`std::string`。

```c++
void TcpConnection::send(const std::string &buf){
    if (state_ == kConnected){
        if (loop_->isInLoopThread()){
            sendInLoop(buf.c_str(), buf.size());
        }
        else{
            //  sendInLoop有多重重载，需要使用函数指针确定
            void(TcpConnection::*fp)(const void* data, size_t len) = &TcpConnection::sendInLoop;
            loop_->runInLoop(std::bind(
                fp,
                this,
                buf.c_str(),
                buf.size()
            ));
        }
    }
}

void TcpConnection::send(Buffer *buf){
    if (state_ == kConnected){
        if (loop_->isInLoopThread()){
            sendInLoop(buf->peek(), buf->readableBytes());
            buf->retrieveAll();
        }
        else{
            //  sendInLoop有多重重载，需要使用函数指针确定
            void (TcpConnection::*fp)(const std::string& message) = &TcpConnection::sendInLoop;
            loop_->runInLoop(std::bind(fp, this, buf->retrieveAllAsString()));
        }
    }
}
```

## sendInLoop函数

函数首先会判断连接状态，如果连接已经关闭（状态为 `kDisConnected`），则直接返回。接下来，如果 `channel_` 对应的文件描述符没有在监听可写事件，且输出缓冲区 `outputBuffer_` 中没有待发送数据，那么说明此时可以直接发送数据，使用系统调用 `write` 发送数据。如果 `write` 调用返回的已发送字节数小于0，且错误码不是 `EWOULDBLOCK`，则记录错误信息。如果返回的字节数大于等于0，说明可以一次性发送完数据，否则说明一次性没有发送完数据，还有部分数据需要保存到 `outputBuffer_` 中。

如果需要保存数据到 `outputBuffer_` 中，则先判断待发送数据的长度是否超过了 `highWaterMark_`（发送缓冲区高水位标记），如果超过了，且之前缓冲区中的待发送数据长度小于 `highWaterMark_`，则调用 `highWaterMarkCallback_`（缓冲区数据达到高水位的回调函数），并传递当前 `outputBuffer_` 中待发送数据的总长度。接着，将待发送数据添加到 `outputBuffer_` 中，然后判断 `channel_` 是否正在监听可写事件，如果没有，则注册可写事件（调用 `channel_->enableWriting()`），这样就能确保在发送缓冲区有空间的情况下，能够监听到可写事件，并触发 `handleWrite()` 回调函数，继续发送数据。

```c++
void TcpConnection::sendInLoop(const void* data, size_t len){
    ssize_t nwrote = 0;
    size_t remaining = len;
    bool faultError = false;

    //  之前调用过该connection的shutdown，不能在进行发送了
    if (state_ == kDisConnected){
        LOG_ERR("disconnected, give up writing! \n");
        return;
    }

    //  channel第一次写数据，且缓冲区没有待发送数据
    if (!channel_->isWriting() && oututBuffer_.readableBytes() == 0){
        nwrote = ::write(channel_->fd(), data, len);
        if (nwrote >= 0){
            //  判断有没有一次性写完
            remaining = len - nwrote;
            if (remaining == 0 && writeCompleteCallback_){
                //  既然一次性发送完事件就不用让channel对epollout事件感兴趣了
                loop_->queueInLoop(std::bind(writeCompleteCallback_, shared_from_this()));
            }
        }
        //  nwrote < 0
        else{
            nwrote = 0;
            if (errno != EWOULDBLOCK){
                LOG_ERR("TcpConnection::sendInLoop() \n");
                //  SIGPIPE
                if (errno == EPIPE || errno == ECONNRESET){
                    faultError = true;
                }
            }
        }
    }

    //  说明一次性并没有发送完数据，剩余数据需要保存到缓冲区中，且需要改channel注册写事件
    //  注册epollout事件，poller发现tcp的发送缓冲区有空间，会通知相应的sock-channel，然后调用handleWrite回调方法
    //  最终也就是调用TcpConnection::handleWrite()方法，吧发送缓冲区数据全部发送完成
    if(!faultError && remaining > 0){
        //  目前发送缓冲区剩余的待发送的数据的长度
        size_t oldLen = oututBuffer_.readableBytes();
        if (oldLen + remaining >= highWaterMark_ && 
            oldLen < highWaterMark_ &&
            highWaterMarkCallback_){
            loop_->queueInLoop(std::bind(highWaterMarkCallback_, shared_from_this(), oldLen + remaining));
        }
        oututBuffer_.append((char *)data + nwrote, remaining);
        if (channel_->isWriting()){
            //  这里一定要注册channel的写事件 否则poller不会给channel通知epollout
            channel_->enableWriting();
        }
    }
}
```

##### 余下成员函数：

- `handleRead(Timestamp receiveTime)` 函数用于处理读事件，即当有数据从连接的套接字上读取到时被调用。
- `handleWrite()` 函数用于处理写事件，即当连接的套接字上的可写事件被触发时被调用。
- `handleClose()` 函数用于处理连接关闭事件，即当连接的套接字关闭时被调用。
- `handleError()` 函数用于处理连接错误事件，即当连接发生错误时被调用。

```c++
void handleRead(Timestamp receiveTime);
void handleWrite();
void handleClose();
void handleError();
```

