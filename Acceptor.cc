#include "Acceptor.h"
#include "Logger.h"
#include "InetAddress.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <unistd.h>

// 创建非阻塞socket
static int createNonblocking()
{
    int sockfd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (sockfd < 0)
    {
        LOG_FATAL("%s:%s:%d listen socket create err:%d \n", __FILE__, __FUNCTION__, __LINE__, errno);
    }
    return sockfd;
}

Acceptor::Acceptor(EventLoop *loop, const InetAddress &listenAddr, bool reuseport)
    : loop_(loop), acceptSocket_(createNonblocking()) // socket
      ,
      acceptChannel_(loop, acceptSocket_.fd()), listenning_(false)
{
    acceptSocket_.setReuseAddr(true);
    acceptSocket_.setReusePort(true);
    acceptSocket_.bindAddress(listenAddr); // bind
    // TcpServer::start() Acceptor.listen  有新用户的连接，要执行一个回调（connfd=》channel=》subloop）
    // baseLoop => acceptChannel_(listenfd) =>
    /*
    当baseloop监听到acceptchannel上有事件发生了，如果是读事件，channel中就会调用readcallback
    这个回调函数会事先在acceptor中定义好，即下面的handleRead()
    */
    /*
    下面绑定的这个回调的作用：
    当Acceptor监听到一个新连接，需要把这个客户端和服务器连接的socketfd(connfd)打包成一个channel
    然后唤醒一个subloop，将这个channel交给subloop，后续由subloop负责监听这个connfd上的读写事件
    */
    acceptChannel_.setReadCallback(std::bind(&Acceptor::handleRead, this));
}

Acceptor::~Acceptor()
{
    acceptChannel_.disableAll();
    acceptChannel_.remove();
}

//监听
void Acceptor::listen()
{
    listenning_ = true;
    acceptSocket_.listen();         // listen
    acceptChannel_.enableReading(); // acceptChannel_ => Poller
}

/*
listenfd有事件发生了，就是有新用户连接了
当有新用户连接时，acceptor下的channel(即：acceptChannel_)会监听到这个新连接，然后执行下面的回调
新连接是读事件，所以定义handleread
*/
void Acceptor::handleRead()
{
    InetAddress peerAddr;
    //此处这个connfd就是处理连接的fd，而acceptChannel_是负责监听新连接的fd
    int connfd = acceptSocket_.accept(&peerAddr);
    if (connfd >= 0)
    {
        //newConnectionCallback_这个回调函数由TcpServer对象（即由用户设置）设置
        if (newConnectionCallback_)
        {
            newConnectionCallback_(connfd, peerAddr); // 轮询找到subloop，唤醒，分发当前的新客户端的Channel
        }
        else
        {
            ::close(connfd);
        }
    }
    else
    {
        LOG_ERROR("%s:%s:%d accept err:%d \n", __FILE__, __FUNCTION__, __LINE__, errno);
        if (errno == EMFILE) // 超过文件描述符上限
        {
            LOG_ERROR("%s:%s:%d sockfd reached limit! \n", __FILE__, __FUNCTION__, __LINE__);
            /*
                单台服务器已经不足以支持现有的流量了
                此时可以进行调整文件描述符上限的操作
                进行分布式部署，集群
                ...
            */
        }
    }
}