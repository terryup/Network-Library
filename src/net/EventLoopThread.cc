#include "EventLoopThread.h"
#include "EventLoop.h"



EventLoopThread::EventLoopThread(const ThreadInitCallback &cb, const std::string &name)
    : loop_(nullptr)
    , exiting_(false)
    , thread_(std::bind(&EventLoopThread::threadFunc, this), name)
    , mutex_()
    , cond_()
    , callback_(cb) {

}

EventLoopThread::~EventLoopThread(){
    exiting_ = true;
    if (loop_ != nullptr){
        loop_->quit();
        thread_.join();
    }
}

//  开启线程池
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

//  下面这个方法，是在单独的新线程里面运行的
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