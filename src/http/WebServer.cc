#include "HttpResponse_test.h"
#include "HttpRequest_test.h"
#include "HttpContext.h"
#include "WebServer.h"

#include <iostream>
#include <memory>

void defaultHttpCallback(const HttpRequestTest&, HttpResponseTest* resp)
{
    resp->setStatusCode_(HttpResponseTest::k404NotFound);
    resp->setStatusMessage_("Not Found");
    resp->setIsKeepAlive_(true);
}

WebServer::WebServer(EventLoop *loop,
                      const InetAddress &listenAddr,
                      const std::string &name,
                      TcpServer::Option option)
  : server_(loop, listenAddr, name, option)
  , httpCallback_(defaultHttpCallback){
    server_.setConnectionCallback(std::bind(
        &WebServer::onConnection, this, std::placeholders::_1
    ));
    server_.setMessageCallback(std::bind(
        &WebServer::onMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3
    ));
    server_.setThreadNum(4);
}

void WebServer::start()
{
    LOG_INFO("WebServer[%s]starts listening on %s \n", server_.name().c_str(), server_.ipPort().c_str());
    server_.start();
}

void WebServer::onConnection(const TcpConnectionPtr& conn)
{
    if (conn->connected())
    {
        LOG_INFO("new Connection arrived \n");
    }
    else 
    {
        LOG_INFO("Connection closed \n");
    }
}

void WebServer::onMessage(const TcpConnectionPtr& conn,
                           Buffer* buf,
                           Timestamp receiveTime) {
    std::unique_ptr<HttpContext> context(new HttpContext);

    if (!context->parseRequest(buf, receiveTime)) {
        LOG_INFO("parseRequest failed!\n");
        conn->send("HTTP/1.1 400 Bad Request\r\n\r\n");
        conn->shutdown();
    } 

    if (context->gotAll()){
        LOG_INFO("parseRequest success!\n");
        onRequest(conn, context->getRequest());
        context->reset();
    }
}

void WebServer::onRequest(const TcpConnectionPtr& conn, const HttpRequestTest& req){
    const std::string& connection = req.getHeader_("Connection");

    bool isKeepAlive = connection == "close" || 
        (req.getVersion_() == HttpRequestTest::kHttp10 
            && connection != "Keep-Alive");

    HttpResponseTest response(isKeepAlive);
    httpCallback_(req, &response);
    Buffer buf;
    response.appendToBuffer(&buf);
    conn->send(&buf);
    if (response.getIsKeepAlive_()){
        conn->shutdown();
    }
}