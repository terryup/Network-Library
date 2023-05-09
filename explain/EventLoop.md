# EventLoop

## 什么是EventLoop？

EventLoop是整个网络库的核心组件，是一个事件循环器，他负责事件的分发和处理，其驱动着Reactor模型。之前实现的Channel类和Poller类都需要依靠EventLoop来调用。

EventLoop在初始化时会创建一个Poller实例，并将其作为成员变量保存，同时还会创建一个Channel对象，并调用Poller的`updateChannel`函数将该Channel注册到Poller中去，让其加入到被关注的文件描述符集合当中。

EventLoop可以看作为Channel和Poller之间的桥梁，Channel和Poller之间并不沟通，而是借助EventLoop这个类。

可以看到EventLoop里面的成员变量就包含有Channel和Poller

```c++
std::unique_ptr<Poller> poller_;
std::unique_ptr<Channel> wakeupChannel_;
ChannelList activeChannels_;
```

EventLoop其重点在于循环调用Poller中实现的`epoll_wait`不断的监听发生的事件，然后再调用相应的事件函数。

![](/Users/zixuanhuang/Desktop/webserver_explain/EventLoop.png)

## 成员变量

- `wakeupFd_`：当需要在其他线程执行一个任务并且通知EventLoop线程时，可以向`wakeupFd_`写入一个字节，这将唤醒EventLoop线程，从而在下一次`Poller::poll()`调用时立即返回。在EventLoop中，可以监听`wakeupFd_`上的读事件并读取其内容，然后执行相应的任务。
- `activeChannels_`：通过调用`Poller::poll()`得到发生事件的Channel，将其储存在`activeChannels_`中。
- `pendingFuntors_`：作用是用于在`EventLoop`对象所在的线程中执行一些需要执行的回调函数，例如在非`IO`线程中发送消息到`IO`线程进行处理。因为`EventLoop`对象只能在一个线程中运行，因此需要把需要在`EventLoop`线程中执行的函数对象加入到这个容器中，然后在`EventLoop`对象所在的线程中循环遍历这个容器，依次执行其中的函数对象。

```c++
 using ChannelList = std::vector<Channel*>;

    std::atomic_bool looping_;  //  原子操作，通过CAS实现的
    std::atomic_bool quit_; //  标志退出loop循环
    
    const pid_t threadId_;  //  记录当前loop所在线程的ID
    Timestamp pollReturnTime_;  //  poller返回发生事件的channels的时间点
    std::unique_ptr<Poller> poller_;
    std::unique_ptr<TimerQueue> timerQueue_;

    //  主要作用，当mainloop获取一个新用户的channel，通过轮询算法选择一个subloop，通过该成员唤醒subloop处理channel
    int wakeupFd_;  
    std::unique_ptr<Channel> wakeupChannel_;

    ChannelList activeChannels_;

    std::atomic_bool callingPendingFunctors_;   //  标识当前loop是否有需要执行的回调操作
    std::vector<Functor> pendingFuntors_;   //  存储loop需要执行的回调操作
    std::mutex mutex_;  //  互斥锁，用来保护上面vector容器的线程安全操作

```

## 成员方法

### 判断EventLoop对象是否在自己的线程里面

![](/Users/zixuanhuang/Desktop/webserver_explain/68747470733a2f2f63646e2e6e6c61726b2e636f6d2f79757175652f302f323032322f706e672f32363735323037382f313636333332343935353132362d33613830373866652d663237312d346131622d383263372d6237356564666633636461382e706e6723617665726167654875653d253233663465.png)

由于muduo库是主从的Reactor模型，主Reactor（main Event Loop)，负责监听连接，然后通过轮询的方法吧新连接发送到某和从Reactor（sub Event Loop)上维护。

1. 一个线程只有一个`Reactor`
2. 一个`Reactor`只有一个`EventLoop`
3. 每一个`EventLoop`在创建时都会保存他自己的线程值

```c++
bool isInLoopThread() const { return threadId_ == CurrentThread::tid(); }
```

而这个`CurrentThread::tid()`用于在当前线程下获取线程的ID（线程值）。

`t_cachedTid`是一个线程局部储存的变量，通过`__thread`关键字声明，所以意味着每一个线程都会拥有一个独立的`int`类型的变量来存放他们的tid。而`cachedTid()`则是用于获取当前线程的ID并将它缓存到`t_cachedTid`中。

`tid()`函数则是一个访问器函数，它用于获取当前线程的一个ID。如果`t_cachedTid`为0的话，那也就说明当前还没有缓存这个线程它的ID，那就需要调用`cachedTid()`函数来吧ID缓存到`t_cachedTid`中。

```c++
namespace CurrentThread{
    extern __thread int t_cachedTid;
    
    void cachedTid();

    inline int tid(){
        if (__builtin_expect(t_cachedTid == 0, 0)){
            cachedTid();
        }
        return t_cachedTid;
    }

}
```

而`cachedTid()`函数则是先判断一下`t_cachedTid`是否为0，如果为0则说明没缓存过，此时就通过系统调用`syscall`来获取当前线程的tid值，并将它赋值给`t_cachedTid`。

