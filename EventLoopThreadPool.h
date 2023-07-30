#pragma once

#include "noncopyable.h"
#include <functional>
#include <string>
#include <vector>
#include <memory>

class EventLoop;
class EventLoopThread;

class EventLoopThreadPool : noncopyable
{
public:
    using ThreadInitCallback = std::function<void(EventLoop*)>; 

    EventLoopThreadPool(EventLoop *baseLoop, const std::string &nameArg);
    ~EventLoopThreadPool();

    //设置底层线程数量
    void setThreadNum(int numThreads) { numThreads_ = numThreads; }

    //开启事件循环线程
    void start(const ThreadInitCallback &cb = ThreadInitCallback());

    // 如果工作在多线程中，baseLoop_默认以轮询的方式分配channel给subloop
    EventLoop* getNextLoop();

    //返回loop池里的所有的subloop对象
    std::vector<EventLoop*> getAllLoops();

    //返回当前subloop是否启动
    bool started() const { return started_; }
    //返回loop_name
    const std::string name() const { return name_; }

private:
    EventLoop *baseLoop_; //用户创建的loop
    std::string name_; //loop名称
    bool started_; //是否启动
    int numThreads_; //线程数量
    int next_; //轮询时的下标
    std::vector<std::unique_ptr<EventLoopThread>> threads_; //loop里所有的线程
    std::vector<EventLoop*> loops_; //baseloop创建的所有subloop
};