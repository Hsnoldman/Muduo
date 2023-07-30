#pragma once

#include "noncopyable.h"
#include "Timestamp.h"

#include <functional>
#include <memory>

class EventLoop;
/*
EventLoop作为事件分发器，里面包含了epoll和相应的事件，事件被封装在channel里， 相当于EventLoop{epoll,channel}
    把事件注册在poller中，发生的事件由poller通知channel，channel得到相应事件的fd后，调用回调函数，处理事件
*/
/**
 * 理清楚  EventLoop、Channel、Poller之间的关系   《= Reactor模型上对应 Demultiplex
 * Channel 理解为通道，封装了sockfd和其感兴趣的event，如EPOLLIN、EPOLLOUT事件
 * 还绑定了poller返回的具体事件
 */

/*
每个Channel对象自始至终只属于一个EventLoop，因此每个Channel对象都只属于某一个IO线程。
每个Channel对象自始至终只负责一个文件描述符（fd）的IO事件分发，但它并不拥有这个fd，也不会在析构的时候关闭这个fd。
一个channel对应一个fd
*/
class Channel : noncopyable
{
public:
    // 使用函数指针来吧一个函数作为参数传递，可以实现回调函数的机制。
    using EventCallback = std::function<void()>;
    using ReadEventCallback = std::function<void(Timestamp)>;

    Channel(EventLoop *loop, int fd);
    ~Channel();

    // fd得到poller通知以后，处理事件的(通过调用响应的回调方法)
    void handleEvent(Timestamp receiveTime);

    // 设置回调函数对象
    void setReadCallback(ReadEventCallback cb) { readCallback_ = std::move(cb); } // move将左值转换为右值
    void setWriteCallback(EventCallback cb) { writeCallback_ = std::move(cb); }
    void setCloseCallback(EventCallback cb) { closeCallback_ = std::move(cb); }
    void setErrorCallback(EventCallback cb) { errorCallback_ = std::move(cb); }

    // 防止当channel被手动remove掉，channel还在执行回调操作
    void tie(const std::shared_ptr<void> &);

    int fd() const { return fd_; }         // 当前channel表示的socket事件
    int events() const { return events_; } // 这个fd感兴趣的事件
    // int set_revents(int revt) { revents_ = revt;} // 真正发生的事件
    void set_revents(int revt) { revents_ = revt; } // 真正发生的事件

    // 设置fd相应的事件状态
    // 将读事件kReadEvent更新到events_上
    void enableReading()
    {
        events_ |= kReadEvent;
        update();
    }

    // 从events_去掉读事件
    void disableReading()
    {
        events_ &= ~kReadEvent;
        update();
    }
    // 将写事件kReadEvent更新到events_上
    void enableWriting()
    {
        events_ |= kWriteEvent;
        update();
    }
    // 从events_去掉写事件
    void disableWriting()
    {
        events_ &= ~kWriteEvent;
        update();
    }
    // 去掉所有事件
    void disableAll()
    {
        events_ = kNoneEvent;
        update();
    }

    // 返回fd当前的事件状态
    bool isNoneEvent() const { return events_ == kNoneEvent; }
    bool isWriting() const { return events_ & kWriteEvent; }
    bool isReading() const { return events_ & kReadEvent; }

    int index() { return index_; }
    void set_index(int idx) { index_ = idx; }

    // one loop per thread
    /*
    一个线程有一个eventloop，一个eventloop里有一个poller，一个poller上可以监听很多的channel
    一个eventloop包含很多个channel
    */
    EventLoop *ownerLoop() { return loop_; } // 获取当前这个channe属于哪个Eventloop
    void remove();                           // 删除cannel
private:
    void update(); // 更新事件状态
    // 根据poller通知的channel发生的具体事件， 由channel负责调用具体的回调操作
    void handleEventWithGuard(Timestamp receiveTime);

    // 表示当前fd的状态
    static const int kNoneEvent;  // 没有对任何事件感兴趣
    static const int kReadEvent;  // 对读时间感兴趣
    static const int kWriteEvent; // 对写事件感兴趣

    EventLoop *loop_; // 事件循环
    const int fd_;    // fd, Poller监听的对象
    int events_;      // 注册fd感兴趣的事件
    int revents_;     // poller返回的具体发生的事件，目前活动的事件
    int index_;

    std::weak_ptr<void> tie_; // 防止channel被手动调用，进行跨线程监听
    bool tied_;               // 判断当前channel是否存活

    // 因为channel通道里面能够获知fd最终发生的具体的事件revents，所以它负责调用具体事件的回调操作
    ReadEventCallback readCallback_;
    EventCallback writeCallback_;
    EventCallback closeCallback_;
    EventCallback errorCallback_;
};
