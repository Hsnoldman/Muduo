#pragma once
/**
 * 用户使用muduo编写服务器程序
 */
#include "EventLoop.h"
#include "Acceptor.h"
#include "InetAddress.h"
#include "noncopyable.h"
#include "EventLoopThreadPool.h"
#include "Callbacks.h"
#include "TcpConnection.h"
#include "Buffer.h"

#include <functional>
#include <string>
#include <memory>
#include <atomic>
#include <unordered_map>

// 对外的服务器编程使用的类
class TcpServer : noncopyable
{
public:
    using ThreadInitCallback = std::function<void(EventLoop *)>;

    // 是否端口复用
    enum Option
    {
        kNoReusePort,
        kReusePort,
    };

    TcpServer(EventLoop *loop,
              const InetAddress &listenAddr,
              const std::string &nameArg,
              Option option = kNoReusePort); // 默认不使用端口复用
    ~TcpServer();

    // 设置EventloopThread中线程初始化时的回调
    void setThreadInitcallback(const ThreadInitCallback &cb) { threadInitCallback_ = cb; }
    // 设置新连接到来时的回调
    void setConnectionCallback(const ConnectionCallback &cb) { connectionCallback_ = cb; }
    // 设置有消息到来时的回调
    void setMessageCallback(const MessageCallback &cb) { messageCallback_ = cb; }
    // 设置消息发送完成以后的回调
    void setWriteCompleteCallback(const WriteCompleteCallback &cb) { writeCompleteCallback_ = cb; }

    // 设置底层subloop的个数
    void setThreadNum(int numThreads);

    // 开启服务器监听
    void start();

private:
    void newConnection(int sockfd, const InetAddress &peerAddr);
    void removeConnection(const TcpConnectionPtr &conn);
    void removeConnectionInLoop(const TcpConnectionPtr &conn);

    EventLoop *loop_; // baseLoop 用户定义的loop

    const std::string ipPort_; // ip地址，端口号
    const std::string name_;

    std::unique_ptr<Acceptor> acceptor_; // 运行在mainLoop，任务就是监听新连接事件

    std::shared_ptr<EventLoopThreadPool> threadPool_; // one loop per thread

    ConnectionCallback connectionCallback_;       // 有新连接时的回调
    MessageCallback messageCallback_;             // 有读写消息时的回调
    WriteCompleteCallback writeCompleteCallback_; // 消息发送完成以后的回调

    // EventloopThread.cc中线程初始化时调用
    ThreadInitCallback threadInitCallback_; // loop线程初始化的回调

    std::atomic_int started_;

    int nextConnId_;
    using ConnectionMap = std::unordered_map<std::string, TcpConnectionPtr>;
    ConnectionMap connections_; // 保存所有的连接，hash表
};