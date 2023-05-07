#include "TcpConnection.h"
#include "logger.h"
#include "Socket.h"
#include "Channel.h"
#include "EventLoop.h"

#include <functional>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <strings.h>
#include <netinet/tcp.h>
#include <string>

//  检查传入的 baseLoop 指针是否有意义
static EventLoop *CheckLoopNotNull(EventLoop *loop){
    if(loop == nullptr){
        LOG_FAT("TcpConnection is null! \n");
    }
    return loop;
}

TcpConnection::TcpConnection(EventLoop *loop, 
        const std::string &nameArg, 
        int sockfd, 
        const InetAddress& localaddr, 
        const InetAddress& peeraddr)
    : loop_(CheckLoopNotNull(loop))
    , name_(nameArg) 
    , state_(kConnecting)
    , reading_(true)
    , socket_(new Socket(sockfd))
    , channel_(new Channel(loop, sockfd))
    , localAddr_(localaddr)
    , peerAddr_(peeraddr)
    , highWaterMark_(64 * 1024 * 1024) //   64M
{
    //  下面给channel设置相应的回调函数 poller给channel通知感兴趣的事件发生了 channel会回调相应的回调函数
    channel_->setReadCallback(std::bind(&TcpConnection::handleRead, this, std::placeholders::_1));
    channel_->setWriteCallback(std::bind(&TcpConnection::handleWrite, this));
    channel_->setCloseCallback(std::bind(&TcpConnection::handleClose, this));
    channel_->setErrorCallback(std::bind(&TcpConnection::handleError, this));

    LOG_INFO("TcpConnection::ctor[%s]at fd = %d \n", name_.c_str(), sockfd);
    socket_->setKeepAlive(true);
}

TcpConnection::~TcpConnection(){
    LOG_INFO("TcpConnection::dtor[%s]at fd = %d state = %d \n", name_.c_str(), channel_->fd(), (int)state_);
}

//  发数据
void TcpConnection::send(const std::string &buf){
    if (state_ == kConnected){
        if (loop_->isInLoopThread()){
            sendInLoop(buf.c_str(), buf.size());
        }
        else{
            //  sendInLoop有多重重载，需要使用函数指针确定
            void(TcpConnection::*fp)(const void* data, size_t len) = &TcpConnection::sendInLoop;
            loop_->runInLoop(std::bind(
                fp,
                this,
                buf.c_str(),
                buf.size()
            ));
        }
    }
}

void TcpConnection::send(Buffer *buf){
    if (state_ == kConnected){
        if (loop_->isInLoopThread()){
            sendInLoop(buf->peek(), buf->readableBytes());
            buf->retrieveAll();
        }
        else{
            //  sendInLoop有多重重载，需要使用函数指针确定
            void (TcpConnection::*fp)(const std::string& message) = &TcpConnection::sendInLoop;
            loop_->runInLoop(std::bind(fp, this, buf->retrieveAllAsString()));
        }
    }
}

void TcpConnection::sendInLoop(const std::string& message){
    sendInLoop(message.data(), message.size());
}

/*
    发送数据    应用写的快，而内核发送数据慢，需要吧待发送数据写入缓冲区，而且设置了水位回调
*/
void TcpConnection::sendInLoop(const void* data, size_t len){
    ssize_t nwrote = 0;
    size_t remaining = len;
    bool faultError = false;

    //  之前调用过该connection的shutdown，不能在进行发送了
    if (state_ == kDisConnected){
        LOG_ERR("disconnected, give up writing! \n");
        return;
    }

    //  channel第一次写数据，且缓冲区没有待发送数据
    if (!channel_->isWriting() && oututBuffer_.readableBytes() == 0){
        nwrote = ::write(channel_->fd(), data, len);
        if (nwrote >= 0){
            //  判断有没有一次性写完
            remaining = len - nwrote;
            if (remaining == 0 && writeCompleteCallback_){
                //  既然一次性发送完事件就不用让channel对epollout事件感兴趣了
                loop_->queueInLoop(std::bind(writeCompleteCallback_, shared_from_this()));
            }
        }
        //  nwrote < 0
        else{
            nwrote = 0;
            if (errno != EWOULDBLOCK){
                LOG_ERR("TcpConnection::sendInLoop() \n");
                //  SIGPIPE
                if (errno == EPIPE || errno == ECONNRESET){
                    faultError = true;
                }
            }
        }
    }

    //  说明一次性并没有发送完数据，剩余数据需要保存到缓冲区中，且需要改channel注册写事件
    //  注册epollout事件，poller发现tcp的发送缓冲区有空间，会通知相应的sock-channel，然后调用handleWrite回调方法
    //  最终也就是调用TcpConnection::handleWrite()方法，吧发送缓冲区数据全部发送完成
    if(!faultError && remaining > 0){
        //  目前发送缓冲区剩余的待发送的数据的长度
        size_t oldLen = oututBuffer_.readableBytes();
        if (oldLen + remaining >= highWaterMark_ && 
            oldLen < highWaterMark_ &&
            highWaterMarkCallback_){
            loop_->queueInLoop(std::bind(highWaterMarkCallback_, shared_from_this(), oldLen + remaining));
        }
        oututBuffer_.append((char *)data + nwrote, remaining);
        if (channel_->isWriting()){
            //  这里一定要注册channel的写事件 否则poller不会给channel通知epollout
            channel_->enableWriting();
        }
    }
}

