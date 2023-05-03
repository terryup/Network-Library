#ifndef TIMESTAMP_H
#define TIMESTAMP_H

#include <iostream>
#include <sys/time.h>
#include <string>

//  时间类， 可以获取事件
class Timestamp{
public:
    Timestamp();
    Timestamp(int64_t microSecondsSinceEpoch);

    //  获取当前时间戳
    static Timestamp now();

    //用std::string 的形式返回，格式为[millisec].[microsec]
    std::string toString() const;

private:
    //  表示事件的事件戳
    int64_t microSecondsSinceEpoch_;
};

#endif