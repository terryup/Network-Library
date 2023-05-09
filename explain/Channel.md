# Channel

## 什么是Channel

Channel是一个种事件处理器，它与一个或者多个文件描述符相关联，用于监听该文件描述符上的事件并进行处理。简单来说它对文件描述符和事件进行了一层封装。在网络编程中，我们平时写基本都是创建套接字，绑定地址，转为可监听状态（这部分已经封装在`Socket`类中了，直接给`Acceptor`类调用就可以了），然后接受连接。

但得到一个初始化好的socket还不够，我们需要监听这个socket上的事件并处理事件。例如Reactor模型中用到了epoll监听该socket上的事件，我们还需要将被监听的套接字和监听的事件注册到epoll对象中。

可以想到文件描诉符和事件还有IO函数全放在一起，极其难维护。而Channel 的核心思想都是将事件处理逻辑与文件描述符的 I/O 操作分离开来，将文件描述符与其感兴趣的事件（需要监听的事件）封装到一起。而事件监听相关的代码则放到了Poller/EPollPoller类中，这样能使程序更加灵活和可扩展。

## 成员变量

##### 在`Channel`类定义三个静态的成员变量。其中：

`EPOLLIN`：表示文件描述符上有数据可读事件

`EPOLLPRI`：表示文件描述符上有紧急数据可读事件（也称带外数据，可以在任意时刻插入到数据流中，以便及时地传递一些重要信息）

`EPOLLOUT`：表示文件描述符上有数据可写事件

```c++
const int Channel::kNoneEvent = 0;
const int Channel::kReadEvent = EPOLLIN | EPOLLPRI;
const int Channel::kWriteEvent = EPOLLOUT;
```

- `int fd_`：这个Channel对象照看的文件描述符
- `int events_`：代表了fd感兴趣的事件类型的集合
- `int revents_`：代表事件监听器实际监听到该fd发生的事件类型集合，当事件监听器监听到一个fd发生了什么事件，会通过`Channel::set_revents()`函数来设置revents值
- `EventLoop *loop_`：记录这个Channel属于哪个EventLoop对象，因为采用的是one loop per thread模型，所以不止有一个EventLoop（一个EventLoop对应一个线程，Event Loop简要理解就是它负责管理这个线程中的所有事件）。当MainLoop接收到新的连接，将新连接相关事件注册到线程池中的某一个线程的subLoop上（轮询）。因为不希望跨线程处理函数，所以每一个Channel都需要记录是哪一个EventLoop在处理自己的事情。
- `int index_`：使用index_来记录channel与Poller相关的几种状态，Poller类会判断当前的Channel的状态然后处理不同的事情
  - `const int kNew = -1`：channel未添加到poller中
  - `const int kAdded = 1`：channel以添加到poller中
  - `const int kDeleted = 2`：channel从poller中删除

```c++
EventLoop *loop_;   //  事件循环 
const int fd_;  //  fd,poller监听的对象     epoll_ctr
int events_;    //  注册fd感兴趣的事件
int revents_;   //  poller返回的具体发生的事件
int index_;

std::weak_ptr<void> tie_;
bool tied_; 
```

- `readCallback_`：读事件回调函数
- `writeCallback_`：写事件回调函数
- `closeCallback_`：连接关闭回调函数
- `errorCallback_`：错误发生回调函数

这些全都是std::function类型，代表这个Channel为这个文件描述符保存的各事件类型发生时的处理函数。例如发生可读事件，需要执行可读事件处理函数，这时候Channel类都保管好了这些可以调用的函数。到时候交给EventLoop执行即可。

```c++
ReadEventCallback readCallback_;
EventCallback writeCallback_;
EventCallback closeCallback_;
EventCallback errorCallback_;
```

## 成员函数

### 设置此Channel对于事件的回调函数

```c++
//  设置回调函数对象
void setReadCallback(ReadEventCallback cb) {readCallback_ = std::move(cb);}
void setWriteCallback(EventCallback cb) {writeCallback_ = std::move(cb);}
void setCloseCallback(EventCallback cb) {closeCallback_ = std::move(cb);}
void setErrorCallback(EventCallback cb) {errorCallback_ = std::move(cb);}
```

### 设置 Channel 感兴趣的事件到 Poller

```c++
//  设置fd相应的事件状态
void enableReading() { events_ |= kReadEvent; update(); }
void disableReading() { events_ &= ~kReadEvent; update(); }
void enableWriting() { events_ |= kWriteEvent; update(); }
void disableWriting() { events_ &= ~kWriteEvent; update();}
void disableAll() { events_ = kNoneEvent; update(); }
```