其中`__thread`是一个GCC内置的关键字，它用于声明线程局部储存的变量，所以每一个线程都会拥有一份独立的实例，它具有线程安全性，也就保证了`t_cachedTid`的线程安全。

```c++
namespace CurrentThread{
    __thread int t_cachedTid = 0;

    void cachedTid(){
        if(t_cachedTid == 0){
            //  通过linux系统调用，获取当前线程的tid值
            t_cachedTid = static_cast<pid_t>(::syscall(SYS_getpid));
        }
    }

}
```

### EventLoop的构造函数

```c++
EventLoop::EventLoop()
    : looping_(false)
    , quit_(false)
    , callingPendingFunctors_(false)
    , threadId_(CurrentThread::tid()) 
    , poller_(Poller::newDefaultPoller(this))
    , timerQueue_(new TimerQueue(this))
    , wakeupFd_(CreateEventfd())
    , wakeupChannel_(new Channel(this, wakeupFd_)) {
    LOG_DEB("EventLoop created %p in thread %d \n", this, threadId_);
    if(t_loopInThisThread){
        LOG_FAT("Another EventLoop %p exists in this thread %d \n", t_loopInThisThread, threadId_);
    }
    else{
        t_loopInThisThread = this;
    }

    //  设置wakeupfd的事件类型，以及发生事件后的回调操作
    wakeupChannel_->setReadCallback(std::bind(&EventLoop::handleRead, this));
    //  每一个EventLoop都将监听wakeupchannel的EPOLLIN读事件了
    wakeupChannel_->enableReading();
}
```

这里需要关注`wakeupFd_`的初始化，可以看到它调用了`CreateEventfd()`函数来初始化，而这个既是用来notify唤醒subReactor处理新来的Channel的。既为设置非阻塞。

```c++
int CreateEventfd(){
    //  用来通知睡觉的EventLoop起来，有一个新的连接的用户的channel要扔给你来处理
    int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (evtfd < 0){
        LOG_FAT("eventfd error:%d \n", errno);
    }
    return evtfd;
}
```

### EventLoop的析构函数

```c++
EventLoop::~EventLoop(){
    //  channel移除所有感兴趣事件
    wakeupChannel_->disableAll();
    //  将channel从EventLoop中删除
    wakeupChannel_->remove();
    //  关闭 wakeupFd_
    ::close(wakeupFd_);
    //  指向EventLoop指针为空
    t_loopInThisThread = nullptr;
}
```

### EventLoop的驱动核心

当调用了`EventLoop::loop()`方法后，整个事件循环正式开启，他底层会调用`epoll_wait`来循环监听是否有事件到达。

1. 首先会清空`activeChannels_`中的Channel，然后调用`poller->pool()`方法，底层也就是调用了`epoll_wait`来监听是否有事件发生，当有事件发生，会把发生事件的channel加入`activeChannels_`当中去。
2. 遍历`activeChannels_`调用各个事件的回调函数
3. `doPendingFunctors()`则为跨线程执行回调

```c++
void EventLoop::loop(){
    looping_ = true;
    quit_ = false;
    

    LOG_INFO("EventLoop %p start looping \n", this);

    while(!quit_){
        activeChannels_.clear();
        //  监听两类fd  一种是client的fd，一种是wakeupfd
        pollReturnTime_ = poller_->poll(kPollTimeMs, &activeChannels_);
        for (Channel *channel : activeChannels_){
            //  Poller监听哪些channel发生事件了，然后上报给EventLoop，通知channel处理相应的事件
            channel->handleEvemt(pollReturnTime_);
        }
        //  执行当前EventLoop事件循环需要处理的回调操作
        /*
            IO线程 mainLoop accept fd <= channel subloop
            mainLoop 事先注册一个回调cb (需要subloop来执行) wakeup subloop后，执行下面的方法，执行之						前mainLoop注册的cb操作
        */
        doPendingFunctors();
    }

    LOG_INFO("EventLoop %p stop looping. \n", this);
    looping_ = false;
}
```

### EventLoop分派任务

EventLoop使用`runInLoop(Functor cb)`函数执行任务，传入的参数是一个回调参数，让此EventLoop去执行任务，可以跨线程调用。

例如在`TcpServer`类中的`newConnection()`方法里，当通过轮询算法选择了一个sub Event Loop之后，他会传回来一个`ioLoop`的对象，我们通过这个对象将`TcpConnection::connectEstablished`函数注册到`ioLoop`所对应的事件循环中的`pendingFunctors_`队列中。

```c++
ioLoop->runInLoop(std::bind(&TcpConnection::connectEstablished, conn));
```

接下来看看`runInLoop()`的实现：

他先回调用`isInLoopThread()`判断本线程是否是创建该EventLoop的线程

1. 如果是，那就直接同步调用，执行任务
2. 如果不是，那就说明需要用到跨线程调用，需要执行函数`queueInLoop()`

```c++
void EventLoop::runInLoop(Functor cb){
    //  在当前的loop线程中，执行cb
    if(isInLoopThread()){
        cb();
    }
    else{   //  在非loop线程中执行cb，就需要唤醒loop所在线程，执行cb
        queueInLoop(cb);
    }
}
```

