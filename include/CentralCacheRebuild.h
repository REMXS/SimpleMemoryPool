#pragma once
#include"Common.h"
#include<map>

namespace tyMemoryPool
{

class CentralCacheRe
{
private:

    // 定义map的分片数量
    static constexpr size_t NUM_SHARDS = 128;
    
    //每一个span的信息
    struct SpanInfo {
        SpanInfo()
            :prev(nullptr),addr(nullptr),pageNum(0),freeList(nullptr),freeCount(0),totalBlocks(0),next(nullptr){}
        SpanInfo* prev;//上一个一个spaninfo的地址
        void*addr;//span首地址
        size_t pageNum;//页数
        void* freeList; // 此span的空闲块链表
        size_t freeCount;//span中空闲内存块的数量
        size_t totalBlocks;//span中内存块的总数
        SpanInfo* next;//下一个spaninfo的地址
    };

    //每一个自由链表的信息
    struct ListInfo
    {
        //指向当前的可用span
        SpanInfo* currSpan;
        //中心缓存的span链表
        //链表采用头插法
        SpanInfo*spanList;
        //对应每个自由链表的自旋锁
        std::atomic_flag lock;
        //每一个自由链表中完全空闲span的数量
        size_t freeSize;
    };

    //中心缓存的自由链表
    std::array<ListInfo,FREE_LIST_SIZE>_centralFreeList;

    //每一个span的注册信息，用于通过内存块地址快速找到span对应的地址
    //map采用分片映射来提高并发访问性能
    std::array<std::map<void*,SpanInfo*>,NUM_SHARDS>_spanRegistry;
    std::array<std::atomic_flag,NUM_SHARDS>_mapLocks;

    /// @brief 寻找span对应的分片
    /// @param idx span链表的下标
    /// @return span对应分片的下标
    inline size_t findShard(size_t idx);

    /// @brief 检查是否需要向pagechache返还span
    /// @param idx span链表的下标
    /// @return 
    bool shouldReturnToPageCache(size_t idx);

    CentralCacheRe(){
        for(auto&l:_mapLocks) l.clear();
        for(auto&list:_centralFreeList)
        {
            list.lock.clear();
            list.freeSize=0;
            list.spanList=nullptr;
        }
    };
    
    /// @brief 从页缓存中获取span
    /// @param idx 对应中心缓存自由链表数组的下标
    /// @return first:span的首地址 second:实际获取的span中的页数
    std::pair<void*,size_t>fetchFromPageCache(size_t idx);


    /// @brief 注册一个span
    /// @param start span内部链表的首地址
    /// @param totalBlocks 一共分配的内存块的数目
    /// @param addr span内存的首地址
    /// @param pageNum span的页数
    /// @param idx span中分割的对应大小内存块对应的下标
    void registSpan(void* start,size_t totalBlocks,void* addr,size_t pageNum,size_t idx);
    
    /// @brief 注销span
    /// @param span 要注销的span
    /// @param idx span对应_spanCentralFreeList的下标
    void cancelSpan(SpanInfo*span,size_t idx);

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

    static CentralCacheRe& getInstance(){
        static CentralCacheRe instance;
        return instance;
    }
};

}