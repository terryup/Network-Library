#include "HttpServer_test.h"

#include <assert.h>
#include <fstream>

//枚举这些类型
const std::unordered_map<std::string, std::string> HttpServerTest::SUFFIX_TYPE = {
    { ".html",  "text/html" },
    { ".xml",   "text/xml" },
    { ".xhtml", "application/xhtml+xml" },
    { ".txt",   "text/plain" },
    { ".rtf",   "application/rtf" },
    { ".pdf",   "application/pdf" },
    { ".word",  "application/nsword" },
    { ".png",   "image/png" },
    { ".gif",   "image/gif" },
    { ".jpg",   "image/jpeg" },
    { ".jpeg",  "image/jpeg" },
    { ".au",    "audio/basic" },
    { ".mpeg",  "video/mpeg" },
    { ".mpg",   "video/mpeg" },
    { ".avi",   "video/x-msvideo" },
    { ".gz",    "application/x-gzip" },
    { ".tar",   "application/x-tar" },
    { ".css",   "text/css "},
    { ".js",    "text/javascript "},
};

const std::unordered_map<int, std::string> HttpServerTest::CODE_STATUS = {
    { 200, "OK" },
    { 400, "Bad Request" },
    { 403, "Forbidden" },
    { 404, "Not Found" },
};

const std::unordered_map<int, std::string> HttpServerTest::CODE_PATH = {
    { 400, "/400.html" },
    { 403, "/403.html" },
    { 404, "/404.html" },
};

HttpServerTest::HttpServerTest(){}


void HttpServerTest::onRequest(const HttpRequestTest& requst_, HttpResponseTest* response_){
    staticFilePath = std::string(getcwd(nullptr, 256)) + "/resources/";
    headers_ = requst_.headers_();
    path_ = requst_.getPath_();

    LOG_INFO("Headers %s %s", requst_.methodString_() , requst_.getPath_().c_str());

    for (auto &header : headers_){
        LOG_INFO("%s : %s", header.first.c_str(), header.second.c_str());
    }

    makeResponse(response_);

}

void HttpServerTest::makeResponse(HttpResponseTest* response_) {
    if (stat((staticFilePath + path_).data(), &mmFileStat_) < 0 || 
        S_ISDIR(mmFileStat_.st_mode)){
        response_->setStatusCode_(HttpResponseTest::k404NotFound);
    }
    else if(!(mmFileStat_.st_mode & S_IROTH)){
        response_->setStatusCode_(HttpResponseTest::kForbidden);
    }
    else if(response_->getStatusCode_() == HttpResponseTest::kUnknown){
        response_->setStatusCode_(HttpResponseTest::k200Ok);
    }

    errorHTML_(response_);
    addStateLine_(response_);
    addResponseHeader_(response_);
    addResponseContent_(response_);
}

void HttpServerTest::errorHTML_(HttpResponseTest* response_) {
    if (CODE_PATH.count(static_cast<int>(response_->getStatusCode_())) == 1){
        path_ = CODE_PATH.find(static_cast<int>(response_->getStatusCode_()))->second;
        stat((staticFilePath + path_).data(), &mmFileStat_);
    }
}

void HttpServerTest::addStateLine_(HttpResponseTest* response_){
    std::string status;
    if (CODE_STATUS.count(static_cast<int>(response_->getStatusCode_())) == 1){
        status = CODE_STATUS.find(static_cast<int>(response_->getStatusCode_()))->second;
    }
    else{
        response_->setStatusCode_(HttpResponseTest::k400BadRequest);
        status = CODE_STATUS.find(static_cast<int>(response_->getStatusCode_()))->second;
    }
    response_->setStatusMessage_(status);
}

void HttpServerTest::addResponseHeader_(HttpResponseTest* response_) {
    std::string::size_type idx = path_.find_last_of('.');
    if (idx == std::string::npos){
        response_->setContentType("text/plain");
        return;
    }
    std::string suffix = path_.substr(idx);
    if (SUFFIX_TYPE.count(suffix) == 1){
        response_->setContentType(SUFFIX_TYPE.find(suffix)->second);
        return;
    }
    response_->setContentType("text/plain");
    return;
}

void HttpServerTest::addResponseContent_(HttpResponseTest* response_){
    
    int srcFd = open((staticFilePath + path_).data(), O_RDONLY);
    if (srcFd < 0){
        errorContent(response_, "File NotFound!");
        return;
    }

    int* mmRet = (int*)mmap(0, mmFileStat_.st_size, PROT_READ,
        MAP_PRIVATE, srcFd, 0);
    if (*mmRet == -1){
        errorContent(response_, "File NotFound!");
        return;
    }
    mmFile_ = (char*)mmRet;
    close(srcFd);
    response_->setStatusCode_(HttpResponseTest::k200Ok);
    response_->addHeader_("Server", "mymuduo");
    //  这个位置出现bug，因为body_只能接收一个std::string类型的数组
    //  如果直接吧char* 类型的mmFile_传回去，那body_是无法接收的
    //  所以会导致无法传输图片类型的数据
    //  response_->setBody_(mmFile_);
    std::string content(mmFile_, mmFileStat_.st_size);
    response_->setBody_(content);
}

void HttpServerTest::errorContent(HttpResponseTest* response_, std::string message){
    std::string body;
    std::string status;
    body += "<html><tile>Error</title>";
    body += "<body bgcolor=\"ffffff\">";
    if (CODE_STATUS.count(static_cast<int>(response_->getStatusCode_())) == 1){
        status = CODE_STATUS.find(static_cast<int>(response_->getStatusCode_()))->second;
    }
    else{
        status = "Bad Request";
    }
    body += std::to_string(static_cast<int>(response_->getStatusCode_())) + " : " + status + "\n";
    body += "<p>" + message + "</p>";
    body += "<hr><em>TinyWebServer</em></body></html>";

    response_->setBody_(body);
}
