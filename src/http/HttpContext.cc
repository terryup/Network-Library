#include "HttpContext.h"
#include "Buffer.h"
#include "logger.h"

#include <regex>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <assert.h>

//  解析HTTP请求
bool HttpContext::parseRequest(Buffer* buff, Timestamp receiveTime){
    //  自动机未完成并且缓冲区可写入的的字节数不为0
    const char CRLF[] = "\r\n";
    if(buff->readableBytes() <= 0){
        LOG_ERR("readableBytes Less than or equal to 0! \n");
        return false;
    }

    while(buff->readableBytes() && state_ != FINISH){
        const char* lineEnd = std::search(buff->peek(),
            buff->beginWriteConst(), CRLF, CRLF + 2);
        std::string line(buff->peek(), lineEnd);
        switch ((state_))
        {
        case REQUEST_LINE:  //  解析请求行
            LOG_INFO("REQUEST:%s \n", line.c_str());
            if(!parseRequestLine_(line)){
                LOG_ERR("parseRequestLine_ error! \n");
                return false;
            }
            parsePath_();
            break;
        case HEADERS:   //  解析请求头
            parseRequestHeader_(line);
            if(buff->readableBytes() <= 2){
                state_ = FINISH;
            }
            break;
        case BODY:  //  解析请求体
            parseDataBody_(line);
            break;
        default:
            break;
        }
        if(lineEnd == buff->beginWrite()){
            break;
        }
        buff->retrieveUntil(lineEnd + 2);
    }
    return true;
}

//在解析请求行的时候，会解析出路径信息，之后还需要对路径信息做一个处理
void HttpContext::parsePath_(){
    if(request_.getPath_() == "/"){
        request_.setPath_("/index.html");
    }
    else{
        for(auto & item : request_.GET_DEFAULT_HTML_()){
            if(item == request_.getPath_()){
                request_.push_back_setPath_(".html");
                break;
            }
        }
    }
}

//解析请求行
//GET /mix/76.html?name=kelvin&password=123456 HTTP/1.1
//用正则表达式，^([GET]*) ([/mix/76.html?name=kelvin&password=123456]*) HTTP/([1.1]*)$
bool HttpContext::parseRequestLine_(const std::string& line){
    std::regex patten("^([^ ]*) ([^ ]*) HTTP/([^ ]*)$");
    std::smatch subMatch;
    if(regex_match(line, subMatch, patten)){//如果匹配成功了(没有出错)
        // HTTP 方式 为GET
        if (subMatch[1] == "GET"){
            request_.setMethod_(HttpRequestTest::kGet);
        }
        request_.setPath_(subMatch[2]);//路径 为/mix/76.html?name=kelvin&password=123456
        if(subMatch[3] == "1.1"){
            //  版本 为 1.1
            request_.setVersion_(HttpRequestTest::kHttp11);
        }
        else if (subMatch[3] == "1.0"){
            //  版本 为 1.0
            request_.setVersion_(HttpRequestTest::kHttp10);
        }
        state_ = HEADERS;//状态机改为  当前正在分析头部字段
        return true;
    }
    return false;
}

//解析请求头
//Host: www.baidu.com   
//用正则表达式，^([Host:]*): ?(www.baidu.com)$
void HttpContext::parseRequestHeader_(const std::string& line){
    std::regex patten("^([^:]*): ?(.*)$");
    std::smatch subMatch;
    if(regex_match(line, subMatch, patten)){
        request_.addHeader_(subMatch[1], subMatch[2]);//请求头为 www.baidu.com
    }
    else{
        state_ = BODY;//状态机改为  当前正在解析请求体
    }
}

//解析请求体,同样没有真正的解析HTTP请求的请求体
void HttpContext::parseDataBody_(const std::string& line){
    request_.setBody_(line);
    parsePost_();
    state_ = FINISH;
}

//转换 Hex 格式的函数
int HttpContext::convertHex(char ch){
    if(ch >= 'A' && ch <= 'F'){
        return ch - 'A' + 10;
    }
    if(ch >= 'a' && ch <= 'f'){
        return ch - 'a' + 10;
    }
    return ch;
}

//在处理数据体的时候，如果格式是 post，那么还需要解析 post 报文，用函数 parsePost 实现
void HttpContext::parsePost_(){
    if(request_.getmMthod_() == HttpRequestTest::kPost && request_.getHeader_("Content-Type") == 
        "application/x-www-form-urlencoded"){

        if(request_.getBody_().size() == 0){
           return;
        }

        std::string key, value;
        int num = 0;
        int n = request_.getBody_().size();
        int i = 0, j = 0;
        std::string body = request_.getBody_();
        for(; i < n; ++i){
            char ch = body[i];
            switch (ch)
            {
            case '=':
                key = body.substr(j, i - j);
                j = i + 1;
                break;
            case '+':
                body[i] = ' ';
                break;
            case '%':
                num = convertHex(body[i + 1]) * 16 + convertHex(body[i + 2]);
                body[i + 2] = num % 10 + '0';
                body[i + 1] = num / 10 + '0';
                i += 2;
                break;
            case '&':
                value = body.substr(j, i - j);
                j = i + 1;
                request_.addPost_(key, value);
                break;
            default:
                break;
            }
        }
        request_.setBody_(body);
    }
}