#include "Thread.h"
#include "CurrentThread.h"

#include <semaphore.h>

//静态成员变量，类外初始化
std::atomic_int Thread::numCreated_(0);

Thread::Thread(ThreadFunc func, const std::string &name)
    : started_(false)
    , joined_(false)
    , tid_(0)
    , func_(std::move(func))
    , name_(name)
{
    setDefaultName();
}

/*
detach()的作用是将子线程和主线程的关联分离
detach()后子线程在后台独立继续运行，主线程无法再取得子线程的控制权
即使主线程结束，子线程未执行也不会结束。
当主线程结束时，由内核资源负责清理与子线程相关的资源。
*/
Thread::~Thread()
{
    if (started_ && !joined_)
    {
        thread_->detach(); // thread类提供的设置分离线程的方法
    }
}

void Thread::start()  // 一个Thread对象，记录的就是一个新线程的详细信息
{
    started_ = true;
    sem_t sem;
    sem_init(&sem, false, 0);

    // 开启线程，产生一个线程对象
    thread_ = std::shared_ptr<std::thread>(new std::thread([&](){
        // 获取线程的tid值
        tid_ = CurrentThread::tid();
        /*
        sem_post( sem_t *sem )用来增加信号量的值
        */
        sem_post(&sem); //当执行到这里，说明上一行tid_已经获取到了，这时调用sem_post使得信号量+1，下面的sem_wait才能停止阻塞，然后返回
        // 开启一个新线程，专门执行该线程函数，里面就是一个eventloop
        func_(); //这个func()就是EventLoopThread中绑定的threadFunc()函数
    }));

    // 这里必须等待获取上面新创建的线程的tid值，使用信号量做同步
    /*
    用来阻塞当前线程直到信号量sem的值大于0，解除阻塞后将sem的值-1
    */
    sem_wait(&sem);//如果外部线程执行的更快，还没有等到上面新线程创建完，就准备执行这里了，那么调用sem_wait将会阻塞
}

//Thread.join方法是将指定线程加入当前线程，将两个交替执行的线程转换成顺序执行
void Thread::join()
{
    joined_ = true;
    thread_->join();
}

//如果创建的时候没有传入线程名，设置默认线程名
void Thread::setDefaultName()
{
    int num = ++numCreated_;
    if (name_.empty())
    {
        char buf[32] = {0};
        snprintf(buf, sizeof buf, "Thread%d", num);
        name_ = buf;
    }
}