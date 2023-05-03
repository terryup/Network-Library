#include "EpollPoller.h"
#include "logger.h"
#include "Channel.h"

#include <errno.h>
#include <string.h>

    //  channel未添加到poller中
    const int kNew = -1;    //  chanell 的成员index_初始化就是-1
    //  channel以添加到poller中
    const int kAdded = 1;
    //  channel从poller中删除
    const int kDeleted = 2;

EpollPoller::EpollPoller(EventLoop *loop) 
    : Poller(loop)  //  传给基类
    , epollfd_(::epoll_create1(EPOLL_CLOEXEC))
    , events_(kInitEventListSize) { //  vector<epoll_event>
    if(epollfd_ < 0){
        LOG_FAT("epoll_create error:%d \n", errno);
    }
}

EpollPoller::~EpollPoller() {
    ::close(epollfd_);
}

Timestamp EpollPoller::poll(int timeoutMs, ChannelList *activechannels) {
    //  实际上应该用LOG_DEBUG更为合适
    LOG_INFO("func=%s => fd total count:%lu \n", __FUNCTION__, channels_.size());

    int numEvents = ::epoll_wait(epollfd_, &*events_.begin(), static_cast<int>(events_.size()), timeoutMs);
    int saveErrno = errno;
    Timestamp now(Timestamp::now());

    //  有事件产生
    if(numEvents > 0){
        LOG_INFO("%d events happened \n", numEvents);
        fillActiveChannels(numEvents, activechannels);  //  填充活跃的channels
        //  // 填充活跃的channels
        if(numEvents == events_.size()){
            events_.resize(events_.size() * 2);
        }
    }
    //  超时
    else if(numEvents == 0){
        LOG_DEB("%s timeoud! \n", __FUNCTION__);
    }
    //  出错
    else{
        //  不是终端错误
        if(saveErrno != EINTR){
            errno = saveErrno;
            LOG_ERR("EpollPoller::poll() err!");
        }
    }
    return now;
}

//  channel update remoce => EventLoop updateChaneel removeChannel => Poller updateChaneel removeChannel
/*
            EventLoop => poller.poll
    ChannelList     Poller
                    ChannelMap   <fd, channel*>  epollfd
*/
void EpollPoller::updateChannel(Channel* channel) {
    //  获取参数channel在epoll的状态
    const int index = channel->index();
    LOG_INFO("func=%s => fd=%d events=%d index=%d \n", __FUNCTION__,  channel->fd(), channel->events(), index);
    //  未添加状态和已删除状态都有可能会被再次添加到epoll中
    if(index == kNew || index == kDeleted){
        //  添加到键值对 
        if(index == kNew){
            int fd = channel->fd();
            channels_[fd] = channel;
        }

        //  修改channel的状态，此时是已添加状态
        channel->set_index(kAdded);
        //  向epoll对象加入channel
        update(EPOLL_CTL_ADD, channel);
    }
    else{   //  channel已经在poller上注册过了
        //  没有感兴趣事件说明可以从epoll对象中删除该channel了
        int fd = channel->fd();
        if(channel->isNonEvents()){
            update(EPOLL_CTL_DEL, channel);
            channel->set_index(kDeleted);
        }
        //  还有事件说明之前的事件删除，但是被修改了
        else{
            update(EPOLL_CTL_MOD, channel);
        }
    }
}

//  从poller中删除channel
void EpollPoller::removeChannel(Channel* channel) {
    int fd = channel->fd();
    channels_.erase(fd);

    LOG_INFO("func=%s => fd=%d \n", __FUNCTION__,  channel->fd());

    int index = channel->index();
    if(index == kAdded){
        //  如果此fd已经被添加到Poller中，则还需从epoll对象中删除
        update(EPOLL_CTL_DEL, channel);
    }
    //  重新设置channel的状态为未被Poller注册
    channel->set_index(kNew);
}

//  填写活跃的连接
void EpollPoller::fillActiveChannels(int numEvents, ChannelList *activechannels) const{
    for(int i = 0; i < numEvents; ++i){
        //  void* => Channel*
        Channel *channel = static_cast<Channel*>(events_[i].data.ptr);
        channel->set_revents(events_[i].events);
        activechannels->push_back(channel); //  EventLoop就拿到了它的poller给他返回的所有发生事件的channel列表
    }
}

//  更新channel通道     epoll_ctl  add/mod/del
void EpollPoller::update(int operation, Channel *channel){
    epoll_event event;
    ::memset(&event, 0, sizeof(event));

    int fd = channel->fd();

    event.events = channel->events();
    event.data.fd = channel->fd();
    event.data.ptr = channel;

    if(::epoll_ctl(epollfd_, operation, fd, &event) < 0){
        if(operation == EPOLL_CTL_DEL){
            LOG_ERR("epoll_ctl del error:%d \n", errno);
        }
        else{
            LOG_FAT("epoll_ctl add/mod error:%d \n", errno);
        }
    }
}
