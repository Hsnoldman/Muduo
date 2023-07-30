#include "EPollPoller.h"
#include "Logger.h"
#include "Channel.h"

#include <errno.h>
#include <unistd.h>
#include <strings.h>

// channel未添加到poller中
const int kNew = -1; // channel的成员index_ = -1
// channel已添加到poller中
const int kAdded = 1;
// channel从poller中删除
const int kDeleted = 2;

/*
默认情况下子进程会继承父进程打开的多余fd资源
epoll_create1这个函数 它的参数可以是 ::EPOLL_CLOEXEC
这样就可以在某些情况下解决掉一些问题 即在fock后关闭子进程中无用文件描述符的问题
*/

EPollPoller::EPollPoller(EventLoop *loop) : Poller(loop), // 调用基类的构造函数初始化从基类继承来的loop成员
                                            epollfd_(::epoll_create1(EPOLL_CLOEXEC)),
                                            events_(kInitEventListSize) // 16 vector<epoll_event>
{
    // 发生错误，日志记录
    if (epollfd_ < 0)
    {
        LOG_FATAL("epoll_create error:%d \n", errno);
    }
}

EPollPoller::~EPollPoller()
{
    ::close(epollfd_); //::作用域标识，表示调用的是全局close函数，不是类内的close函数
}

/*
EPollPoller::poll()函数通过epoll_wait()监听到那些文件发生了事件，把发生事件的channel，填入ChannelList中
即：将发生事件的channel告知Eventloop
*/
/*
    int epoll_wait(int epfd, struct epoll_event * events, int maxevents, int timeout);
        1) int epfd： epoll_create()函数返回的epoll实例的句柄。
        2) struct epoll_event * events： 接口的返回参数，epoll把发生的事件的集合从内核复制到 events数组中。events数组是一个用户分配好大小的数组，数组长度大于等于maxevents。（events不可以是空指针，内核只负责把数据复制到这个 events数组中，不会去帮助我们在用户态中分配内存）
        3) int maxevents： 表示本次可以返回的最大事件数目，通常maxevents参数与预分配的events数组的大小是相等的。
        4) int timeout： 表示在没有检测到事件发生时最多等待的时间，超时时间(>=0)，单位是毫秒ms，-1表示阻塞，0表示不阻塞。
返回值：
成功返回需要处理的事件数目。失败返回0，表示等待超时
*/
Timestamp EPollPoller::poll(int timeoutMs, ChannelList *activeChannels)
{
    // 实际上应该用LOG_DEBUG输出日志更为合理
    // LOG_INFO("func=%s => fd total count:%lu \n", __FUNCTION__, channels_.size()); // 高并发场景下会大量调用poll，每次都打印日志则会影响效率，可以注掉本行提升效率

    // 通过events_.begin()拿到vector容器首元素迭代器，之后解引用*，拿到首元素的值，再进行取址&，拿到首元素地址，也即数组起始地址
    // events_.size()返回的是unsigned_int类型，但epoll_wait要求int类型，此处使用c++11类型强转static_cast
    //const int kPollTimeMs = 10000  timeoutMs = kPollTimeMs = 10s
    int numEvents = ::epoll_wait(epollfd_, &*events_.begin(), static_cast<int>(events_.size()), timeoutMs);

    int saveErrno = errno;
    Timestamp now(Timestamp::now());

    if (numEvents > 0) // 表示存在活跃事件
    {
        LOG_INFO("%d events happened \n", numEvents);
        // 将监听到的活跃事件写入ChannelList
        fillActiveChannels(numEvents, activeChannels);
        // numEvents == events_.size()表明events里的所有事件都活跃，那此时有可能存在更多的活跃事件，events已经不够记录了，所以需要进行扩容
        if (numEvents == events_.size())
        {
            events_.resize(events_.size() * 2); // 2倍扩容
        }
    }
    else if (numEvents == 0) // 表明本次epoll_wait调用没有监听到活跃事件
    {
        LOG_DEBUG("%s timeout! \n", __FUNCTION__); //__FUNCTION__:当前函数名
    }
    else
    {
        if (saveErrno != EINTR) // EINTR：外部中断
        {
            errno = saveErrno;
            LOG_ERROR("EPollPoller::poll() err!");
        }
    }
    return now;
}

// 调用流程 channel update remove => EventLoop updateChannel removeChannel => Poller updateChannel removeChannel
/**
 *           EventLoop  =>   poller.poll
 *     ChannelList             Poller
 *                             ChannelMap  <fd, channel*>   epollfd
 */
