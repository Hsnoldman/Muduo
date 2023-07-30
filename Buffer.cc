#include "Buffer.h"

#include <errno.h>
#include <sys/uio.h>
#include <unistd.h>

/**
 * 从fd上读取数据  Poller工作在LT模式
 * Buffer缓冲区是有大小的！ 但是从fd上读数据的时候，却不知道tcp数据最终的大小
 */
ssize_t Buffer::readFd(int fd, int *saveErrno)
{
    char extrabuf[65536] = {0}; // 栈上的内存空间  64K
    /*
    struct iovec {
        void  *iov_base;    //Starting address
        size_t iov_len;    // Number of bytes to transfer };
    用于快速读取数据的一个字节块，指出首地址和字节块的长度即可。

    ssize_t readv(int fd, const struct iovec *iov, int iovcnt);  // 从fd读取数据
    ssize_t writev(int fd, const struct iovec *iov, int iovcnt); // 向fd写入数据
    fd是文件描述符，iov是队列的队首指针，iovcnt是队列的长度。返回读写的字节数或者错误返回-1.
    */
    struct iovec vec[2];

    const size_t writable = writableBytes(); // Buffer底层缓冲区剩余的可写空间大小
    vec[0].iov_base = begin() + writerIndex_;
    vec[0].iov_len = writable;

    vec[1].iov_base = extrabuf;
    vec[1].iov_len = sizeof extrabuf;

    const int iovcnt = (writable < sizeof extrabuf) ? 2 : 1;
    const ssize_t n = ::readv(fd, vec, iovcnt);
    if (n < 0)
    {
        *saveErrno = errno;
    }
    else if (n <= writable) // Buffer的可写缓冲区已经够存储读出来的数据了
    {
        writerIndex_ += n;
    }
    else // extrabuf里面也写入了数据
    {
        writerIndex_ = buffer_.size();
        append(extrabuf, n - writable); // writerIndex_开始写 n - writable大小的数据
    }

    return n; //返回读取的字节数
}

ssize_t Buffer::writeFd(int fd, int *saveErrno)
{
    ssize_t n = ::write(fd, peek(), readableBytes());
    if (n < 0)
    {
        *saveErrno = errno;
    }
    return n;
}