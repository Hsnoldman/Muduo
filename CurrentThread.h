#pragma once

#include <unistd.h>
#include <sys/syscall.h>
//获取当前线程的tid
namespace CurrentThread
{
    extern __thread int t_cachedTid;

    void cacheTid();

    inline int tid()
    {
        if (__builtin_expect(t_cachedTid == 0, 0))
        {
            cacheTid();
        }
        return t_cachedTid;
    }
}

/*
通过系统调用获取线程tid，第一次获取之后进行缓存
之后如果需要获取会判断t_cachedTid==0
==0表示还未获取过，执行系统调用
!=0表示之前获取过，直接返回缓存值
*/