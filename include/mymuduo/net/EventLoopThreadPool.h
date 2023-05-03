#ifndef EVENTLOOPTHREADPOOL_H
#define EVENTLOOPTHREADPOOL_H

#include "noncopyable.h"

#include <functional>
#include <string>
#include <vector>
#include <memory>

class EventLoop;
class EventLoopThread;

class EventLoopThreadPool : noncopyable {
public:
    //  用户传入的函数
    using ThreadInitCallback = std::function<void(EventLoop*)>;

    EventLoopThreadPool(EventLoop *baseLoop, const std::string &nameArg);
    ~EventLoopThreadPool();

    //  设置线程数量
    void setThreadNum(int NumThread) { numThreads_ = numThreads_; }

    //  启动线程池
    void start(const ThreadInitCallback& cb = ThreadInitCallback());

    //  如果工作在多线程中，baseLoop_默认以轮询方式分配channel给subloop
    EventLoop* getNextLoop();

    std::vector<EventLoop*> getAllLoops();

    bool started() const { return started_; }

    const std::string& name() const { return name_; }
private:

    EventLoop *baseLoop_;   //  EventLoop Loop;
    std::string name_;
    bool started_;  //  开启线程池标志
    int numThreads_;    //  创建线程数量
    int next_;  //  轮询的下标
    std::vector<std::unique_ptr<EventLoopThread>> threads_; //  保存所有的EventLoopThread容器
    std::vector<EventLoop*> loops_; //  保存创建的所有EventLoop


};





#endif