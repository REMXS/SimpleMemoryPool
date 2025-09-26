#pragma once
#include<cstddef>
#include<atomic>
#include<array>
#include<memory>
#include<iostream>
#include<thread>


#define DEBUG 1

#if DEBUG
    #define DEBUG_CODE(code) code
#else
    #define DEBUG_CODE(code)
#endif


namespace tyMemoryPool
{

    
// 对齐数和大小定义
constexpr size_t ALIGNMENT = 8; //对齐数
constexpr size_t MAX_BYTES = 256 * 1024;  //最大的分配的大小，大于此大小的内存直接调用malloc分配 // 256KB
constexpr size_t FREE_LIST_SIZE = MAX_BYTES / ALIGNMENT; // ALIGNMENT等于指针void*的大小

class SizeClass{
public:
    static inline size_t roundUp(size_t bytes){
        return (bytes+ALIGNMENT-1)& ~(ALIGNMENT-1);
    }


    static inline size_t getIndex(size_t bytes){
        //0-8 对齐为8，取下标0，9-16 对齐为16 取下标1
        if(!bytes) return 0;
        return (bytes-1)/ALIGNMENT;

    }
};

class SpinLockGuard {
    std::atomic_flag& lock_;
public:
    SpinLockGuard(std::atomic_flag& lock) : lock_(lock) {
        while(lock_.test_and_set(std::memory_order_acquire)) {
            //让出cpu，避免让cpu忙等待
            std::this_thread::yield();
        }
    }
    ~SpinLockGuard() { lock_.clear(std::memory_order_release); }
};

}