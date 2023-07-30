### 项目编译

./autobuild.sh

### Channel类

##### 成员

（fd,events,revents,callbacks）

封装了fd，events，revent和一组回调函数

fd 是往poller上注册的文件描述符

events是事先注册好的感兴趣事件，读事件，写事件

revents是poller最终通知的当前fd上真实发生的事件

#####  Channel所封装的fd类型

Channel中存在两种主要文件描述符，listenfd(socket)和connectionfd(accept)

- Listenfd封装成了acceptorChannel，connfd封装成了connectionChannel

- Listenfd上发生事件的回调函数由Acceptor类传入，connfd上发生的事件的回调函数由TcpConnection类传入

在muduo库中，不管是监听用的listenfd还是accept后返回的跟客户端专门通信的connectionfd，都会将这些fd打包成一个channel对象，然后注册到相应的Poller上去。

#####  资源监控

在channel中使用弱智能指针weak_ptr定义了一个成员变量tie_，进行跨线程监听，如果上层对象已经销毁（析构）了，那channel再执行这个回调就没有意义了。

```c++
std::weak_ptr<void> tie_;
```

防止channel被手动调用，（channel只能被它上层的Acceptor类和TcpConnection类调用）

当一个channel创建的时候，会调用相应的成员函数，将传入的指向上层对象的强智能指针shared_ptr赋给tie，因为tie_是弱智能指针，所以不会对引用计数造成影响。

在channel开始处理事件的时候，先调用tie_.lock()函数，看能否将这个弱智能指针提升为强智能指针，如果可以提升成功，说明上层对象存在，那继续执行回调，处理事件，否则，做资源异常处理。

```c++
if (tied_)
    {
        std::shared_ptr<void> guard = tie_.lock(); 
        if (guard)//提升成功
        {
            /*执行回调，处理事件*/
        }
    }
    else{/*进行错误处理*/}
```

##### Channel类的主要作用

- 封装各种fd

- 执行上层类注册的各种回调函数

### Poller类

Poller中有一个成员变量channels，hash表结构,存储该类管理的所有channel对象

其中key为sockfd，value为sockfd所属的channel对象

```c++
   using ChannelMap = std::unordered_map<int, Channel *>;
   ChannelMap channels_; //channel数组
```

当poller检测到哪个fd有事件发生，会通过hash表查到该事件对应的channel对象，channel对象负责执行相应事件的回调函数。

```c++
auto it = channels_.find(channel->fd());
```

Channel和Poller互相独立，不能直接通信，需要依赖EventLoop类进行通信

Channel和Poller中都有一个EventLoop *loop成员，记录它所属的EventLoop。

```c++
private:
    EventLoop *ownerLoop_; 
```

Poller类中的虚函数接口

```c++
 // 使用虚函数，给所有IO复用保留统一的接口，由派生类实现
    virtual Timestamp poll(int timeoutMs, ChannelList *activeChannels) = 0;
    virtual void updateChannel(Channel *channel) = 0;
    virtual void removeChannel(Channel *channel) = 0;
```

Poller是一个抽象类，在mymuduo中通过Epoll实现

在EpollPoller中，通过底层调用epoll_ctl，将channel中的fd注册到epoll上，当监听到事件后，epoll返回，通过channels（hashmap）找到fd对应的channel对象，调用相应的回调函数。

EventLoop类就是Reactor，Poller就是Demultiplex，事件分发器。

##### EpollPoller类

##### 相关成员变量

epoll对象的fd

```c++
int epollfd_;
```

epoll事件数组

```c++
using EventList = std::vector<epoll_event>;
EventList events_;
```

定义时间数组监听的最大文件描述符个数

```c++
static const int kInitEventListSize = 16;
```

##### 相关成员函数

当检测的活跃事件后，通过调用这个函数将活跃的channel添加到activeChannels数组中，上层的EventLoop通过查看activeChannels数组来获取活跃事件进行处理。

```c++
void fillActiveChannels(int numEvents, ChannelList *activeChannels) const;
```

实现Poller类的虚函数接口：

- 函数通过epoll_wait()监听到那些文件发生了事件，把发生事件的channel，填入ChannelList中，即：将发生事件的channel告知EventLoop

```c++
Timestamp poll(int timeoutMs, ChannelList *activeChannels) override;
```

- 将EventLoop传入的新的channel添加到Poller的channel表中进行管理，调用epoll_ctl()向epoll中注册该channel的fd

