#ifndef EPOLLPOLLER_H
#define EPOLLPOLLER_H

#include <vector>
#include <sys/epoll.h>
#include <unistd.h>

#include "Poller.h"
#include "Timestamp.h"

class Channel;

/*
    epoll的使用
    epoll_create
    epoll_ctl   add/mod/del
    epoll_wait
*/
class EpollPoller : public Poller{
public:
    EpollPoller(EventLoop *loop);
    ~EpollPoller() override;

    //  重写基类poller的抽象方法
    Timestamp poll(int timeoutMs, ChannelList *activechannels) override;
    void updateChannel(Channel* channel) override;
    void removeChannel(Channel* channel) override;

private:
    static const int kInitEventListSize = 16;

    //  填写活跃的连接
    void fillActiveChannels(int numEvents, ChannelList *activechannels) const;
    //  更新channel通道
    void update(int operation, Channel *channel);

    using EventList = std::vector<epoll_event>;

    int epollfd_;
    EventList events_;


};













#endif