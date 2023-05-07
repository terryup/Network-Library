#include "HttpResponse_test.h"
#include "Buffer.h"

#include <string>

HttpResponseTest::HttpResponseTest(bool close)
        : code_(kUnknown)
        , isKeepAlive_(close){}

void HttpResponseTest::appendToBuffer(Buffer* output) const{
    char buf[64];
    snprintf(buf, sizeof(buf), "HTTP/1.1 %d ", code_);
    output->append(buf);
    output->append(statusMessage_);
    output->append("\r\n");

    if (isKeepAlive_){
        output->append("Connection: close\r\n");
    }
    else{
        snprintf(buf, sizeof(buf), "Content-Length: %zd\r\n", body_.size());
        output->append(buf);
        //output->append("Content-length: " + std::to_string(body_.size()) + "\r\n\r\n");
        output->append("Connection: Keep-Alive\r\n");
    }

    /// 响应头部
    for (const auto& header : header_){
        output->append(header.first);
        output->append(": ");
        output->append(header.second);
        output->append("\r\n");
    }
    output->append("\r\n");
    //  设置body
    output->append(body_);

}
