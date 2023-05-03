#include "Thread.h"
#include "CurrentThread.h"

#include <semaphore.h>

std::atomic_int Thread::numCreated_(0);

Thread::Thread(ThreadFunc, const std::string &name)
    : started_(false)
    , joined_(false)
    , tid_(0)
    , func_(std::move(func_))
    , name_(name) {
    //  设置线程索引编号和姓名
    setDefaultName();
}

Thread::~Thread(){
    if (started_ && !joined_){
        //  thread类提供了设置分离线程的方法
        thread_->detach();
    }
}

//  开启线程
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

//  等待线程
void Thread::join(){
    joined_ = true;
    //  等待线程执行完毕
    thread_->join();
}

void Thread::setDefaultName(){
    int num = ++numCreated_;
    if (name_.empty()){
        char buf[32] = {0};
        snprintf(buf, sizeof(buf), "Thread%d", num);
        name_ = buf;
    }
}