```c++
void updateChannel(Channel *channel) override;
```

- 从Poller管理的channel表中删除这个channel，从epoll中移除该channel的fd

```c++
void removeChannel(Channel *channel) override;
```

EpollPoller类构造函数会通过调用epoll_creat()创建一个epoll文件描述符

```c++
EPollPoller::EPollPoller(EventLoop *loop) : 
							Poller(loop),
							epollfd_(::epoll_create1(EPOLL_CLOEXEC)),
							events_(kInitEventListSize){}
```

##### EpollPoller类主要作用

1.实现了具体的epoll操作，如epoll_creat()，epoll_ctl()，epoll_wait()

2.负责监听EventLoop类传入的channel，并将活跃channel返回。

### EventLoop类

##### 相关成员变量

成员变量activeChannels中存储了所有的channel

```c++
using ChannelList = std::vector<Channel *>;
ChannelList activeChannels_;
```

wakeupFd成员变量

每个loop中都含有一个wakeupfd，当想要唤醒某一个loop所在的线程，通过向这个wakeupfd上写一个数据，唤醒该线程，即唤醒该loop

```c++
int wakeupFd_;
```

每个loop中的wakeupfd也封装成了channel，注册在了自己所管理的poller中。

```c++
std::unique_ptr<Channel> wakeupChannel_;
```

一个EventLoop管理一堆Channel和一个Poller

当一个Channel想要把自己注册到Poller上，或者在Poller上修改自己感兴趣的事件，都是通过EventLoop进行。

当Poller检测到相应的socketfd上有事件发生，通过EventLoop调用相应channel上的回调函数。

每一个回调函数在执行的时候，都应该在loop自己所在的线程执行。

当一个线程将要执行回调的时候，需要判断，如果当前线程就是loop线程，则直接执行，否则就将需要执行的回调函数存储在pendingFunctors_中，然后唤醒相应的loop去vector中拿到这个回调函数执行。

```c++
using Functor = std::function<void()>;
std::vector<Functor> pendingFunctors_;    // 存储loop需要执行的所有的回调操作
```

EventLoop类的构造函数会创建一个Poller(EpollPoller)对象，并且创建一个自己的wakeupfd，通过

创建一个Channel对象将这个wakeupfd封装成wakeupChannel

##### EventLoop类的主要作用

1.拥有一个Poller和多个Channel，管理Poller和Channel间的通信。

2.当Poller返回了活跃事件时，执行上层类（TcpServer，TcpConnection）注册的回调函数。



因为每个EventLoop都管理了自己独有的Poller和Channel，所以EventLoop中的Poller和

Channel数组都通过unique_ptr修饰。

```c++
std::unique_ptr<Poller> poller_;
std::unique_ptr<Channel> wakeupChannel_;
```

unique_ptr 是 C++ 11 提供的用于防止内存泄漏的智能指针中的一种实现，独享被管理对象指针所有权的智能指针。

在EventLoop中维护了一个回调函数的vector，存储mainloop需要subloop执行的回调函数。

1.当前线程需要执行回调操作了，但是当前线程此时在mainloop中，则需要先将回调函数注册到存储回调函数的vector中，之后唤醒subloop来执行。

2.当subloop具体执行回调函数的时候，需要从存储回调的vector中把回调函数拷贝到自己线程中的临时vector数组中，然后执行。

在这个拷贝的过程中，需要加锁来保证线程安全操作。定义这个锁对象的时候，用unique_ptr管理。

```c++
std::unique_lock<std::mutex> lock(mutex_);
```

### Thread类

底层线程

调用start()成员函数创建新线程

创建新线程的时候，需要等待新线程对象创建完毕后才能获取线程tid，因为线程的执行没有顺序可言，因此，此处需要用到信号量机制进行同步，等到新线程创建完毕后，才能获取其线程id，之后start()才能返回。

使用lambda表达式以引用的方式接收外部线程对象，因此就可以在新线程中以引用访问外部线程的成员变量了

```c++
void Thread::start()  // 一个Thread对象，记录的就是一个新线程的详细信息
{
    started_ = true;
    sem_t sem;
    sem_init(&sem, false, 0); //初始化
    // 开启线程，产生一个线程对象
    thread_ = std::shared_ptr<std::thread>(new std::thread([&](){
        // 获取线程的tid值
        tid_ = CurrentThread::tid();
        sem_post(&sem);
        // 开启一个新线程，专门执行该线程函数，里面就是一个eventloop
        func_(); //这个func()就是EventLoopThread中绑定的threadFunc()函数
    }));
    sem_wait(&sem);
}
```

