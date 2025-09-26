#pragma once
#include"Common.h"

namespace tyMemoryPool
{
class CentralCache
{
private:
    //不同大小中心缓存的自由链表
    std::array<std::atomic<void*>,FREE_LIST_SIZE>_centralFreeList;
    //对应每个自由链表的自旋锁
    std::array<std::atomic_flag,FREE_LIST_SIZE>_lockList;
    
    CentralCache(){
        for(auto&l:_lockList){
            l.clear();
        }
        for(auto&ptr:_centralFreeList){
            ptr.store(nullptr,std::memory_order_release);
        }
    };
    
    /// @brief 从页缓存中获取span
    /// @param idx 对应中心缓存自由链表数组的下标
    /// @return first:span的首地址 second:实际获取的span中的页数
    std::pair<void*,size_t>fetchFromPageCache(size_t idx);

public:
    /// @brief 从中心缓存中获取内存块
    /// @param idx 对应中心缓存自由链表数组的下标
    /// @param batchNum 获取内存块的数量
    /// @return first:第一个内存块的地址 second:实际返回的内存块的数量
    std::pair<void*,size_t> fetchRange(size_t idx,size_t batchNum);

    /// @brief 向中心缓存返还内存块
    /// @param start 首个内存块的地址
    /// @param idx 自由链表对应的下标
    void returnRange(void*start,size_t idx);

    static CentralCache& getInstance(){
        static CentralCache instance;
        return instance;
    }
};

}