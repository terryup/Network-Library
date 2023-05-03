#include "Poller.h"
#include "EpollPoller.h"

#include <stdlib.h>

Poller* Poller::newDefaultPoller(EventLoop* loop){
    if(::getenv("MUDUO_USE_PILL")){
        return nullptr; //   生成poll实例
    }
    else{
        return new EpollPoller(loop);  //  生成epoll实例
    }
}