有一个成员函数 numCreated() 和成员变量 numCreated_，用来记录所有创建的线程数量。

```c++
static std::atomic_int numCreated_;
static int numCreated() { return numCreated_; }
```

因为需要对所有的线程计数，属于类的公有资源，所以都用static修饰。类内声明，类外初始化

```c++
std::atomic_int Thread::numCreated_(0);
```



使用c++提供的线程类，编写相比linux原生thread，效率更高，头文件<thread>

不能直接通过Thread worker_thread 定义一个对象，因为这样定义的话，定义完之后线程就直接启动了。

因为需要控制线程启动的时机，所以通过智能指针来管理创建的线程对象。

每一个新创建的线程对象用shared_ptr管理

```c++
thread_ = std::shared_ptr<std::thread>(/*new thread*/)
```

### EventLoopThread类

将loop和thread绑定起来，一个loop对应一个线程，one loop per thread

在本类里对loop成员变量的一些访问需要用锁保证安全。

### EventLoopThreadPoll类

事件循环线程池，baseloop通过该类来管理所有的subloop

有一个成员方法getNEXTLoop()通过轮询算法获取下一个subloop

```c++
EventLoop* EventLoopThreadPool::getNextLoop()
{
    EventLoop *loop = baseLoop_; //先指向baseloop，如果loop_没有其他loop，那最终返回的就是baseloop
    if (!loops_.empty()) // 通过轮询获取下一个处理事件的loop
    {
        loop = loops_[next_];
        ++next_;
        if (next_ >= loops_.size())
        {
            next_ = 0;
        }
    }
    return loop;
}
```

如果用户没有通过setThreadNum设置过线程数量，则默认只有一个线程工作。getNEXTLoop()方法就无法获取到subloop，会返回baseloop。

当设置了setThreadNum后，调用getNEXTLoop()会开始创建subloop线程，one loop per thread。一个线程对应一个loop。

setThreadNum()设置的线程数量不包括baseloop

### Socket类

  对socket的封装

```c++
    int fd() const { return sockfd_; }
    void bindAddress(const InetAddress &localaddr);
    void listen();
    int accept(InetAddress *peeraddr);
```

  相关函数：

```c++
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
```

  Acceptor类创建一个监听socket，将这个listenfd用Socket类实例化一个对象进行接收。

### Acceptor类

主要封装了listenfd相关的操作，创建socket bind listen ，并将创建好的fd打包成acceptChannel，（封装成Channel），返回给baseloop监听（通过Poller）

Acceptor构造函数列表初始化中，调用createNonblocking()创建一个非阻塞socket，将创建的socket用Socket类实例化一个对象（acceptSocket_）进行接收。

```c++
Socket acceptSocket_;
```

```c++
Acceptor::Acceptor(EventLoop *loop, const InetAddress &listenAddr, bool reuseport)
    : loop_(loop), acceptSocket_(createNonblocking()), // socket
      acceptChannel_(loop, acceptSocket_.fd()), listenning_(false)
{
    acceptSocket_.setReuseAddr(true);
    acceptSocket_.setReusePort(true);
    acceptSocket_.bindAddress(listenAddr); // bind
    acceptChannel_.setReadCallback(std::bind(&Acceptor::handleRead, this));
}
```

```c++
static int createNonblocking()
{
    int sockfd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    return sockfd;
}
```

Acceptor将上面创建的Socket封装成acceptChannel（一个Channel对象）

```c++
Channel acceptChannel_;
```

```c++
acceptChannel_(loop, acceptSocket_.fd())
```

在Acceptor中定义一个回调函数（handleRead），将这个函数绑定到acceptChannel中的读事件回调（因为处理新连接是一个读事件），当新连接到来时，acceptChannel会执行这个回调函数。

```c++
NewConnectionCallback newConnectionCallback_; // 新连接到来时的回调
```

```c++
    void setNewConnectionCallback(const NewConnectionCallback &cb)
    {
        newConnectionCallback_ = std::move(cb);
    }
```

```c++
acceptChannel_.setReadCallback(std::bind(&Acceptor::handleRead, this));
```

