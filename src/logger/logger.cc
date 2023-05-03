#include "logger.h"
#include "Timestamp.h"

#include <time.h>
#include <iostream>


//  获取日志唯一的实例对象
Logger& Logger::instance(){
    static Logger logger;
    return logger;
}

//  设置日志级别
void Logger::setLogLevel(int level){
    loglevel_ = level;
}

//  写日志的接口
void Logger::log(std::string msg){
    lockQue_.Push(msg);
}

Logger::Logger(){
        //  启动专门写日志的线程
    std::thread writeLogTask([&](){
        for(;;){
            //  获取当天的日期，然后取日志信息写入相应的日志文件当中 a+
            time_t now = time(nullptr);
            tm *nowtm = localtime(&now);

            char file_name[128];
            sprintf(file_name, "%d-%d-%d-log.txt", nowtm->tm_year + 1900, nowtm->tm_mon + 1, nowtm->tm_mday);

            FILE *pf = fopen(file_name, "a+");
            if(pf == nullptr){
                std::cout << "logger file:" << file_name << "open error!" << std::endl;
                exit(EXIT_FAILURE);
            }

            std::string msg = lockQue_.Pop();
            std::string time_buf = Timestamp::now().toString();
            //char time_buf[128] = Timestamp::now().toString();
            // sprintf(time_buf, "%d-%d-%d => [%s]", 
            //         nowtm->tm_hour, 
            //         nowtm->tm_min, 
            //         nowtm->tm_sec,
            //         (loglevel_ == INFO ? "info" : "error"));
            msg.insert(0, time_buf);
            msg.append("\n");
            
            fputs(msg.c_str(), pf);
            fclose(pf);
        }
    });
    //  设置分离线程，守护线程
    writeLogTask.detach();
}