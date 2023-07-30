#pragma once
//时间类
#include <iostream>
#include <string>

// 时间类
class Timestamp
{
public:
    Timestamp();
    //声明为explicit的构造函数不能在隐式转换中使用。
    /*
    只能用于修饰只有一个参数的类构造函数, 它的作用是表明该构造函数是显示的, 而非隐式的
    跟它相对应的另一个关键字是implicit, 意思是隐藏的,类构造函数默认情况下即声明为implicit(隐式).
    也有一个例外, 就是当除了第一个参数以外的其他参数都有默认值的时候, explicit关键字依然有效
     此时, 当调用构造函数时只传入一个参数, 等效于只有一个参数的类构造函数
    */
    explicit Timestamp(int64_t microSecondsSinceEpoch);
    static Timestamp now();
    std::string toString() const;
private:
    /*
    int64_t是C++中的一种整型数据类型，占用8个字节(64位)
    它是一种固定长度的数据类型，可以保证在不同的平台上都有相同的位数和取值范围，适用于需要存储大整数的场合。
    */
    int64_t microSecondsSinceEpoch_;
};