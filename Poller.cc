#include "Poller.h"
#include "Channel.h"

// 构造函数，记录poller所属的eventloop
Poller::Poller(EventLoop *loop)
    : ownerLoop_(loop)
{
}

bool Poller::hasChannel(Channel *channel) const
{
    // map的key：sockfd  value：sockfd所属的channel通道类型
    auto it = channels_.find(channel->fd());
    return it != channels_.end() && it->second == channel;
}