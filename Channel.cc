#include "Channel.h"
#include "EventLoop.h"
#include "Logger.h"

#include <sys/epoll.h>

//static修饰成员变量，类内定义，类外初始化
const int Channel::kNoneEvent = 0;
const int Channel::kReadEvent = EPOLLIN | EPOLLPRI; // EPOLLIN：连接到达，有数据来临  EPOLLPRI：外带数据
const int Channel::kWriteEvent = EPOLLOUT;          // EPOLLOUT：写事件 对端读取了一些数据，当前可写了

// EventLoop: ChannelList Poller
// channel构造函数，初始化成员变量
Channel::Channel(EventLoop *loop, int fd)
    : loop_(loop), fd_(fd), events_(0), revents_(0), index_(-1), tied_(false)
{
}

Channel::~Channel()
{
}

//一个TcpConnection新连接创建的时候，调用此函数 TcpConnection => Channel
void Channel::tie(const std::shared_ptr<void> &obj)
{
    tie_ = obj;
    tied_ = true;
}

/**
 * 当改变channel所表示fd的events事件后，update负责在poller里面更改fd相应的事件epoll_ctl
 * EventLoop 包含： ChannelList   Poller
 * channel不能直接更改poller里的事件fd，通过返回给EventLoop，由EventLoop更改poller中的事件fd
 */
void Channel::update()
{
    // 通过channel所属的EventLoop，调用poller的相应方法，注册fd的events事件
    loop_->updateChannel(this);
}

// 在channel所属的EventLoop中， 把当前的channel删除掉
//Channel::update()会调用EventLoop::updateChannel()，后者会转而调用Poller::updateChannel()
void Channel::remove()
{
    loop_->removeChannel(this);
}

// fd得到poller通知以后，处理事件的
void Channel::handleEvent(Timestamp receiveTime)
{
    if (tied_)
    {
        std::shared_ptr<void> guard = tie_.lock(); //使用lock() 将弱智能指针tie_提升为强智能指针
        if (guard)//提升成功
        {
            handleEventWithGuard(receiveTime);//执行回调，处理事件
        }
    }
    else
    {
        handleEventWithGuard(receiveTime);
    }
}

// 根据poller通知的channel发生的具体事件， 由channel负责调用具体的回调操作
/*
Acceptor封装的accpetchannel只会调用readcallback
因为它只需要在新连接到来时通过往subloop的wakeupfd上写一个数据以此来唤醒subloop，新连接到来是一个读事件
具体的readcallback函数在Acceptor类中定义

Tcpconnection类管理的connfd封装的channel是具体处理各种读写事件的，下面的回调都会出发
具体的closeCallback_，errorCallback_，readCallback_，writeCallback_函数在Tcpconnection类中定义
*/
void Channel::handleEventWithGuard(Timestamp receiveTime)
{
    //日志
    LOG_INFO("channel handleEvent revents:%d\n", revents_);

    if ((revents_ & EPOLLHUP) && !(revents_ & EPOLLIN)) //EPOLLHUP表示读写都关闭
    {
        if (closeCallback_)
        {
            closeCallback_();//执行关闭回调函数
        }
    }

    if (revents_ & EPOLLERR) //EPOLLERR表示错误事件
    {
        if (errorCallback_)
        {
            errorCallback_();//执行错误回调
        }
    }

    if (revents_ & (EPOLLIN | EPOLLPRI)) //读事件
    {
        if (readCallback_)
        {
            readCallback_(receiveTime);
        }
    }

    if (revents_ & EPOLLOUT)//写事件
    {
        if (writeCallback_)
        {
            writeCallback_();
        }
    }
}