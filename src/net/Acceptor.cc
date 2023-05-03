#include "Acceptor.h"
#include "logger.h"
#include "InetAddress.h"

#include <functional>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>

static int createNonbloking(){
    int sockfd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_TCP);
    if (sockfd < 0){
        LOG_FAT("listen socket create err:%d \n", errno);
    }
    return sockfd;
}

Acceptor::Acceptor(EventLoop *loop, const InetAddress &listenAddr, bool reuseport)
    : loop_(loop)
    , acceptSocket_(createNonbloking())
    , acceptChannel_(loop, acceptSocket_.fd())
    , listenning_(false) {
    acceptSocket_.setReuseAddr(true);
    acceptSocket_.setReusePort(true);
    acceptSocket_.bindAddress(listenAddr);  //  bind绑定套接字
    //  TcpServer::start()  Acceptor.listen
    //  如果有新用户的连接，要执行一个回调(connfd => channel => ubloop)
    //  baseLoop_ => acceptChannel_(listenfd) => 
    acceptChannel_.setReadCallback(std::bind(&Acceptor::handleRead, this));

}

Acceptor::~Acceptor(){
    //  把从Poller中感兴趣的事件删除掉
    acceptChannel_.disableAll();
    //  调用EventLoop->removeChannel => Poller->removeChannel 把Poller的ChannelMap对应的部分删除
    acceptChannel_.remove();
}

void Acceptor::listen(){
    listenning_ = true;
    acceptSocket_.listen();
    //  将acceptChannel的读事件注册到poller
    acceptChannel_.enableReading();
}

//  listenfd有事件发生了，就是有新用户连接了
void Acceptor::handleRead(){
    InetAddress peerAddr;
    //  接受新连接
    int connfd = acceptSocket_.accept(&peerAddr);
    //  确实有新连接到来
    if (connfd >= 0){
        //  TcpServer::NewConnectionCallback_
        if (newConnectionCallback_){
            //  轮询找到subLoop 唤醒并分发当前的新客户端的Channel
            newConnectionCallback_(connfd, peerAddr);
        }
        else{
            ::close(connfd);
        }
    }
    else{
        LOG_ERR("accept err:%d \n", errno);

        //  当前进程的fd已经用完
        //  可以调整单个服务器的fd上限
        //  也可以分布式部署
        if (errno == EMFILE){
            LOG_ERR("sockfd reached limit \n");
        }
    }
}