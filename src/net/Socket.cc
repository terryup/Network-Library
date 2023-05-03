#include "Socket.h"
#include "logger.h"
#include "InetAddress.h"

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <strings.h>
#include <netinet/tcp.h>

Socket::~Socket(){
    close(sockfd_);
}

void Socket::bindAddress(const InetAddress& localaddr){
    if (bind(sockfd_, (sockaddr*)localaddr.getSockAddr(), sizeof(sockaddr_in)) != 0){
        LOG_FAT("bind sockfd:%d fail \n", sockfd_);
    }
}

void Socket::listen(){
    if (::listen(sockfd_, 1024) != 0){
        LOG_FAT("listen sockfd:%d fail \n", sockfd_);
    }
}

int Socket::accept(InetAddress *peeraddr){
    /*
        1.accept函数的参数不合法
        2.对返回的connfd没有设置非阻塞
        Reactor模型 one loop per thread
        poller + non-blockingio
    */
    sockaddr_in addr;
    socklen_t len = sizeof(addr);
    bzero(&addr, sizeof(addr));
    int connfd = ::accept4(sockfd_, (sockaddr*)&addr, &len, SOCK_NONBLOCK | SOCK_CLOEXEC);
    if (connfd >= 0){
        peeraddr->getSockAddr();
    }
    return connfd;
}

//  socket的各个设置的用法
void Socket::shutdownWrite(){
    //  关闭写端，但是可以接受客户端数据
    if (::shutdown(sockfd_, SHUT_WR) < 0){
        LOG_ERR("sockets::shutdownWrite error!");
    }
}

//  不启动Nagle算法
void Socket::setTcpNoDelay(bool on){
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval));

}

//  设置地址复用，其实就是可以使用处于Time-wait的端口
void Socket::setReuseAddr(bool on){
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
}

//  通过改变内核信息，多个进程可以绑定同一个地址。通俗就是多个服务的ip+port是一样
void Socket::setReusePort(bool on){
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
}

void Socket::setKeepAlive(bool on){
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval));
}