### 如何保证线程安全

还是看到上面的`ioLoop`对象，他是从线程池中的某个线程创建而来的，所以可以知道创建`ioLoop`和本线程并不是同一个线程，那么这个操作是线程不安全的。

所以这里的处理方法是，保证各个任务都在原有的线程中执行。如果出现跨线程执行，那就把这个任务加入到任务队列当中，并且韩星应当执行这个任务的线程。而原来的线程在唤醒其他线程后则可以继续执行别的操作。下面是`queueInLoop`实现：

```c++
void EventLoop::queueInLoop(Functor cb){
    {
        std::unique_lock<std::mutex> lock(mutex_);
        pendingFuntors_.emplace_back(cb);
    }


    //  唤醒相应的需要执行上面回调操作的loop的线程了
    /*
        || callingPendingFunctors_ 的意思是：当前loop正在执行回调，但是loop又遇到了新的回调
                                            这个时候也需要唤醒，否则就会发生有事件到来但是仍被阻塞住的情况
    */
    if (!isInLoopThread() || callingPendingFunctors_){
        wakeup();   //  唤醒loop所在线程
    }
}

```

这个函数具体做了这几件事：

1. 先用互斥锁保护回调函数列表`pendingFuntors_，在保证线程安全的前提下，将新的回调加入到列表当中
2. 如果当前线程不是循环所在的线程，或者说实现循环正在执行回调函数，那么调用`wakeup()`函数唤醒相应的事件循环线程
3. 如果当前线程是事件函数所在的线程，但是事件循环正在执行回调函数，那么即使`wakeup()`被调用了也无法立即执行回调，因此这种情况不需要唤醒事件循环线程。

需要注意的是，这个函数中的 `wakeup()` 调用不一定会触发跨线程调用，因为事件循环对象 `EventLoop` 和 `Channel` 对象中都有一个 `wakeupFd_` 文件描述符，这个文件描述符可以用来实现跨线程唤醒。如果 `EventLoop` 和 `Channel` 对象在同一个线程中，那么 `wakeup()` 调用就不会触发跨线程调用。如果它们在不同的线程中，那么 `wakeup()` 调用就会触发跨线程调用，从而唤醒相应的事件循环线程来执行回调函数。

因为每个`EventLoop`的`wakeupFd_`都被加入到了epoll对象中，所以只要写了数据就会触发读事件，`epoll_wait`就会返回。因此`EventLoop::loop`中的阻塞就会被打断，Reactor又会被事件驱动起来。

```c++
//  用来唤醒loop所在的线程的    向wakeupfd_写一个数据   wakeupchannel就发生读事件，当前loop线程就会被唤醒
void EventLoop::wakeup(){
    uint64_t one = 1;
    ssize_t n = write(wakeupFd_, &one, sizeof(one));
    if(n != sizeof(one)){
        LOG_ERR("EventLoop::wakeup writes %lu bytes instead of 8 \n", n);
    }
}
```

### EventLoop如何处理pendingFunctior里的回调函数？

这里是一个细节处理，这里定义了一个functors交换pendingFunctors的元素，然后在遍历。

因为如果直接遍历pendingFunctors的话，我们在这个遍历的过程中别的线程又向这个容器添加了新的回调函数，那这个过程肯定是无法保证线程安全的，那就得上互斥锁，那我们在执行回调的时候就无法添加新的回调会阻塞在哪里，这就非常影响效率。

所以这里的处理方式是先用互斥锁保护一个交换操作，这个操作会比我们一点一点执行回调的操作快很多，所以我们交换完成后直接就能把锁释放掉，然后直接遍历新定义的这个funtors，而其他进程又可以继续添加回调任务到`pendingFunctors`。

```c++
void EventLoop::doPendingFunctors(){
    std::vector<Functor> functors;
    callingPendingFunctors_ = true;

    /*
        如果没有这个局部的functors 
        则在互斥锁的加持下直接遍历callingPendingFunctors_
        其他的线程这个时候就无法访问，也无法向里面注册回调函数，增加服务器时延
    */
    {
        std::unique_lock<std::mutex> lock(mutex_);
        functors.swap(pendingFuntors_);
    }

    for (const Functor &functor : functors){
        functor();  //  执行当前loop需要执行的回调操作
    }

    callingPendingFunctors_ = false;
}
```

### 主动关闭循环事件

```c++
void EventLoop::quit(){
    quit_ = true;

    //  如果是在其他的线程中，调用的quit    在一个subloop(woker)中，调用了mainLoop(IO)的quit
    if(!isInLoopThread()){
        wakeup();
    }
}
```

这里会出现两种情况一种是在自己的线程中，一种不在自己线程中。

1. 将`quit_`置为true
2. 如果不是当前线程调用，则唤醒EventLoop处理事件

这里唤醒`EventLoop`是因为可以让他自己的循环`wile(!quit_)`去判断，然后执行完它目前的这一次循环后再回到判断时就会因为`quit_ = ture`而退出循环。这样就不会出现正在处理任务时处理一半突然退出了。但不能保证未来事件发生的处理。