handleRead函数的具体操作，通过acceptSocket_.accept()函数创建一个connfd来管理这个新连接。这里的connfd不同于acceptChannel封装的这个fd，acceptChannel是负责监听的socket的fd，connfd是新连接的fd。

之后handleRead执行newConnectionCallback_回调函数，这个函数通过轮询找到一个subloop，唤醒这个subloop，将这个connfd封装成channel，然后由这个channel处理这个连接。

```c++
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
        if (errno == EMFILE) // 超过文件描述符上限
        {/*错误处理*/}
    }
}
```

上面这个newConnectionCallback_函数由TcpServer定义，就是TcpServer中的newConnection()函数。

### TcpConnection类

一个连接成功的客户端对应一个Tcpconnection

封装了一个socket ，一个channel，各种回调，发送缓冲区和接收缓冲区

##### 相关成员

这个loop指针一定指向一个subloop对象，因为所有的TcpConnection都是在subloop中管理的。

```c++
EventLoop *loop_;
```

管理一个Socket对象

```c++
std::unique_ptr<Socket> socket_;
```

管理一个channel对象，即管理这条连接的connfd

```c++
std::unique_ptr<Channel> channel_;
```

enum StateE 标识当前连接状态的枚举类型

```c++
enum StateE {kDisconnected, kConnecting, kConnected, kDisconnecting};
```

定义了channel中需要的各种回调函数实体closeCallback_，errorCallback_，readCallback_，writeCallback_

给封装了connfd的channel设置这些回调函数

```c++
void handleRead(Timestamp receiveTime); //具体的读事件处理逻辑
void handleWrite(); //具体的写事件处理逻辑
void handleClose(); //具体的关闭事件处理逻辑
void handleError(); //具体的错误件处理逻辑   
```

在TcpConnection构造函数中，会给channel绑定这些回调函数

```c++
//TcpConnection构造函数体内
{
	channel_->setReadCallback(std::bind(&TcpConnection::handleRead, this, std::placeholders::_1));
    channel_->setWriteCallback(std::bind(&TcpConnection::handleWrite, this));
    channel_->setCloseCallback(std::bind(&TcpConnection::handleClose, this));
    channel_->setErrorCallback(std::bind(&TcpConnection::handleError, this));
}
```

定义了建立连接和销毁当前连接的函数

```c++
 	// 连接建立
    void connectEstablished();
    // 连接销毁
    void connectDestroyed();
```

##### send()函数

send()里会判读当前线程是不是当前loop对象对应的线程

如果是，就调用sendInLoop()直接发送数据，如果不是，就调用当前loop对象的runInLoop函数执行（loop_->runInLoop()）

```c++
void TcpConnection::send(const std::string &buf)
{
    if (state_ == kConnected)
    {
        if (loop_->isInLoopThread())
        {
            sendInLoop(buf.c_str(), buf.size());
        }
        else
        {
            loop_->runInLoop(/*参数*/);
        }
    }
}
```

具体的发送逻辑由sendInLoop()函数定义，send()函数处理完上面的逻辑就调用sendInLoop()

sendInLoop()函数相关操作：

判断当前connection的状态，如果是关闭状态，就不再进行后续操作了，打印错误日志

判断当前缓冲区情况

判断当前Channel有没有之前没发送完的数据

底层调用linux中的write()函数发送数据，发送完成后执行用户设置的写操作完成回调

```c++
::write(channel_->fd(), data, len);
```

当数据没有一次发送完，需要把剩余数据放入发送缓冲区，并给当前channel设置一个epollout事件，等待下次调用

 发送完成之后对缓冲区进行一些更新处理



使用std::enable_shared_from_this<TcpConnection>管理当前TcpConnection对象，在需要传递时使用

shared_ptr中的shared_from_this()函数获取管理当前TcpConnection对象的智能指针，并传递。

例：当需要执行回调时

```c++
connectionCallback_(shared_from_this());
```

### TcpServer类

TcpServer类管理所有的类

##### 主要成员

一个Acceptor对象，用来监听新连接的到来

```c++
   std::unique_ptr<Acceptor> acceptor_; // 运行在mainLoop，任务就是监听新连接事件
```

一个EventLoopThreadPool对象，用来管理所有的subloop

```c++
    std::shared_ptr<EventLoopThreadPool> threadPool_; // one loop per thread
```

一个保存所有已建立连接的的Hash表

```c++
	using ConnectionMap = std::unordered_map<std::string, TcpConnectionPtr>;
    ConnectionMap connections_; // 保存所有的连接，hash表
```

