#pragma once

#include "noncopyable.h"
#include "Timestamp.h"

#include <vector>
#include <unordered_map>

class Channel;
class EventLoop;

// muduo库中多路事件分发器的核心IO复用模块
class Poller : noncopyable
{
public:
    using ChannelList = std::vector<Channel *>;

    Poller(EventLoop *loop);
    virtual ~Poller() = default; // 虚析构，使用默认实现

    // 使用虚函数，给所有IO复用保留统一的接口，由派生类实现
    virtual Timestamp poll(int timeoutMs, ChannelList *activeChannels) = 0;
    virtual void updateChannel(Channel *channel) = 0;
    virtual void removeChannel(Channel *channel) = 0;

    // 判断参数channel是否在当前Poller当中
    bool hasChannel(Channel *channel) const;

    
    /*
    此处要返回一个具体的实现，但是当前类为Poller抽象类，如果要返回具体实现，需要包含具体的EpollPoller类头文件
    但是EpollPoller是继承自当前Poller类
    如果在当前类中包含EpollPoller类，相当于子类(EpollPoller)继承父类(Poller)，父类又依赖子类，这种设计不好
    因此，将当前方法的实现，写在一个外部公共.h文件(DefaultPoller)中
    */
   //static方法，类内声明，类外定义
    static Poller *newDefaultPoller(EventLoop *loop);// EventLoop可以通过该接口获取默认的IO复用的具体实现

protected:
    // map的key：sockfd  value：sockfd所属的channel通道类型
    using ChannelMap = std::unordered_map<int, Channel *>; // 存储所有的channel 无序关联容器，hash表。
    ChannelMap channels_; //channel数组

private:
    EventLoop *ownerLoop_; // 定义Poller所属的事件循环EventLoop
};