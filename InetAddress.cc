#include "InetAddress.h"

#include <strings.h>
#include <string.h>

InetAddress::InetAddress(uint16_t port, std::string ip)
{
    bzero(&addr_, sizeof addr_); //bzero()置0
    addr_.sin_family = AF_INET;//IPv4网络协议的套接字类型,选择AF_INET 的目的就是使用IPv4 进行通信
    addr_.sin_port = htons(port); //htons() 是网络字节序与主机字节序之间转换的函数 网络字节序采用大端排序方式
    addr_.sin_addr.s_addr = inet_addr(ip.c_str()); //ip地址转换，inet_addr()的功能是将一个点分十进制的IPv4转换成一个长整数型数（u_long类型）
}

//获取IP
std::string InetAddress::toIp() const
{
    // addr_
    char buf[64] = {0};
    ::inet_ntop(AF_INET, &addr_.sin_addr, buf, sizeof buf); //inet_ntop()将网络字节序地址转换为本地字节序网络地址
    return buf;
}

//返回ip:port形式字符串 
std::string InetAddress::toIpPort() const
{
    // ip:port
    char buf[64] = {0};
    ::inet_ntop(AF_INET, &addr_.sin_addr, buf, sizeof buf);
    size_t end = strlen(buf);
    uint16_t port = ntohs(addr_.sin_port); //ntohs作用是将一个16位数由网络字节顺序转换为主机字节顺序
    sprintf(buf+end, ":%u", port);
    return buf;
}

//获取port
uint16_t InetAddress::toPort() const
{
    return ntohs(addr_.sin_port);
}

#include <iostream>
int main()
{
    // InetAddress addr(8080,"192.168.35.129");
    InetAddress addr(8080);
    std::cout << addr.toIpPort() << std::endl;

    return 0;
}