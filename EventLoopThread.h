#pragma once

#include "noncopyable.h"
#include "Thread.h"

#include <functional>
#include <mutex>
#include <condition_variable>
#include <string>


class EventLoop;
//绑定一个线程和一个loop，one loop per thread
class EventLoopThread : noncopyable
{
public:
    using ThreadInitCallback = std::function<void(EventLoop *)>;

    EventLoopThread(const ThreadInitCallback &cb = ThreadInitCallback(),
                    const std::string &name = std::string());
    ~EventLoopThread();

    EventLoop *startLoop();

private:
    void threadFunc();//线程函数，在该函数里创建一个eventloop

    EventLoop *loop_;
    bool exiting_; //标记是否退出
    Thread thread_; //线程对象
    std::mutex mutex_; //锁
    std::condition_variable cond_; //条件变量
    ThreadInitCallback callback_; //初始化回调
};