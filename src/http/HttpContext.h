#ifndef HTTPCONTEXT_H
#define HTTPCONTEXT_H

#include "copyable.h"
#include "HttpRequest_test.h"

class Buffer;

class HttpContext : public copyable{
public:
    /*
        解析客户端请求时，主状态机的状态
        PARSE_STATE_REQUEST_LINE:当前正在分析请求行
        PARSE_STATE_HEADER:当前正在分析头部字段
        PARSE_STATE_BODY:当前正在解析请求体
        PARSE_STATE_FINISH:是否完成
    */
    enum PARSE_STATE{REQUEST_LINE, HEADERS, BODY, FINISH};

    HttpContext()
        : state_(REQUEST_LINE){}

    //  解析HTTP请求
    bool parseRequest(Buffer* buff, Timestamp receiveTime);

    bool gotAll() const {
        return state_ == FINISH;
    }

    void reset(){
        state_ = REQUEST_LINE;
        HttpRequestTest dummy;
        request_.swap(dummy);
    }

    const HttpRequestTest& getRequest() const {
        return request_;
    }

    HttpRequestTest& getRequest() {
        return request_;
    }

private:

    //解析请求行
    bool parseRequestLine_(const std::string& line);
    //解析请求头
    void parseRequestHeader_(const std::string& line);
    //解析请求体
    void parseDataBody_(const std::string& line);

    //在解析请求行的时候，会解析出路径信息，之后还需要对路径信息做一个处理
    void parsePath_();

    //在处理数据体的时候，如果格式是 post，那么还需要解析 post 报文，用函数 parsePost 实现
    void parsePost_();

    //转换 Hex 格式的函数
    static int convertHex(char ch);

    HttpRequestTest request_;    //  封装了request
    PARSE_STATE state_; //  实现状态机




};


#endif