void TcpConnection::handleRead(Timestamp receiveTime){
    int saveErrno = 0;
    //  TcpConnection会从socket读取数据，然后写入inpuBuffer
    ssize_t n = inputBuffer_.readFd(channel_->fd(), &saveErrno);
    //  已建立连接的用户，有可读事件发生，调用用户传入的回调操作
    if (n > 0){
        messageCallback_(shared_from_this(), &inputBuffer_, receiveTime);
    }
    //  没有数据，说明客户端关闭连接
    else if (n == 0){
        handleClose();
    }
    //  出错情况
    else{
        errno = saveErrno;
        LOG_ERR("TcpConnection::handleRead() failed \n");
        handleError();
    }
}

//  关闭连接
void TcpConnection::shutdown(){
    if (state_ == kConnected){
        setState(kDisConnecting);
        loop_->queueInLoop(std::bind(&TcpConnection::shutdownInLoop, this));
    }
}

void TcpConnection::shutdownInLoop(){
    //  说明当前outputBuffer_的数据全部向外发送完成
    if (!channel_->isWriting()){
        socket_->shutdownWrite();
    }
}

//  连接建立
void TcpConnection::connectEstablished(){
    //  建立连接，设置一开始状态为连接态
    setState(kConnected);
    /*
        tie相当于在底层有一个强引用指针记录着，防止析构
        为了防止TcpConnection这个资源被误删掉，而这个时候还有许多事件要处理
        channel->tie 会进行一次判断，是否将弱引用指针变成强引用，变成得话就防止了计数为0而被析构得可能
    */
    channel_->tie(shared_from_this());
    channel_->enableReading();  //  向poller注册EPOLLIN事件

    //  新连接建立，执行回调
    connectionCallback_(shared_from_this());
}  

//  连接销毁
void TcpConnection::connectDestroyed(){
    if (state_ == kConnected){
        setState(kDisConnected);
        //  把channel的所有感兴趣的事件从poller中删除掉
        channel_->disableAll();
        connectionCallback_(shared_from_this());
    }
    //  把channel从poller中删除掉
    channel_->remove();
}

void TcpConnection::handleWrite(){
    if (channel_->isWriting()){
        int saveErrno = 0;
        ssize_t n = oututBuffer_.writFd(channel_->fd(), &saveErrno);
        //  正确读取数据
        if (n > 0){
            oututBuffer_.retrieve(n);
            //  说明buffer可读数据都被TcpConnection读取完毕并写入给了客户端
            //  此时就可以关闭连接，否则还需继续提醒写事件
            if (oututBuffer_.readableBytes() == 0){
                channel_->disableWriting();
                //  调用用户自定义的写完数据处理函数
                if (writeCompleteCallback_){
                    //  唤醒loop_对应得thread线程，执行写完成事件回调
                    loop_->queueInLoop(std::bind(writeCompleteCallback_, shared_from_this()));
                }
                if(state_ == kDisConnecting){
                    shutdownInLoop();
                }
            }
        }
        else{
            LOG_ERR("TcpConnection::handleWrite() failed \n");
        }
    }
    //  state_不为写状态
    else{
        LOG_ERR("TcpConnection fd=%d is down, no more writing \n", channel_->fd());
    }
}

void TcpConnection::handleClose(){
    LOG_INFO("fd=%d state=%d \n", channel_->fd(), (int)state_);
    //  设置状态为关闭连接状态
    setState(kDisConnected);
    //  注销Channel所有感兴趣事件
    channel_->disableAll();

    TcpConnectionPtr connPtr(shared_from_this());
    connectionCallback_(connPtr);
    //  关闭连接得回调
    //  执行的是TcpServer::removeConnection回调方法
    closeCallback_(connPtr);
}

void TcpConnection::handleError(){
    int optval;
    socklen_t optlen = sizeof(optval);
    int err = 0;
    if (::getsockopt(channel_->fd(), SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0){
        err = errno; 
    }
    else {
        err = optval;
    }
    LOG_ERR("TcpConnection::handleError() name:%s - SO_ERROR:%d", name_.c_str(), err);
}