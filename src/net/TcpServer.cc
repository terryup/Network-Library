#include "TcpServer.h"
#include "logger.h"
#include "TcpConnection.h"

#include <functional>
#include <strings.h>

//  检查传入的 baseLoop 指针是否有意义
static EventLoop *CheckLoopNotNull(EventLoop *loop){
    if(loop == nullptr){
        LOG_FAT("mainLoop is null!");
    }
    return loop;
}

TcpServer::TcpServer(EventLoop *loop, const InetAddress &listenAddr, const std::string& nameArg, Option option)
    : loop_(CheckLoopNotNull(loop))
    , ipPort_(listenAddr.toIpPort())
    , name_(nameArg)
    , acceptor_(new Acceptor(loop, listenAddr, option == kReusePort))
    , threadPool_(new EventLoopThreadPool(loop, name_))
    , connectionCallback_()
    , messageCallback_()
    , writeCompleteCallback_()
    , threadInitCallback_()
    , nextConnId_(1)
    , started_(0) {
    //  当有新用户连接时，Acceptor类中绑定的acceptChannel_会有读事件发生执行handleRead()调用TcpServer::newConnection回调
    acceptor_->setNewConnectionCallback(std::bind(&TcpServer::newConnection, this, std::placeholders::_1, std::placeholders::_2));
}

TcpServer::~TcpServer(){
    for (auto & item : connections_){
        //  这个局部的shared_ptr只能指针对象，出右括号可以自动释放new出来的TcpConnection对象资源
        TcpConnectionPtr conn(item.second);
        item.second.reset();
        conn->getLoop()->runInLoop(std::bind(&TcpConnection::connectDestroyed, conn));
    }
}

//  设置底层subloop的个数
void TcpServer::setThreadNum(int numThreads){
    threadPool_->setThreadNum(numThreads);
}

//  开启服务器监听
void TcpServer::start(){
    //  防止一个TcpServer对象被start多次
    if (started_++ == 0){
        //  启动底层的loop线程池
        threadPool_->start(threadInitCallback_);
        //  acceptor_.get()绑定时候需要地址
        loop_->runInLoop(std::bind(&Acceptor::listen, acceptor_.get()));
    }
}

//  有一个新的客户端的连接，acceptor会执行这个回调操作
void TcpServer::newConnection(int sockfd, const InetAddress &peerAddr){
    //  轮询算法选择一个subLoop，来管理channel
    EventLoop *ioLoop = threadPool_->getNextLoop();
    char buf[64] = {0};
    snprintf(buf, sizeof(buf), "-%s#%d", ipPort_.c_str(), nextConnId_);
    ++nextConnId_;
    std::string connName = name_ + buf;

    LOG_INFO("TcpServer::newConnection [%s] - new connection [%s] from %s"
        , name_.c_str(), connName.c_str(), peerAddr.toIpPort().c_str());
    
    //  通过sockfd获取其绑定的本机的ip地址和端口信息
    sockaddr_in local;
    ::bzero(&local, sizeof(local));
    socklen_t addrlen = sizeof(local);
    if (::getsockname(sockfd, (sockaddr*)&local, &addrlen) < 0){
        LOG_ERR("sockets::getLocalAddr() failed \n");
    }
    InetAddress localAddr(local);

    //  根据连接成功的sockfd，创建Tcp Connection连接对象
    TcpConnectionPtr conn(new TcpConnection(ioLoop, connName, sockfd, localAddr, peerAddr));
    connections_[connName] = conn;
    //  下面的回调都是用户设置给Tcp Server => TcpConnection => Channel => Poller => notify => channel调用回调
    conn->setConnectionCallback(connectionCallback_);
    conn->setMessageCallback(messageCallback_);
    conn->setWriteCompleteCallback(writeCompleteCallback_);

    //  设置了如何关闭连接的回调
    conn->setCloseCallback(std::bind(&TcpServer::removeConnection, this, std::placeholders::_1));

    //  直接调用TcpConnection::connectEstablished
    ioLoop->runInLoop(std::bind(&TcpConnection::connectEstablished, conn));

}

void TcpServer::removeConnection(const TcpConnectionPtr &conn){
    loop_->runInLoop(std::bind(&TcpServer::removeConnectionInLoop, this, conn));
}

void TcpServer::removeConnectionInLoop(const TcpConnectionPtr &conn){
    LOG_INFO("TcpServer::removeConnectionInLoop [%s] - connection %s", name_.c_str(), conn->name().c_str());

    connections_.erase(conn->name());
    EventLoop *ioLoop = conn->getLoop();
    ioLoop->queueInLoop(std::bind(&TcpConnection::connectDestroyed, conn));
}
