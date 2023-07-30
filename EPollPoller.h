#pragma once

#include "Poller.h"
#include "Timestamp.h"

#include <vector>
#include <sys/epoll.h>

class Channel;

/**
 * epoll的使用
 * 1.epoll_create
 *      int epoll_create(int size);
        创建一个epoll的句柄，size用来告诉内核这个监听的数目一共有多大。当创建好epoll句柄后，它就是会占用一个fd值，在使用完epoll后，必须调用close()关闭，否则可能导致fd被耗尽。
 * 2.epoll_ctl   
        int epoll_ctl(int epfd, int op, int fd, struct epoll_event *event);
        epoll的事件注册函数，epoll_ctl向 epoll对象中添加、修改或者删除感兴趣的事件，返回0表示成功，否则返回–1，此时需要根据errno错误码判断错误类型。
 * 3.epoll_wait
        int epoll_wait(int epfd, struct epoll_event * events, int maxevents, int timeout);
        该函数返回需要处理的事件数目，如返回0表示已超时。如果返回–1，则表示出现错误，需要检查 errno错误码判断错误类型。



events可以是以下几个宏的集合：
EPOLLIN ：表示对应的文件描述符可以读（包括对端SOCKET正常关闭）；
EPOLLOUT：表示对应的文件描述符可以写；
EPOLLPRI：表示对应的文件描述符有紧急的数据可读（这里应该表示有带外数据到来）；
EPOLLERR：表示对应的文件描述符发生错误；
EPOLLHUP：表示对应的文件描述符被挂断
EPOLLET ： 默认使用水平触发，将EPOLL设为边缘触发(Edge Triggered)模式，这是相对于水平触发(Level Triggered)来说的。
EPOLLRDHUP：表示对端已经关闭连接，或者关闭了写操作端的写入
EPOLLONESHOT：只监听一次事件，当监听完这次事件之后，如果还需要继续监听这个socket的话，需要再次把这个socket加入到EPOLL队列里
*/

/*
描述：override保留字表示当前函数重写了基类的虚函数。
目的：
1.在函数比较多的情况下可以提示读者某个函数重写了基类虚函数（表示这个虚函数是从基类继承，不是派生类自己定义的）；
2.强制编译器检查某个函数是否重写基类虚函数，如果没有则报错。
*/
class EPollPoller : public Poller
{
public:
    EPollPoller(EventLoop *loop);
    ~EPollPoller() override;

    // 重写基类Poller的抽象方法
    Timestamp poll(int timeoutMs, ChannelList *activeChannels) override;
    void updateChannel(Channel *channel) override;
    void removeChannel(Channel *channel) override;

private:
    static const int kInitEventListSize = 16; // 定义epollevent数组初始长度

    // 填写活跃的连接
    void fillActiveChannels(int numEvents, ChannelList *activeChannels) const;
    // 更新channel通道
    void update(int operation, Channel *channel);

    using EventList = std::vector<epoll_event>;

    int epollfd_;      // epoll对象的fd，epoll的句柄
    EventList events_; // epoll事件数组
};