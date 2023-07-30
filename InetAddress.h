#pragma once
//地址类
#include <arpa/inet.h>
#include <netinet/in.h>
#include <string>

// 封装socket地址类型
class InetAddress
{
public:
    explicit InetAddress(uint16_t port = 0, std::string ip = "127.0.0.1"); //默认地址127.0.0.1
    explicit InetAddress(const sockaddr_in &addr)
        : addr_(addr)
    {}

    std::string toIp() const;
    std::string toIpPort() const;
    uint16_t toPort() const;

    const sockaddr_in* getSockAddr() const {return &addr_;}
    void setSockAddr(const sockaddr_in &addr) { addr_ = addr; }
private:
    sockaddr_in addr_;
};

/*
struct sockaddr和struct sockaddr_in这两个结构体用来处理网络通信的地址
sockaddr_in在头文件#include<netinet/in.h>或#include <arpa/inet.h>中定义
该结构体解决了sockaddr的缺陷，把port和addr 分开储存在两个变量中
*/