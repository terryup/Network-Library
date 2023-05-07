#ifndef WEBSERVER_H
#define WEBSERVER_H

#include "TcpServer.h"
#include "logger.h"

#include <string>
#include <memory>

class HttpResponseTest;
class HttpRequestTest;


class WebServer : noncopyable{
public:
    using HttpCallback = std::function<void(const HttpRequestTest&,
        HttpResponseTest*)>;

    WebServer(EventLoop *loop,
        const InetAddress& listenAddr,
        const std::string& name,
        TcpServer::Option option = TcpServer::kNoReusePort);


    EventLoop* getLoop() const { return server_.getLoop(); }

    void setHttpCallback(const HttpCallback& cb){
        httpCallback_ = cb;
    }

    void start();


private:

    void onConnection(const TcpConnectionPtr& conn);

    void onMessage(const TcpConnectionPtr &conn,
        Buffer *buf,
       Timestamp receiveTime);
    void onRequest(const TcpConnectionPtr&, const HttpRequestTest&);

    TcpServer server_;
    HttpCallback httpCallback_;

};


#endif