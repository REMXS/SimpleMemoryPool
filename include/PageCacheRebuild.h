#pragma once
#include"Common.h"
#include<mutex>
#include<map>
#include<sys/mman.h>
#include<unordered_set>
namespace tyMemoryPool
{
class PageCacheRe
{
private:
    //使用侵入式span链表
    //0-7 上个Span的地址
    //8-15 内存首地址
    //16-23 页面的数量
    //24-31 下个Span的地址
    // #define OFFSET_ADDR 8
    // #define OFFSET_PREV 0
    // #define OFFSET_PAGENUM 16
    // #define OFFSET_NEXT 24

    // inline void** getNextPtr(void* span){
    //     return reinterpret_cast<void**>(static_cast<char*>(span)+OFFSET_NEXT);
    // }
    // inline void** getPrevPtr(void* span){
    //     return reinterpret_cast<void**>(static_cast<char*>(span)+OFFSET_PREV);
    // }
    // inline void** getAddrPtr(void* span){
    //     return reinterpret_cast<void**>(static_cast<char*>(span)+OFFSET_ADDR);
    // }
    // inline size_t* getPageNumPtr(void* span){
    //     return reinterpret_cast<size_t*>(static_cast<char*>(span)+OFFSET_PAGENUM);
    // }


    struct SpanControl{
        void* prev;
        void* addr;
        size_t pageNum;
        void* next;
    };

    struct Span{
        Span():size(0),spanList(nullptr){}
        size_t size;//链表的节点数量
        void*spanList;//链表头节点
    };
    
    //空闲的不同大小的Span链表
    std::map<size_t,Span>_freeSpanList;

    //通过首地址来映射span对象，用于回收（由于首地址的位置就是span的位置，所以不用map存储）
    std::unordered_set<void*>_spanMap;

    //std::atomic_flag _um_lock;
    std::mutex _mtx;

    
    /// @brief 获取span的SpanControl对象
    /// @param span span
    /// @return 对应的SpanControl对象
    inline SpanControl* getSpanControl(void*span){
        return static_cast<SpanControl*>(span);
    }
    

    /// @brief 从系统中获取内存
    /// @param numPages 要获取内存的页数
    /// @return 内存的地址
    void* getMemroyFromSystem(size_t numPages);

    /// @brief 注册span
    /// @param span span的指针
    /// @param addr 内存首地址
    /// @param pageNum span的页数
    void registSpan(void* span,void* addr,size_t pageNum);

    /// @brief 检查是否需要向系统返还内存
    /// @param pageNum span中的页数，用于定位_freeSpanList中对应的链表
    /// @return 
    bool shouldReturnMemory(size_t pageNum);

    /// @brief 向系统返还内存
    /// @param pageNum span中的页数，用于定位_freeSpanList中对应的链表
    void returnMemroyToSystem(size_t pageNum);


public:
    //每一个页的大小为4k
    static constexpr size_t PAGESIZE=4096;

    static PageCacheRe& getInstance(){
        static PageCacheRe instance;
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