void EPollPoller::updateChannel(Channel *channel)
{
    const int index = channel->index(); // 获取当前channel的状态(-1, 1, 2) channel中的index默认为-1
    LOG_INFO("func=%s => fd=%d events=%d index=%d \n", __FUNCTION__, channel->fd(), channel->events(), index);

    if (index == kNew || index == kDeleted) // index == kDeleted，这个channel之前在poller中，已经被删除了
    {
        if (index == kNew) // 这个channel从来没有被添加到poller中
        {
            int fd = channel->fd();
            //将当前channel添加到poller管理的channels_hash表中
            channels_[fd] = channel; // 将当前channel添加到channel数组中，channels为hash表unordered_map<int, Channel *>
        }

        channel->set_index(kAdded);     // 设置channel状态为已添加
        //EPOLL_CTL_ADD：注册新的fd到epfd中
        update(EPOLL_CTL_ADD, channel); // 调用epoll_ctl向epoll中注册channel
    }
    else // channel已经在poller上注册过了
    {
        int fd = channel->fd();
        if (channel->isNoneEvent()) // 当前channel没有感兴趣的事件
        {
            //EPOLL_CTL_DEL：从epfd中删除一个fd
            update(EPOLL_CTL_DEL, channel); // 删除这个channel
            channel->set_index(kDeleted);
        }
        else
        {
            //EPOLL_CTL_MOD：修改已经注册的fd的监听事件
            update(EPOLL_CTL_MOD, channel); // 当前channel对新事件感兴趣，更新
        }
    }
}

// 从poller中删除channel
void EPollPoller::removeChannel(Channel *channel)
{
    int fd = channel->fd();
    channels_.erase(fd);

    LOG_INFO("func=%s => fd=%d\n", __FUNCTION__, fd);

    int index = channel->index();
    if (index == kAdded)
    {
        update(EPOLL_CTL_DEL, channel);
    }
    channel->set_index(kNew);
}

// 填写活跃的连接
void EPollPoller::fillActiveChannels(int numEvents, ChannelList *activeChannels) const
{
    for (int i = 0; i < numEvents; ++i)
    {
        // epoll_data结构体中.ptr类型为void *ptr，此处需要使用类型强转，转换为Channel *
        Channel *channel = static_cast<Channel *>(events_[i].data.ptr);

        channel->set_revents(events_[i].events);
        activeChannels->push_back(channel); // EventLoop就拿到了它的poller给它返回的所有发生事件的channel列表了
    }
}

//  更新channel通道 epoll_ctl add/mod/del
void EPollPoller::update(int operation, Channel *channel)
{
    /*
    epoll_event结构体：
    struct epoll_event {
        uint32_t events;  // epoll 事件类型，包括可读，可写等
        epoll_data_t data; // 用户数据，可以是一个指针或文件描述符等
    };

    epoll_data结构体：
    typedef union epoll_data {
        void *ptr; //ptr可以指向任何类型的用户数据
        int fd; //表示文件描述符
        uint32_t u32; //32位的无符号整数
        uint64_t u64; //64位的无符号整数
    } epoll_data_t;

    使用时，用户可以将自己需要的数据存放到这个字段中，当事件触发时，epoll系统调用会返回这个数据，以便用户处理事件。
    */
    epoll_event event;
    bzero(&event, sizeof event); // 置0

    int fd = channel->fd();

    event.events = channel->events();
    event.data.fd = fd;
    event.data.ptr = channel;

    if (::epoll_ctl(epollfd_, operation, fd, &event) < 0)
    {
        if (operation == EPOLL_CTL_DEL)
        {
            LOG_ERROR("epoll_ctl del error:%d\n", errno);
        }
        else
        {
            LOG_FATAL("epoll_ctl add/mod error:%d\n", errno);
        }
    }
}

/*
int epoll_ctl(int epfd，int op，int fd，struct epoll_event * event);
    1) int epfd： epoll_create()函数返回的epoll实例的句柄。
    2) int op： 需要执行的操作，添加，修改，删除。
    3) int fd： 需要添加，修改，删除的socket文件描述符
    4) struct epoll_event * event： 需要epoll监视的fd对应的事件类型

返回值：
成功epoll_ctl()返回0。错误返回-1。错误码error(详见第三节)会被设置。

*/

/*
Eventloop包含了Poller和CjannelList
    一个Eventloop含有一个Poller
    一个Eventloop含有多个channel
        一个Poller管理多个channel
*/