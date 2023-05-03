#include "Channel.h"
#include "EventLoop.h"
#include "logger.h"

#include <sys/epoll.h>

const int Channel::kNoneEvent = 0;
const int Channel::kReadEvent = EPOLLIN | EPOLLPRI;
const int Channel::kWriteEvent = EPOLLOUT;

Channel::Channel(EventLoop *loop, int fd)
    : loop_(loop)
    , fd_(fd)
    , events_(0)
    , revents_(0)
    , index_(-1)
    , tied_(false){
}

Channel::~Channel(){
}

//  channel的tie方法在什么时候调用过?  一个Tcp Connection新连接创建的时候 TcpConneciton => Channel
void Channel::tie(const std::shared_ptr<void>& obj){
    tie_ = obj;
    tied_ = true;
}

/*
    当改变channel所表示的fd的event事件后，需要通过update在epoll里面更改fd相应的事件epoll_ptr
    EventLoop  =>  ChannelList    Poller
*/
void Channel::update(){
    //  通过channel所属的EventLoop，调用Poller的响应方法，注册fd的events事件
    loop_->updateChannel(this);
}

//  在Channel所属的EventLoop中，吧当前的Channel删除掉
void Channel::remove(){
    loop_->removeChannel(this);
}

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