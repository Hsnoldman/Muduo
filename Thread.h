#pragma once

#include "noncopyable.h"

#include <functional>
#include <thread>
#include <memory>
#include <unistd.h>
#include <string>
#include <atomic>

class Thread : noncopyable
{
public:
    //线程函数
    using ThreadFunc = std::function<void()>;

    /*
    explicit关键字用来修饰类的构造函数，被修饰的构造函数的类，不能发生相应的隐式类型转换，只能以显式的方式进行类型转换
    */
    explicit Thread(ThreadFunc, const std::string &name = std::string());
    ~Thread();

    void start();
    /*
    join()的使用场景
        在很多情况下，主线程创建并启动子线程，如果子线程中要进行大量的耗时运算
        主线程将可能早于子线程结束。如果主线程需要知道子线程的执行结果时
        就需要等待子线程执行结束了。主线程可以sleep(xx),但这样的xx时间不好确定
        因为子线程的执行时间不确定，join()方法比较合适这个场景。
    */
    void join();

    bool started() const { return started_; }
    pid_t tid() const { return tid_; }
    const std::string& name() const { return name_; }

    //获取产生的线程个数，因为需要对所有线程进行计数，所以用static修饰
    static int numCreated() { return numCreated_; }
private:
    void setDefaultName(); //给线程设置默认名称

    bool started_;
    bool joined_;
    std::shared_ptr<std::thread> thread_;
    pid_t tid_; //线程id
    ThreadFunc func_; //存储线程函数
    std::string name_; //线程名
    static std::atomic_int numCreated_; //记录产生的线程个数，原子类型
};