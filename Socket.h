#pragma once

#include "noncopyable.h"

class InetAddress;

// 封装socket fd
class Socket : noncopyable
{
public:
    explicit Socket(int sockfd)
        : sockfd_(sockfd)
    {
    }

    ~Socket();

    int fd() const { return sockfd_; }
    void bindAddress(const InetAddress &localaddr);
    void listen();
    int accept(InetAddress *peeraddr);
    // 关闭写端
    void shutdownWrite();
    // 数据不缓冲直接发送
    void setTcpNoDelay(bool on);
    // 地址复用
    void setReuseAddr(bool on);
    // 端口复用
    void setReusePort(bool on);
    // tcp保活机制
    void setKeepAlive(bool on);

private:
    const int sockfd_;
};