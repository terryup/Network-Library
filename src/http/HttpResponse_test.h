#ifndef HTTPRESPONSE_TEST_H
#define HTTPRESPONSE_TEST_H

#include "copyable.h"

#include <unordered_map>
#include <sys/stat.h>

class Buffer;

class HttpResponseTest : public copyable{
public:
    enum HttpStatusCode{
    kUnknown,
    k200Ok = 200,
    k400BadRequest = 400,
    kForbidden = 403,
    k404NotFound = 404,
    };

    HttpResponseTest(bool close);

    void setStatusCode_(HttpStatusCode code) {
        code_ = code;
    }

    HttpStatusCode getStatusCode_() const {
        return code_;
    }

    void setStatusMessage_(const std::string& message){
        statusMessage_ = message;
    }

    const std::string& getStatusMessage_() const {
        return statusMessage_;
    }

    void setIsKeepAlive_(bool on){
        isKeepAlive_ = on;
    }

    bool getIsKeepAlive_() const {
        return isKeepAlive_;
    }

    void setContentType(const std::string& contentType){
        addHeader_("Content-Type", contentType);
    }

    void addHeader_(const std::string& key, const std::string& value){
        header_[key] = value;
    }

    const std::string& getBody_() const {
        return body_;
    }

    void setBody_(const std::string& body){
        body_ = body;
    }

    void setMmFileState_(struct stat mmFileStat){
        mmFileStat_ = mmFileStat;
    }

    void appendToBuffer(Buffer* output) const;

    std::string body_;
private:

    std::unordered_map<std::string, std::string> header_;   //  存储请求头
    std::unordered_map<std::string, std::string> post_; //  储存 post 已经解析出来的信息

    HttpStatusCode code_;  //  代表 HTTP 的状态
    std::string statusMessage_;

    bool isKeepAlive_;  //  是否为长连接

    struct stat mmFileStat_;

};




#endif