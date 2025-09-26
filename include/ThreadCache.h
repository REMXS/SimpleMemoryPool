#pragma once
#include"Common.h"

namespace tyMemoryPool
{
class ThreadCache
{
private:
    static constexpr size_t THRESHOLD=64;
    //存放自由链表的数组
    std::array<void*,FREE_LIST_SIZE> _freeList; 
    //存放每一个链表大小的数组
    std::array<size_t,FREE_LIST_SIZE>_listSize;

    /// @brief 从中心缓存中获取内存(只有在自由链表为空时才调用此函数)
    /// @param idx 中心缓存中对应的自由链表的下标
    /// @return 第一个内存块的地址
    void* fetchFromCentralCache(size_t idx);


    /// @brief 向中心内存返还内存
    /// @param idx 中心缓存中对应的自由链表的下标
    void returnToCentralCache(size_t idx);


    /// @brief 获取对应内存块大小对应的块数量
    /// @param size 内存块的大小
    /// @return 内存块的数量
    size_t getBatchNum(size_t size);

    /// @brief 判断是否需要向中心缓存返还内存
    /// @param idx 自由链表数组的下标
    /// @return 
    inline bool shouldReturnToCentralCache(size_t idx);
    
public:

    /// @brief 分配内存
    /// @param size 分配内存的真实大小
    /// @return 内存地址
    void* allocate(size_t size);


    /// @brief 释放内存
    /// @param addr 内存的地址
    /// @param size 释放内存的大小
    void deallocate(void*addr,size_t size);

    ThreadCache()=default;
    ThreadCache(const ThreadCache&other)=delete;
    ThreadCache& operator=(const ThreadCache&other)=delete;
    ~ThreadCache();
};


}