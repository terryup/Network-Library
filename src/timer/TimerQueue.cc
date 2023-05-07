#include "TImerQueue.h"
#include "EventLoop.h"
#include "Timer.h"
#include "logger.h"

#include <sys/timerfd.h>
#include <string>
#include <unistd.h>
#include <string.h>

int createTimerfd() {
    /*
        CLOCK_MONOTONIC：绝对时间
        TFD_NONBLOCK：非阻塞
        TFD_CLOEXEC：设置为执行 exec 时关闭描述符
    */
   //   创建一个新的定时器对象
   int timerFd = ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
   if (timerFd < 0){
    LOG_ERR("Failed in timerfd_create! \n");
   }
   return timerFd;
}


TimerQueue::TimerQueue(EventLoop* loop)
    : loop_(loop)
    , timerFd_(createTimerfd())
    , timerfdChannel_(loop_, timerFd_)
    , timers_() {
    
    timerfdChannel_.setReadCallback(std::bind(
        &TimerQueue::handleRead, this
    ));
    timerfdChannel_.enableReading();
}

TimerQueue::~TimerQueue(){
    timerfdChannel_.disableAll();
    timerfdChannel_.remove();
    ::close(timerFd_);

    for(const Entry& timer : timers_){
        delete timer.second;
    }
}

void TimerQueue::addTimer(TimerCallback cb, Timestamp when, double interval){
    Timer* timer = new Timer(std::move(cb), when, interval);
    loop_->runInLoop(std::bind(
        &TimerQueue::addTimerInLoop, this, timer
    ));
}

void TimerQueue::addTimerInLoop(Timer* timer){
    //  是否取代了最早的定时触发时间
    bool eraliestChanged = insert(timer);

    //  重新设置timerfd_触发时间
    if (eraliestChanged){
        resetTimerfd(timerFd_, timer->getExpiration_());
    }
}

void TimerQueue::resetTimerfd(int timerFd, Timestamp expiration){
    //  设置定时器的结构体类型  对它们进行清空
    struct itimerspec newValue;
    struct itimerspec oldValue;
    memset(&newValue, '\0', sizeof(newValue));
    memset(&oldValue, '\0', sizeof(oldValue));

    //  超时时间 - 现在时间
    int64_t microSecondDif = expiration.microSecondsSinceEpoch() - Timestamp::now().microSecondsSinceEpoch();
    if (microSecondDif < 100){
        microSecondDif = 100;
    }

    //  在 timerfd 上设置一个新的定时器，使得该定时器在到期时会唤醒事件循环
    struct timespec ts;
    ts.tv_sec = static_cast<time_t>(microSecondDif / Timestamp::kMicroSecondsPerSecond);
    ts.tv_nsec = static_cast<long>((microSecondDif % Timestamp::kMicroSecondsPerSecond) * 1000);
    newValue.it_value = ts;
    if (::timerfd_settime(timerFd_, 0, &newValue, &oldValue)){
        LOG_ERR("timerfd_settime faield()! \n");
    }
}

//  用来读取计时器文件描述符（timerfd）的数据
void ReadTimerFd(int timerfd) 
{
    uint64_t read_byte;
    ssize_t readn = ::read(timerfd, &read_byte, sizeof(read_byte));
    
    if (readn != sizeof(read_byte)) {
        LOG_ERR("TimerQueue::ReadTimerFd read_size < 0");
    }
}

//  获取已经过期的定时器
std::vector<TimerQueue::Entry> TimerQueue::getExpired(Timestamp now){
    //  用于存储过期定时器的vector
    std::vector<Entry> expired;
    //  创建一个哨兵Timer对象，Timer的过期时间为now，定时器ID为最大值
    Entry sentry(now, reinterpret_cast<Timer*>(UINTPTR_MAX));
    //  找到第一个过期时间 > now 的Timer迭代器
    TimerList::iterator end = timers_.lower_bound(sentry);
    //  将所有过期的Timer添加到expired vector中
    std::copy(timers_.begin(), end, back_inserter(expired));
    //  从timers_中删除所有过期的Timer
    timers_.erase(timers_.begin(), end);

    //  返回过期的Timer vector
    return expired;
}

void TimerQueue::handleRead(){
    Timestamp now = Timestamp::now();
    ReadTimerFd(timerFd_);

    std::vector<Entry> expired = getExpired(now);

    //  遍历到期的定时器，调用回调函数
    callingExpiredTimers_ = true;
    for (const Entry& it : expired){
        it.second->run();
    }
    callingExpiredTimers_ = false;

    //  重新设置这些定时器
    reset(expired, now);
}

void TimerQueue::reset(const std::vector<Entry>& expired, Timestamp now){
    Timestamp nextExpire;
    for (const Entry& it : expired){
        //  重复任务则继续执行
        if (it.second->getRepeat_()){
            auto timer = it.second;
            timer->restart(Timestamp::now());
            insert(timer);
        }   
        else{
            delete it.second;
        }

        //  如果重新插入了定时器，需要继续重置timerfd
        if (!timers_.empty()){
            resetTimerfd(timerFd_, (timers_.begin()->second)->getExpiration_());
        }
    }
}

bool TimerQueue::insert(Timer* timer){
    bool earliestChanged = false;
    Timestamp when = timer->getExpiration_();
    TimerList::iterator it = timers_.begin();
    if (it == timers_.end() || when < it->first){
        //  说明最早的定时器已经被替换了
        earliestChanged = true;
    }

    //  定时器管理红黑树插入此新定时器
    timers_.insert(Entry(when, timer));

    return earliestChanged;
}