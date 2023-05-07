#ifndef HTTPSERVER_TEST_H
#define HTTPSERVER_TEST_H

#include "HttpRequest_test.h"
#include "HttpResponse_test.h"
#include "TcpServer.h"
#include "logger.h"

#include <unordered_map>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <assert.h>
#include <string>

class Buffer;

class HttpServerTest{
public:
    HttpServerTest();
    ~HttpServerTest(){}


    void onRequest(const HttpRequestTest& req, HttpResponseTest* res);

public:
    //  生成响应报文的主函数
    void makeResponse(HttpResponseTest* response_) ;

    //在添加数据体的函数中，如果所请求的文件打不开，则需要返回相应的错误信息，由此实现
    void errorContent(HttpResponseTest* response_, std::string message);

private:
     //生成相应报文
    void addStateLine_(HttpResponseTest* response_);//生成请求行
    void addResponseHeader_(HttpResponseTest* response_);//生成请求头
    void addResponseContent_(HttpResponseTest* response_);//生成数据体

    void errorHTML_(HttpResponseTest* response_);//对于 4XX 的状态码是分开考虑的 由此函数实现
    //std::string getFileType_();//添加请求头的时候，我们需要得到文件类型信息 由此函数实现


    std::unordered_map<std::string, std::string> headers_;

    //  用户要请求的文件
    std::string path_;
    //  先为服务器所在文件的根目录   然后找到之后返回回去   
    std::string staticFilePath;

        //哈希表提供 4XX 状态码到响应文件路径的映射
    static const std::unordered_map<std::string, std::string> SUFFIX_TYPE;//后缀名到文件类型的映射关系
    static const std::unordered_map<int, std::string>CODE_STATUS;//状态码到相应状态 (字符串类型) 的映射
    static const std::unordered_map<int, std::string>CODE_PATH;//代码路径

    //由于使用了共享内存，所以需要变量和数据结构指示相关信息
    char* mmFile_;
    struct stat mmFileStat_;

};

#endif