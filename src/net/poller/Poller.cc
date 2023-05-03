#include "Poller.h"
#include "Channel.h"

Poller::Poller(EventLoop *loop)
    : ownerLoop_(loop){
}

Poller::~Poller() = default;

//  判断参数channel是否在当前Poller当中
bool Poller::hasChannel(Channel *channel) const{
    //  可以在map中找到该fd（键），并且it->second==channel（值）
    auto it = channels_.find(channel->fd());
    return it != channels_.end() && it->second == channel;
}