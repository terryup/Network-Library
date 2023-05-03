#ifndef LOGGER_H
#define LOGGER_H

#include "lockqueue.h"
#include "noncopyable.h"

//  日志定义的级别  INFO    ERROR   FATAL   DEBUG
enum LogLevel{
    INFO,   //  普通信息
    ERROR,  //  错误信息
    FATAL,  //  core信息
    DEBUG   //  调试信息
};

//  输出一个日志类
class Logger : noncopyable{
public:
    //  获取日志唯一的实例对象
    static Logger& instance();

    //  设置日志级别
    void setLogLevel(int level);

    //  写日志的接口
    void log(std::string msg);

private:
    int loglevel_;  //  
    Logger();
    LockQueue<std::string> lockQue_;

};

//  定义宏  LOG_INFO("xxx %d %s", 20, "xxx")
#define LOG_INFO(logmsgformat, ...) \
    do { \
        Logger &logger = Logger::instance(); \
        logger.setLogLevel(INFO); \
        char c[1024] = {0}; \
        snprintf(c, 1024, logmsgformat, ##__VA_ARGS__); \
        logger.log(c); \
    } while (0)
    
#define LOG_ERR(logmsgformat, ...) \
    do { \
        Logger &logger = Logger::instance(); \
        logger.setLogLevel(ERROR); \
        char c[1024] = {0}; \
        snprintf(c, 1024, logmsgformat, ##__VA_ARGS__); \
        logger.log(c); \
    } while (0) 

#define LOG_FAT(logmsgformat, ...) \
    do { \
        Logger &logger = Logger::instance(); \
        logger.setLogLevel(FATAL); \
        char c[1024] = {0}; \
        snprintf(c, 1024, logmsgformat, ##__VA_ARGS__); \
        logger.log(c); \
        exit(-1);  \
    } while (0) 

#define LOG_DEB(logmsgformat, ...) \
    do { \
        Logger &logger = Logger::instance(); \
        logger.setLogLevel(DEBUG); \
        char c[1024] = {0}; \
        snprintf(c, 1024, logmsgformat, ##__VA_ARGS__); \
        logger.log(c); \
    } while (0) 

#endif