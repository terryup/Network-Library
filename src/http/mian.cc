#include "WebServer.h"
#include "TcpServer.h"
#include "HttpServer_test.h"

int main(int argc, char* argv[])
{
    HttpServerTest httpServer;
    EventLoop loop;
    WebServer server(&loop, InetAddress(8080), "http-server");
    server.setHttpCallback(std::bind(
        &HttpServerTest::onRequest, &httpServer, std::placeholders::_1, std::placeholders::_2));
    server.start();
    loop.loop();
}