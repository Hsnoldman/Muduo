#pragma once

/**
 * noncopyable被继承以后，派生类对象可以正常的构造和析构，但是派生类对象
 * 无法进行拷贝构造和赋值操作
 */
class noncopyable
{
public:
    // 希望一个类不能被拷贝，把构造函数定义为private，C++11：在构造函数后面加上=delete修饰
    noncopyable(const noncopyable &) = delete;
    noncopyable &operator=(const noncopyable &) = delete;

protected:
    noncopyable() = default; //使用默认构造
    ~noncopyable() = default;
};