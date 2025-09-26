#pragma once
#include"Common.h"
#include<mutex>
#include<map>
#include<sys/mman.h>
#include<unordered_map>
namespace tyMemoryPool
{
class PageCache
{
private:
    struct Span{
        Span* prev;
        void* addr;//内存首地址
        size_t pageNum;//页面的数量
        Span* next;//下个Span的地址
    };
    //空闲的不同大小的Span链表
    std::map<size_t,Span*>_freeSpanList;
    //通过首地址来映射span对象，用于回收
    std::unordered_map<void*,Span*>_spanMap;
    //std::atomic_flag _um_lock;
    std::mutex _mtx;


    /// @brief 从系统中获取内存
    /// @param numPages 要获取内存的页数
    /// @return 内存的地址
    void* getMemroyFromSystem(size_t numPages);

    
    /// @brief 注册span
    /// @param span span的指针
    /// @param addr 内存首地址
    /// @param pageNum span的页数
    void registSpan(Span* span,void* addr,size_t pageNum);


public:
    //每一个页的大小为4k
    static constexpr size_t PAGESIZE=4096;
    static PageCache& getInstance(){
        static PageCache instance;
        return instance;
    }
    /// @brief 获取页缓存中指定页数的span
    /// @param pageNum span当中页的数量
    /// @return span的首地址
    void* allocateSpan(size_t pageNum);


    /// @brief 回收已经分配的Span
    /// @param addr Span的首地址
    /// @param pageNum Span的页数
    void deallocateSpan(void* addr,size_t pageNum);
};


}