## 更新Channel关注的事件

当改变channel所表示的fd的event事件后，需要通过update在epoll里面更改fd相应的事件epoll_ptr

```c++
void Channel::update(){
    //  通过channel所属的EventLoop，调用Poller的响应方法，注册fd的events事件
    loop_->updateChannel(this);
}
```

## 移除操作

```c++
//  在Channel所属的EventLoop中，吧当前的Channel删除掉
void Channel::remove(){
    loop_->removeChannel(this);
}
```

## 用于增加TcpConnection生命周期的tie方法（防止用户误删操作）

```c++
void Channel::tie(const std::shared_ptr<void>& obj){
    tie_ = obj;
    tied_ = true;
}
```

```c++
//  fd得到poller通知后，处理事件的。调用相应的回调方法
void Channel::handleEvemt(Timestamp receiveTime){
    if(tied_){
        //  变成shared_ptr增加引用计数，防止误删
        std::shared_ptr<void> guard = tie_.lock();
        if(guard){
            handleEventWithGuard(receiveTime);
        }
        
    }
    else{
        handleEventWithGuard(receiveTime);
    }
}
```

在`Channel::handleEvent()`中，会先判断Channel对象是否设置了`tie_`，如果设置了就获取一个指向对象的弱引用`guard`，在进行后续处理。其中`tie_`是一个是一个`std::shared_ptr<void>`类型的成员变量，通常情况下它是由`TcpConnection`的`shared_from_this()`函数返回的`shared_ptr`，从而在`TcpConeection`存活期内保持对`Channel`的引用。而`tied_`则是一个标志变量，用于表示`tie_`是否被设置，如果设置了就要进行`guard`的判断。

在`TcpConnection::connectEstablished()`函数会把当前的`TcpConnection`对象绑定到对应的channel对象上，这个绑定操作就是通过`channel_->tie(shared_from_this())`实现。而这个操作的目的在于让`Channel`对象上保持一个对`TcpConnection`的强引用，从而避免`Tcpconnection`被误删的风险。如果不进行这个 操作的话在`TcpConnection`被销毁时，`Channel`对象依然存在，但是其中指向`TcpConnection`的指针已经失效了，就会无法正确的处理`Channel`上的事件。

```c++
//  连接建立
void TcpConnection::connectEstablished(){
    //  建立连接，设置一开始状态为连接态
    setState(kConnected);
    /*
        tie相当于在底层有一个强引用指针记录着，防止析构
        为了防止TcpConnection这个资源被误删掉，而这个时候还有许多事件要处理
        channel->tie 会进行一次判断，是否将弱引用指针变成强引用，变成得话就防止了计数为0而被析构得可能
    */
    channel_->tie(shared_from_this());
    channel_->enableReading();  //  向poller注册EPOLLIN事件

    //  新连接建立，执行回调
    connectionCallback_(shared_from_this());
}  
```

这里传递的是this指针，所以是在Channel的内部增加对TcpConnection对象的引用计数，而不是Channel对象。通过引用计数巧妙的在内部增加一个引用计数，假设外面误删也不会因为计数为0而析构。

## 根据相应事件执行Channel保存的回调函数

Channel里面保存了许多回调函数，这些都是在对应的事件下被调用的。用户提前设置写好此事件的回调函数，并绑定到Channel的成员里。等到事件发生时，Channel自然的调用事件处理方法。借由回调操作实现了异步的操作。

```c++
//  根据poller通知的channel发生的具体事件，由channel负责调用具体的回调操作
void Channel::handleEventWithGuard(Timestamp receivTime){
    LOG_INFO("channel handleEvent revents:%d", revents_);

    //  对端关闭事件
    //  当TcpConnection对应Channel，通过shutdown关闭写端，epoll触发EPOLLHUP
    if((revents_ & EPOLLHUP) && !(revents_ & EPOLLIN)){
        //  确认是否拥有回调函数
        if(closeCallback_){
            closeCallback_();
        }
    }

    //  错误事件
    if(revents_ & EPOLLERR){
        if(errorCallback_){
            errorCallback_();
        }
    }

    //  读事件
    if(revents_ & (EPOLLIN | EPOLLPRI)){
        if(readCallback_){
            readCallback_(receivTime);
        }
    }

    //  写事件
    if(revents_ & EPOLLOUT){
        if(writeCallback_){
            writeCallback_();
        }
    }
}
```

