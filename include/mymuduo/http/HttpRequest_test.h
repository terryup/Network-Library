#ifndef HTTPREQUEST_TEST_H
#define HTTPREQUEST_TEST_H

#include "copyable.h"
#include "Timestamp.h"

#include <unordered_map>
#include <unordered_set>
#include <assert.h>

class Buffer;

class HttpRequestTest : public copyable{
public:
    /*
        请求方法
        目前仅支持GET，POST请求
    */
    enum Method { kInvalid, kGet, kPost, kHead, kPut, kDelete};

    /*
        HTTP的版本号
    */
    enum Version { kUnknown, kHttp10, kHttp11 }; 

    HttpRequestTest()
        : method_(kInvalid)
        , version_(kUnknown)
        , DEFAULT_HTML_({ "/index", "/welcome", "/video", "/picture" }){
            header_.clear();
            post_.clear();
            body_ = "";
        }
    
    void setVersion_(Version v){
        version_ = v;
    }

    Version getVersion_() const {
        return version_;
    }

    bool setMethod_ (Method m) {
        assert(m != kInvalid);
        method_ = m;
        return method_ != kInvalid;
    }

    Method getmMthod_() const{
        return method_;
    }

    const char* methodString_() const{
        const char* result = "UNKNOWN";
        switch(method_){
            case kGet:
                result = "GET";
                break;
            case kPost:
                result = "POST";
                break;
            case kHead:
                result = "HEAD";
                break;
            case kPut:
                result = "PUT";
                break;
            case kDelete:
                result = "DELETE";
                break;
            default:
                break;
        }
        return result;
    }

    void setPath_(const std::string& path){
        path_ = path;
    }

    void push_back_setPath_(const std::string& path){
        path_ += path;
    }

    const std::string& getPath_() const {
        return path_;
    }

    void setQuery_(const std::string& query){
        query_ = query;
    }

    const std::string& getQuery_() const {
        return query_;
    }

    void setReceiveTime__(Timestamp t){
        receiveTime_ = t;
    } 

    Timestamp getReceiveTime_() const {
        return receiveTime_;
    }

    void addHeader_(const std::string& key, const std::string& value){
        header_[key] = value;
    }

    void addPost_ (const std::string& key, const std::string& value) {
        post_[key] = value;
    }

    std::string getHeader_(const std::string& field) const {
        std::string ret;
        auto it = header_.find(field);
        if(it != header_.end()){
            ret = it->second;
        }
        return ret;
    }

    void setBody_(const std::string& body){
        body_ = body;
    }

    const std::string& getBody_() const {
        return body_;
    }

    const std::unordered_map<std::string, std::string> headers_() const {
        return header_;
    }


    const std::unordered_set<std::string> GET_DEFAULT_HTML_() const {
        return DEFAULT_HTML_;
    }

    void swap(HttpRequestTest that){
        std::swap(method_, that.method_);
        std::swap(version_, that.version_);
        path_.swap(that.path_);
        receiveTime_.swap(that.receiveTime_);
        query_.swap(that.query_);
        header_.swap(that.header_);
    }

    bool isKeepAlive() const {
        if (header_.count("Connection") == 1){
            return header_.find("Connection")->second == "keep-alive" && version_ == kHttp11;
        }
        return false;
    }

    std::string getPost_(const std::string& key) const {
        if(post_.count(key) == 1){
            return post_.find(key)->second;
        }
        return "";
    }

    std::string getPost_(const char* key) const {
        if(post_.count(key) == 1){
            return post_.find(key)->second;
        }
        return "";
    }
    
private:

    Method method_; //  请求方法
    Version version_;   //  协议版本号
    std::string path_, query_;   //  路径
    std::string body_;  //  请求体
    Timestamp receiveTime_; //  请求时间
    std::unordered_map<std::string, std::string> header_;   //  存储请求头
    std::unordered_map<std::string, std::string> post_; //  储存 post 已经解析出来的信息

    const std::unordered_set<std::string> DEFAULT_HTML_; //  存储了默认的网页名称

};




#endif