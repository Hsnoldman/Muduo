 #include "EventLoopThread.h"
#include "EventLoop.h"


EventLoopThread::EventLoopThread(const ThreadInitCallback &cb, 
        const std::string &name)
        : loop_(nullptr)
        , exiting_(false)
        , thread_(std::bind(&EventLoopThread::threadFunc, this), name)
        , mutex_()
        , cond_()
        , callback_(cb)
{

}

EventLoopThread::~EventLoopThread()
{
    exiting_ = true;
    if (loop_ != nullptr)
    {
        loop_->quit();
        thread_.join(); //事件循环结束，相应的线程也退出
    }
}

EventLoop* EventLoopThread::startLoop()
{
    thread_.start(); // 启动底层的新线程

    /*
    上面thread_.start()最终调用到下面的threadFunc()函数创建一个新的loop对象
    然后将这个loop对象绑定到loop_指针
    因为多线程执行，线程顺序不能保证，所以需要加锁控制，当threadFunc()中的loop创建好后并成功绑定到了loop_
    loop_才能被正常访问，下面的loop = loop_才能正常执行
    */
    EventLoop *loop = nullptr;
    {
        std::unique_lock<std::mutex> lock(mutex_); //互斥锁
        while ( loop_ == nullptr )
        {
            cond_.wait(lock);
        }
        loop = loop_;
    }
    return loop;
}

// 下面这个方法，是在单独的新线程里面运行的
void EventLoopThread::threadFunc()
{
    EventLoop loop; // 创建一个独立的eventloop，和上面的线程是一一对应的，[one loop per thread]



    // TcpServer中的ThreadInitCallback
    if (callback_)
    {
        callback_(&loop);
    }

    {
        //这里需要将loop_绑定到创建的loop，这一过程不能对上面创建的loop进行修改，所以需要加锁保证安全
        std::unique_lock<std::mutex> lock(mutex_);
        loop_ = &loop;
        /*
        notify_one()
            唤醒一个正在等待 condition_variable 的线程，使其获得锁并继续执行。
            它只会唤醒一个等待的线程，所以如果有多个线程在等待，那么只有一个线程会被唤醒，其他线程还是处于等待状态。
        使用 notify_one() 的方法是：
            1.在线程中使用 std::unique_lock 对象来锁定 condition_variable
            2.然后调用 condition_variable 的 wait() 方法来等待
            3.当线程需要唤醒其他线程时，调用 notify_one() 来唤醒等待的线程。

        */
        cond_.notify_one();
    }

    //启动
    loop.loop(); // EventLoop loop  => Poller.poll
    
    //服务器正常工作时，会一直在上面的loop.loop()里
    //当服务器关闭，事件循环结束时，才执行下面的语句

    //loop_置空时，需要用锁保护，防止其他地方正在访问loop_
    std::unique_lock<std::mutex> lock(mutex_);
    loop_ = nullptr;
}