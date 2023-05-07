#ifndef TIMER_H
#define TIMER_H

#include "noncopyable.h"
#include "Timestamp.h"

#include <functional>

/*
    Timer类描述了一个定时器
    定时器回调函数，下一次超时时刻，重复定时器的时间间隔等
*/
class Timer : noncopyable{
public:
    using TimerCallback = std::function<void()>;

    Timer(TimerCallback cb, Timestamp when, double interval)
        : timerCallback_(cb)
        , expiration_(when)
        , interval_(interval)
        , repeat_(interval > 0.0){}

    void run() const {
        timerCallback_();
    }

    Timestamp getExpiration_() const { return expiration_; }

    bool getRepeat_() const { return repeat_; }

    double getInterval_() const { return interval_; }

    //  重启定时器（如果是非重复事件则到期时间置为0）
    void restart(Timestamp now);

private:

    const TimerCallback timerCallback_; //  定时器的回调函数
    Timestamp expiration_;  //  下一次的超时时刻
    const double interval_;   //  超时时间间隔，如果是一次性定时器，该值为0
    const bool repeat_; //  是否重复(false表示为一次性定时器)

};


#endif