TcpServer中会提供一些设置回调函数的成员方法

这些回调函数由用户创建TcpServer对象时设置，设置后，由TcpServer传给TcpConnection，最终由TcpConnection调用

```c++
  void setThreadInitcallback()   //设置EventLoopThread中线程初始化时的回调
  void setConnectionCallback()   //设置新连接到来时的回调
  void setMessageCallback()  //设置有消息到来时的回调
  void setWriteCompleteCallback() //设置消息发送完成以后的回调
```

设置底层线程数量，即subloop的个数

```c++
void setThreadNum(int numThreads);
```

start()函数，开启服务

```c++
void TcpServer::start()
{
    if (started_++ == 0) // 防止一个TcpServer对象被start多次
    {
        threadPool_->start(threadInitCallback_); // 启动底层的loop线程池
        loop_->runInLoop(std::bind(&Acceptor::listen, acceptor_.get()));
    }
}
```

##### 构造函数

TcpServer构造函数中会创建一个Acceptor对象，这个Acceptor对象的构造函数中会创建一个负责监听的socket，并用Socket类实例化一个对象来接收这个socket，然后将这个Socket类对象打包成acceptchannel，并给这个acceptchannel注册回调函数handleRead()，当新连接到来时，acceptchannel会执行这个回调通知Acceptor。

```c++
//TcpServer构造函数
TcpServer::TcpServer(EventLoop *loop, const InetAddress &listenAddr, const std::string &nameArg, Option option) : 
	loop_(CheckLoopNotNull(loop)),
	ipPort_(listenAddr.toIpPort()), 
	name_(nameArg), 
	acceptor_(new Acceptor(loop, listenAddr, option == kReusePort)), 
	threadPool_(new EventLoopThreadPool(loop, name_)), 
	connectionCallback_(), 
	messageCallback_(), 
	nextConnId_(1), 
	started_(0)
{
    // 当有新用户连接时，会执行TcpServer::newConnection回调
    acceptor_->setNewConnectionCallback(std::bind(&TcpServer::newConnection, this,
                                                  std::placeholders::_1, std::placeholders::_2)); // 两个参数 connfd, peerAddr
}
```

定义newConnection回调函数实体

这个函数在TcpServer构造函数体中通过setConnectionCallback()方法设置到Acceptor中，当有新连接到来，acceptor调用这个函数。

newConnection()函数具体操作：

```c++
void TcpServer::newConnection(int sockfd, const InetAddress &peerAddr){/*函数体*/}
```

- 从EvnetLoopThreadPool中选择一个subloop

```c++
    // 轮询算法，选择一个subLoop，来管理channel
    EventLoop *ioLoop = threadPool_->getNextLoop();
```

- 根据Acceptor返回的连接成功的sockfd（connfd），创建一个TcpConnection对象conn
- 给conn设置相应的回调函数：

```c++
    conn->setConnectionCallback(connectionCallback_);
    conn->setMessageCallback(messageCallback_);
    conn->setWriteCompleteCallback(writeCompleteCallback_);
    // 设置了如何关闭连接的回调   conn->shutDown()
    conn->setCloseCallback(
        std::bind(&TcpServer::removeConnection, this, std::placeholders::_1));
```

- 让subloop执行runInLoop()开始工作

```c++
    // 直接调用TcpConnection::connectEstablished
    ioLoop->runInLoop(std::bind(&TcpConnection::connectEstablished, conn));
```

##### 析构函数

在TcpServer的析构函数中，因为需要析构所有的new出来的conn对象，但这些对象都被相应的智能指针管理，引用计数不为0，所以没法析构。

因此，在析构函数中，创建了一个指向该连接的智能指针对象，这个智能指针对象的作用域在该函数体中，是一个临时对象，出作用域自动析构，之后调用智能指针的reset()方法，使得这个临时智能指针对象代替本来的智能指针接管这个conn对象，那出析构函数作用域后，这个conn对象就可以自动被释放了。

```c++
TcpServer::~TcpServer()
{
    for (auto &item : connections_)
    {
        TcpConnectionPtr conn(item.second); //item.second为一个shared_ptr<TcpConnection>
        item.second.reset(); //使得这个临时智能指针对象接管该conn资源
        // 销毁连接
        conn->getLoop()->runInLoop(std::bind(&TcpConnection::connectDestroyed, conn));
    }
}
```

