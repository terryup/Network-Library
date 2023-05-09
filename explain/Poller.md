# Poller

## 什么是poller

poller是事件分发器（event dispatcher）的一种实现方式。事件分发器是负责监听和分发I/O事件的组件，它是整个网络库中的核心部分。

主要作用：

1. 监听文件描述符上的事件，包括读写事件。
2. 将事件分发给相应的Channel对象，使得Channel对象能够及时响应事件。
3. 支持跨平台，并且实现高效、可靠的I/O事件分发。

`Poller`类则是这个事件分发器的基类，是对系统级I/O复用函数（例如poll、epoll、kqueue）的封装，用于监听文件描述符上的事件并将事件分发给相应的Channel对象。`Poller`类还定义了一些虚函数，用于派生类中实现具体的事件分发。

`EpollPoller`类则是`Poller`的派生类，他是Linux平台上使用epoll实现的事件分发器。`EpollPoller`类实现了`Poller`类中定义的虚函数，具体实现了基于epoll事件分发，并将事件分发给Channel对象。

而定义`Poller`类为基类`EpollPoller`类为派生类也正是这里设计的巧妙之处，他可以通过不同的派生类继承`Poller`类的方法来适应不同平台和不同需求。本项目仅实现Epoll，然而在muduo库中支持两种poller实现方式：poll和epoll。并且还支持两种模式：LT模式和ET模式。这两种模式同样可以根据不同需求进行切换。

## 交互图

![](/Users/zixuanhuang/Desktop/webserver_explain/Poller(基类).png)

## `Poller`类的定义：

```c++
class Poller : noncopyable{
public:
    using ChannelList = std::vector<Channel*>;

    Poller(EventLoop *loop);
    virtual ~Poller();

    //  给所有IO复用保留统一的接口
    virtual Timestamp poll(int timeoutMs, ChannelList *activeChannels) = 0;
    virtual void updateChannel(Channel* channel) = 0;
    virtual void removeChannel(Channel* channel) = 0;
    
    //  判断参数channel是否在当前Poller当中
    bool hasChannel(Channel *channel) const;

    //  EventLoop可以通过该接口获取默认的IO复用的具体实现
    static Poller* newDefaultPoller(EventLoop* loop);
protected:
    //  map的key：sockfd  value:sockfd所属的channel通道类型
    using ChannelMap = std::unordered_map<int , Channel*>;
    ChannelMap channels_;
private:
    EventLoop *ownerLoop_;  //  定义poller所属的事件循环EventLoop
};
```

- `Timestamp poll(int timeoutMs, ChannelList *activeChannels)`：这是一个纯虚函数，它用于时间分发，也就是监听所有已经注册的Channel对象，等待I/O时间的发生。参数`timeoutMs`用于指定poll操作的超时时间，参数`activeChannels`则是一个输出参数，用于存放当前有事件发生的Channel对象。这里我们定义了一个vector来存放他。

- `void updateChannel(Channel* channel)`：这同样是一个纯虚函数，它用于更新Channel对象的监听事件。在事件循环开始之前，调用该函数向Poller对象注册新的或修改已有的Channel对象。如果Channel对象的监听事件发生了改变，那就要调用该函数进行更新。

- `void removeChannel(Channel* channel)`：这也是一个纯虚函数，它用于将Channel对象从Poller对象中移除。当一个Channel对象不再使用时，需要调用该函数将其从Poller对象中移除，避免占用资源。

这三个函数定义了Poller类与Channel类之间的接口，实现了事件的注册、注销、修改和分发。Poller类的派生类，也就是本项目实现的EpollPoller类需要实现这三个虚函数，以完成特定的分发机制。

## newDefaultPoller

这里因为Poller是一个抽象类，是无法直接创建对象的，他只能通过派生类创建对象，所以为了方便使用，提供一个newDefaultPoller静态函数，用于生成默认的Poller对象。这里是通过`MUDUO_USE_PILL`环境变量的值来判断使用哪一种I/O复用机制从而生成相应的Poller对象。如果为true就返回nullptr（因为本项目没有实现Poll实例），反之返回epoll实例。

```c++
Poller* Poller::newDefaultPoller(EventLoop* loop){
    if(::getenv("MUDUO_USE_PILL")){
        return nullptr; //   生成poll实例
    }
    else{
        return new EpollPoller(loop);  //  生成epoll实例
    }
}
```

## `EpollPoller`类的设计

### 成员变量

- `kInitEventListSize`：默认监听事件的数量
- `epollfd_`：使用poll_create创建的指向epoll对象的文件描述符（句柄）
- `events_`：返回的是事件，定义了一个vector来存放

```c++
static const int kInitEventListSize = 16;
using EventList = std::vector<epoll_event>;
int epollfd_;
EventList events_;
```

### 成员函数

#### 填写活跃的连接的方法

`fillActiveChannels`的作用就是将epoll_wait函数返回的就绪事件列表中的每一个事件对应的Channel添加到ChannelList中，这个ChannelList参数就是调用该函数传入的。具体实现过程如下：

1. 遍历事件列表，对于每个事件，都取出对应的Channel对象（我们在注册时就将Channel对象的指针存放在了vector容器存到的epoll_event结构体的data.ptr成员中了）。
2. 将事件列表中对应的Channel对象的revents_成员设置为该事件的events成员。
3. 将对应的Channel添加到ChannelList中。

```c++
void EpollPoller::fillActiveChannels(int numEvents, ChannelList *activechannels) const{
    for(int i = 0; i < numEvents; ++i){
        //  void* => Channel*
        Channel *channel = static_cast<Channel*>(events_[i].data.ptr);
        channel->set_revents(events_[i].events);
        activechannels->push_back(channel); //  EventLoop就拿到了它的poller给他返回的所有发生事件的channel列表
    }
}
```

#### 更新channel通道的方法

`update`用来更新epoll中某一个channel对象的状态。参数operation代表需要操作的类型，它的取值可以为：EPOLL_CTL_ADD、EPOLL_CTL_MOD 或 EPOLL_CTL_DEL，分别代表添加、修改和删除操作。参数channel则是要被更新状态的Channel对象。

```c++
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
```



#### 重写基类poller的抽象方法

这些函数是EpollPoller类继承Poller类需要重写的核心方法。

```c++
Timestamp poll(int timeoutMs, ChannelList *activechannels) override;
void updateChannel(Channel* channel) override;
void removeChannel(Channel* channel) override;
```

##### `poll()`方法

这个方法继承于`Poller`类中的`Timestamp poll(int timeoutMs, ChannelList *activeChannels)`它的作用前面已经讲过，这里就是在基于epoll下它的实现。

```c++
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
```

##### `updateChannel()方法`

这里有一个`index`它就是channel对象在epoll的状态，它的定义和Channel类中是一样的。

```c++
//  channel未添加到poller中
const int kNew = -1;    //  chanell 的成员index_初始化就是-1
//  channel以添加到poller中
const int kAdded = 1;
//  channel从poller中删除
const int kDeleted = 2;
```

```c++
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
```

#### `removeChannel()`方法

```c++
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
```

