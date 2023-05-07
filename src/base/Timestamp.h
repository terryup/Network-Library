#ifndef TIMESTAMP_H
#define TIMESTAMP_H

#include <iostream>
#include <sys/time.h>
#include <string>
#include <assert.h>

//  时间类， 可以获取事件
class Timestamp{
public:
    Timestamp();
    Timestamp(int64_t microSecondsSinceEpoch);

    //  获取当前时间戳
    static Timestamp now();

    void swap(Timestamp& that){
        std::swap(microSecondsSinceEpoch_, that.microSecondsSinceEpoch_);
    }

    //返回当前时间戳的微妙
    int64_t microSecondsSinceEpoch() const { return microSecondsSinceEpoch_; }

    //用std::string 的形式返回，格式为[millisec].[microsec]
    std::string toString() const;

    // 1秒=1000*1000微妙
    static const int kMicroSecondsPerSecond = 1000 * 1000;

private:
    //  表示事件的事件戳
    int64_t microSecondsSinceEpoch_;
};

inline bool operator<(Timestamp lhs, Timestamp rhs)
{
    return lhs.microSecondsSinceEpoch() < rhs.microSecondsSinceEpoch();
}


inline bool operator==(Timestamp lhs, Timestamp rhs)
{
    return lhs.microSecondsSinceEpoch() == rhs.microSecondsSinceEpoch();
}

// 如果是重复定时任务就会对此时间戳进行增加。
inline Timestamp addTime(Timestamp timestamp, double seconds)
{
    // 将延时的秒数转换为微妙
    int64_t delta = static_cast<int64_t>(seconds * Timestamp::kMicroSecondsPerSecond);
    // 返回新增时后的时间戳
    return Timestamp(timestamp.microSecondsSinceEpoch() + delta);
}

#endif