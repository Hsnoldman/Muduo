#pragma once

#include <vector>
#include <string>
#include <algorithm>

// 网络库底层的缓冲器类型定义
class Buffer
{
public:
    static const size_t kCheapPrepend = 8;   // 预留字节，存储缓冲区包的大小，解决粘包问题
    static const size_t kInitialSize = 1024; // 缓冲区大小

    // buffer初始化
    explicit Buffer(size_t initialSize = kInitialSize)
        : buffer_(kCheapPrepend + initialSize), readerIndex_(kCheapPrepend), writerIndex_(kCheapPrepend)
    {
    }

    // 可读缓冲区长度
    size_t readableBytes() const
    {
        return writerIndex_ - readerIndex_;
    }

    // 可写缓冲区长度
    size_t writableBytes() const
    {
        return buffer_.size() - writerIndex_;
    }

    // 返回预留字节位大小
    size_t prependableBytes() const
    {
        return readerIndex_;
    }

    // 返回缓冲区中可读数据的起始地址
    const char *peek() const
    {
        return begin() + readerIndex_;
    }

    // onMessage string <- Buffer
    void retrieve(size_t len)
    {
        if (len < readableBytes())
        {
            readerIndex_ += len; // 应用只读取了刻度缓冲区数据的一部分，就是len，还剩下readerIndex_ += len -> writerIndex_
        }
        else // len == readableBytes()
        {
            retrieveAll();
        }
    }

    // 缓冲区复位
    void retrieveAll()
    {
        readerIndex_ = writerIndex_ = kCheapPrepend;
    }

    // 把onMessage函数上报的Buffer数据，转成string类型的数据返回
    std::string retrieveAllAsString()
    {
        return retrieveAsString(readableBytes()); // 应用可读取数据的长度
    }

    std::string retrieveAsString(size_t len)
    {
        std::string result(peek(), len); // 可读数据起始位置+可读数据长度
        retrieve(len);                   // 上面一句把缓冲区中可读的数据，已经读取出来，这里肯定要对缓冲区进行复位操作
        return result;
    }

    // buffer_.size() - writerIndex_    len
    void ensureWriteableBytes(size_t len)
    {
        if (writableBytes() < len)
        {
            makeSpace(len); // 扩容函数
        }
    }

    // 把[data, data+len]内存上的数据，添加到writable缓冲区当中
    void append(const char *data, size_t len)
    {
        ensureWriteableBytes(len); // 判断缓冲区长度是否够用，不够的话进行扩容
        std::copy(data, data + len, beginWrite());
        writerIndex_ += len;
    }

    char *beginWrite()
    {
        return begin() + writerIndex_;
    }

    const char *beginWrite() const
    {
        return begin() + writerIndex_;
    }

    // 从fd上读取数据
    ssize_t readFd(int fd, int *saveErrno);
    // 通过fd发送数据
    ssize_t writeFd(int fd, int *saveErrno);

private:
    char *begin()
    {
        // it.operator*()
        return &*buffer_.begin(); // vector底层数组首元素的地址，也就是数组的起始地址
    }
    // 常方法
    const char *begin() const
    {
        return &*buffer_.begin();
    }

    // 缓冲区扩容
    void makeSpace(size_t len)
    {
        if (writableBytes() + prependableBytes() < len + kCheapPrepend)
        {
            buffer_.resize(writerIndex_ + len);
        }
        /*
        1.| kCheapPrepend |      读缓冲区          |             写缓冲区             |
        2.| kCheapPrepend |   已读      |   未读   |             写缓冲区             |
        3.| kCheapPrepend |   未读      |   已读   |             写缓冲区             |
        4.| kCheapPrepend |   读区      |                 写缓冲区                    |
        将读缓冲区还未读的数据前移，将读缓冲区已经空余出来的空间和写缓冲区的空间整合起来，形成新的写缓冲区
        */
        else
        {
            size_t readalbe = readableBytes();
            std::copy(begin() + readerIndex_,
                      begin() + writerIndex_,
                      begin() + kCheapPrepend);
            readerIndex_ = kCheapPrepend;
            writerIndex_ = readerIndex_ + readalbe;
        }
    }

    std::vector<char> buffer_; // 缓冲区数组
    size_t readerIndex_;       // 读下标
    size_t writerIndex_;       // 写下标
};