#include <mymuduo/TcpServer.h>
#include <mymuduo/Logger.h>
#include <string>
#include <functional>
using namespace std;

class EchoServer
{
public:
    EchoServer(EventLoop *loop, const InetAddress &addr, const string &name)
        : server_(loop, addr, name), loop_(loop)
    {
        // 注册回调函数
        server_.setConnectionCallback(bind(&EchoServer::onConnection, this, placeholders::_1));

        server_.setMessageCallback(bind(&EchoServer::onMessage, this, placeholders::_1, placeholders::_2, placeholders::_3));

        // 设置合适的loop线程数量
        server_.setThreadNum(3);
    }

    // 开启
    void start()
    {
        server_.start();
    }

private:
    // 连接建立或断开的回调
    void onConnection(const TcpConnectionPtr &conn)
    {
        if (conn->connected())
        {
            LOG_INFO("Connection UP: %s", conn->peerAddress().toIpPort().c_str());
        }
        else
        {
            LOG_INFO("Connection DOWN: %s", conn->peerAddress().toIpPort().c_str());
        }
    }

    // 可读可写事件回调
    void onMessage(const TcpConnectionPtr &conn, Buffer *buf, Timestamp time)
    {
        string msg = buf->retrieveAllAsString();
        if(msg.size() == 5)
        {
            msg.clear();
            msg.assign("Hi~\n");
        }
        else
        {
            msg.clear();
            msg.assign("copy that.\n");
        }
        conn->send(msg);
        // conn->shutdown(); // 写端 EPOLLHUP -> closeCallback
    }

    EventLoop *loop_;
    TcpServer server_;
};

int main()
{

    EventLoop loop;
    InetAddress addr(8000);
    EchoServer server(&loop, addr, "MyServer"); //Acceptor non-blocking listenfd creat bind

    server.start(); //listen loopthread listen -> acceptChannel -> mainloop
    loop.loop(); //启动mainloop的底层Poller

    return 0;
}

/*
使用muduo网络库主要流程：
构造TcpServer对象，设置回调函数，设置底层线程数量setThreadNum()，调用start()方法，开启主线程loop。
*/