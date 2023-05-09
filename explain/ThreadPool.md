# ThreadPool

## one loop per thread

muduo网络库中提到了一个概念：one loop per thread，在写这篇前我也不是非常理解最后画了一个图才有所理解，这里直接上图。

![](https://github.com/terryup/Network-Library/blob/master/explain/one%20loop%20per%20thread.png)

其实就是一个主线程（main Loop）还有他的子线程（sub Loop），主线程负责监听新连接和任务分配，通过IO线程池中的子线程去处理具体的IO事件和业务逻辑。主线程和子线程都拥有自己的EventLoop对象，主线程中的EventLoop负责监听新连接，接收连接后会把连接交给IO线程池中的某个子线程的EventLoop来处理具体的IO事件和业务逻辑。在IO线程池中，每个子线程也都拥有自己的EventLoop对象。主线程和子线程之间通过线程间的通信方式来传递任务和数据。

当然，并不是只能有一个主线程（main Loop），他可以有多个主线程多个子线程，每一个主线程负责自己的子线程，但我们这里只实现了「一个主线程」。

## Thread

`Thread`类是对C++11标准库中的thread类进行封装，目的是为了更方便地创建线程。

##### 成员函数：

```c++
bool started_;  //  是否启动线程
bool joined_;   //  是否等待该线程
std::shared_ptr<std::thread> thread_;
pid_t tid_; //  线程tid
//  Thread::start() 调用的回调函数
//  其实保存的是 EventLoopThread::threadFunc()
ThreadFunc func_;
std::string name_;  //  线程名
static std::atomic_int32_t numCreated_; //  线程索引
```

##### 构造函数：

```c++
Thread::Thread(ThreadFunc, const std::string &name)
    : started_(false)
    , joined_(false)
    , tid_(0)
    , func_(std::move(func_))
    , name_(name) {
    //  设置线程索引编号和姓名
    setDefaultName();
}
```

##### 析构函数：

```c++
Thread::~Thread(){
    if (started_ && !joined_){
        //  thread类提供了设置分离线程的方法
        thread_->detach();
    }
}
```

##### 开启一个线程：

这里`func_()`函数其实调用的就是`EventLoopThread::threadFunc()`这个函数

```c++
void Thread::start(){   //  一个Thread对象，记录的就是一个新线程的详细信息
    started_ = true;
    sem_t sem;
    sem_init(&sem, false, 0);
    //  开启线程
    thread_ = std::shared_ptr<std::thread>(new std::thread([&](){
        //  获取线程的tid值
        tid_ = CurrentThread::tid();
        
        //  v 操作
        sem_post(&sem);
        
        //  开启一个新线程，专门执行该线程函数
        func_();
    }));

    //  这里必须等待获取上面的新创建的线程的tid值
    //  未获取到信息则不会执行sem_post，所以会被阻塞住
    //  如果不使用信号量操作，则别的线程访问tid时候，可能上面的线程还没有获取到tid
    sem_wait(&sem);
}
```

## EventLoopThread

`EventLoopThread`类提供了单独线程中运行EventLoop的功能，我们吧线程的创建和销毁封装在这个类里面。

##### 先来看他的成员函数：

- `loop_`：指向 `EventLoop` 对象的指针，这是当前线程所对应的事件循环对象
- `exiting_`：标记当前线程是否正在退出
- `thread_`：线程对象，用于管理当前线程
- `mutex_`：线程互斥量，用于保护多线程访问下的共享数据
- `cond_`：条件变量，用于线程间的同步
- `callback_`：线程初始化回调函数，在线程启动时被调用，可以用于初始化一些线程特有的数据

```c++
using ThreadInitCallback = std::function<void(EventLoop*)>;
EventLoop *loop_;
bool exiting_;
Thread thread_;
std::mutex mutex_;
std::condition_variable cond_;
ThreadInitCallback callback_;
```

##### 构造函数：

构造函数主要就是一些初始化的操作

```c++
EventLoopThread::EventLoopThread(const ThreadInitCallback &cb, const std::string &name)
    : loop_(nullptr)
    , exiting_(false)
    , thread_(std::bind(&EventLoopThread::threadFunc, this), name)
    , mutex_()
    , cond_()
    , callback_(cb) {
}
```

##### 析构函数：

这里析构函数先把`exiting_`标记为true代表线程退出，然后判断一下如果这个线程的EventLoop还存在那就给他退出掉。

```c++
EventLoopThread::~EventLoopThread(){
    exiting_ = true;
    if (loop_ != nullptr){
        loop_->quit();
        thread_.join();
    }
}
```

##### 开启一个新的线程：

1. 在函数中调用了线程类 `Thread` 中的 `start` 方法，开启一个新线程，并在新线程中运行 `EventLoopThread::threadFunc` 函数。
2. 函数获取到了一个空指针 `loop`，然后加锁进入了一个 `while` 循环，在循环中，判断是否有 `EventLoop` 对象被创建出来，如果没有，则等待条件变量 `cond_` 的通知。
3. 一旦有了新的 `EventLoop` 对象，则将其赋值给 `loop`，解锁并返回该对象的指针。

此时`startLoop()`函数获取到了`loop_`指针，并返回它。这样，调用者就可以通过这个指针来操作该线程所绑定的`EventLoop`对象了。

```c++
EventLoop *EventLoopThread::startLoop(){
    //  调用startLoop即开启一个新线程   启动底层的新线程
    thread_.start();

    EventLoop *loop = nullptr;
    {
        //  等待新线程执行threadFunc完毕，所以使用cond_.wait
        std::unique_lock<std::mutex>lock(mutex_);
        while(loop_ == nullptr){
            cond_.wait(lock);
        }
        loop = loop_;
    }
    return loop;
}
```

##### threadFunc

这实际上是一个线程函数，它在启动新线程时被调用。它的作用是在新线程中创建一个新的 `EventLoop` 对象，并将其赋值给 `loop_` 成员变量。同时，通过条件变量通知在 `EventLoopThread::startLoop()` 中等待的主线程。

当 `EventLoopThread::startLoop()` 被调用时，它会启动一个新线程，调用 `EventLoopThread::threadFunc()` 函数，并等待条件变量 `cond_` 被唤醒。在 `EventLoopThread::threadFunc()` 函数中，它创建了一个新的 `EventLoop` 对象，赋值给 `loop_` 成员变量，并通过条件变量通知主线程。主线程在收到条件变量通知后，可以通过 `EventLoopThread::startLoop()` 返回的指针来使用新创建的 `EventLoop` 对象。同时，子线程在 `EventLoopThread::threadFunc()` 中会进入事件循环，不断地处理事件。

```c++
void EventLoopThread::threadFunc(){
    EventLoop loop; //  创建一个独立的EventLoop，和上面的线程是一一对应的，one loop per thread

    if (callback_){
        callback_(&loop);
    }

    {
        std::unique_lock<std::mutex>lock(mutex_);
        loop_ = &loop;  //  等到生成EventLoop对象之后才唤醒
        cond_.notify_one();
    }

    //  执行EventLoop的loop() 开启了底层的Poller的poll()
    //  这个是subLoop
    loop.loop();
    //  loop是一个事件循环，如果往下执行说明停止了事件循环，需要关闭eventLoop
    //  此处是获取互斥锁再置loop_为空
    std::unique_lock<std::mutex>lock(mutex_);
    loop_ = nullptr;
}
```

## EventLoopThreadPool

`EventLoopThreadPool`类是为了更好地支持多线程网络编程而设计的，它是一个线程池，可以管理多个`EventLoopThread`线程，并提供了获取下一个`EventLoop`的方法，使得新的连接能够被均衡地分配到不同的`EventLoop`上处理，从而提高网络编程的效率。

##### 成员函数：

从成员变量可以看到，他有一个`baseLoop_`这个变量其实就是上文所说的主线程（mian Loop）他的EventLoop。我们会通过构造函数初始化他的主线程，这里只有「一个主线程」。

```c++
EventLoop *baseLoop_;   //  EventLoop Loop;
std::string name_;
bool started_;  //  开启线程池标志
int numThreads_;    //  创建线程数量
int next_;  //  轮询的下标
std::vector<std::unique_ptr<EventLoopThread>> threads_; //  保存所有的EventLoopThread容器
std::vector<EventLoop*> loops_; //  保存创建的所有EventLoop
```

##### 构造函数：

这里我们通过构造函数会传进来一个EventLoop的指针，我们把它初始化给`baseLoop_`，让他作为主线程来监听连接。

```c++
EventLoopThreadPool::EventLoopThreadPool(EventLoop *baseLoop, const std::string &nameArg)
    : baseLoop_(baseLoop)
    , name_(nameArg)
    , started_(false)
    , numThreads_(0)
    , next_(0) {
}
```

##### 启动线程池：

这个函数会先将线程池设置为开启状态，然后，通过循环创建`numThreads_`个`EventLoopThread对象`，并将它们加入`threads_`容器中。每个`EventLoopThread`对象都会启动一个新线程，并在新线程中创建一个`EventLoop`对象，并将该`EventLoop`对象的地址加入`loops_`容器中。

如果`numThreads`_为0，则表示只有一个`EventLoop`线程在运行，即`baseLoop_`，此时如果传入了`ThreadInitCallback`回调函数，则直接在`baseLoop_`线程中执行该回调函数。

```c++
void EventLoopThreadPool::start(const ThreadInitCallback& cb){
    started_ = true;

    //  循环创建线程
    for(int i = 0; i < numThreads_; ++i){
        char buf[name_.size() + 32];
        snprintf(buf, sizeof(buf), "%s%d", name_.c_str(), i);
        //  创建EventLoopThread对象
        EventLoopThread *t = new EventLoopThread(cb, buf);
        //  加入此EventLoopThread容器
        threads_.push_back(std::unique_ptr<EventLoopThread>(t));
        //  底层创建线程，绑定一个EventLoop，并返回该Loop的地址
        //  此时已经开始执行新线程了
        loops_.push_back(t->startLoop());   
    }

    //  整个服务端只有一个线程运行着baseLoop
    if (numThreads_ == 0 && cb){
        cb(baseLoop_);
    }
}
```

##### 获取一个子线程：

如果工作在多线程中，`baseLoop_`(mainLoop)默认以轮询方式分配`channel`给`subloop`。

```c++
EventLoop* EventLoopThreadPool::getNextLoop(){
    /*
        如果只设置一个线程，也就是只有mianLoop没有subLoop
        那么轮询只有一个线程，getnextLoop()每次都返回当前的beaseLoop_
    */
    EventLoop *loop = baseLoop_;
    
    //  通过轮询，获取下一个处理时间的loop
    //  如果没设置多线程数量，则不会进去，相当于直接返回baseLoop_
    if (!loops_.empty()){
        loop = loops_[next_];
        ++next_;
        //  轮询
        if (next_ >= loops_.size()){
            next_ = 0;
        }
    }

    return loop;
}
```

