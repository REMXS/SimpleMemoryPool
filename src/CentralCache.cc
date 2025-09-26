#include"Common.h"
#include"PageCache.h"
#include"CentralCache.h"
#include<thread>

namespace tyMemoryPool{
static constexpr size_t SPAN_PAGES=8;
std::pair<void*,size_t> CentralCache::fetchRange(size_t idx,size_t batchNum){
    //安全性检查
    if(idx>=FREE_LIST_SIZE||batchNum==0) 
        return {nullptr,0};
    
    //获取锁
    while(_lockList[idx].test_and_set(std::memory_order_acquire))
    {
        //线程让步，避免线程忙等待
        std::this_thread::yield();
    }
    void* ret=nullptr;
    try
    {
        ret=_centralFreeList[idx].load(std::memory_order_relaxed);
        //如果中心自由链表中有内存块，则直接拿取
        if(ret)
        {
            char* current=static_cast<char*>(ret);
            size_t currNum=0;
            char* prev=nullptr;
            while(current&&currNum<batchNum){
                currNum++;
                prev=current;
                current=static_cast<char*>(*reinterpret_cast<void**>(ret));
            }
            //切断链表
            if(prev)
            {
                *reinterpret_cast<void**>(prev)=nullptr;
            }
            //更新中心自由链表
            _centralFreeList[idx].store(static_cast<void*>(current));

            _lockList[idx].clear(std::memory_order_release);
            return {ret,currNum};
        }
        else
        {
            //获取一整个span
            auto[start,span_page]=fetchFromPageCache(idx);
            if(!start)
            {
                std::cerr<<"fail to get page cache\n";
                return {nullptr,0};
            }
            ret=start;
            //索引为idx的自由链表所对应的块的大小
            size_t blockSize=(idx+1)*ALIGNMENT;

            size_t totalBlock=(span_page*PageCache::PAGESIZE)/blockSize;

            size_t allowBlock=std::min(totalBlock,batchNum);

            //切割链表
            char*splitSpot=static_cast<char*>(start)+(blockSize*(allowBlock-1));
            //将剩余部分存入链表
            if(*reinterpret_cast<void**>(splitSpot))
            {
                _centralFreeList[idx].store(*reinterpret_cast<void**>(splitSpot));
            }

            *reinterpret_cast<void**>(splitSpot)=nullptr;

            _lockList[idx].clear(std::memory_order_release);
            return {ret,allowBlock};
        }
    }
    catch(const std::exception& e)
    {
        _lockList[idx].clear(std::memory_order_release);
        std::cerr << e.what() << '\n';
        return {nullptr,0};
    }
    
}

std::pair<void*,size_t>CentralCache::fetchFromPageCache(size_t idx){
    //单个内存块的大小
    size_t blockSize=(idx+1)*ALIGNMENT;
    //计算实际需要的页数
    size_t pageNum=(blockSize+PageCache::PAGESIZE-1)/PageCache::PAGESIZE;

    void* start=nullptr;
    size_t span_page=0;
    //按策略分配
    //小于32kb，固定分配32kb
    if(blockSize<=SPAN_PAGES*PageCache::PAGESIZE)
    {
        start=PageCache::getInstance().allocateSpan(SPAN_PAGES);
        span_page=SPAN_PAGES;

    }
    //大于32kb按实际需要进行分配
    else
    {
        start=PageCache::getInstance().allocateSpan(pageNum);
        span_page=pageNum;
    }

    //切割span，构建链表
    char* current=static_cast<char*>(start);
    char* prev=nullptr;
    size_t totalBlock=(span_page*PageCache::PAGESIZE)/blockSize;
    for(int i=0;i<totalBlock-1;++i)
    {   
        prev=current;
        current+=blockSize;
        *reinterpret_cast<void**>(prev)=current;
    }
    return {start,span_page};
}

void CentralCache::returnRange(void*start,size_t idx){
    //安全性检查
    if(!start||idx>=FREE_LIST_SIZE)
        return;

    //获取锁
    while(_lockList[idx].test_and_set(std::memory_order_acquire)){
        std::this_thread::yield();
    }

    try
    {
        //找到start链表的最后一个元素
        void* tail=start;
        while(*reinterpret_cast<void**>(tail))
        {
            tail=*reinterpret_cast<void**>(tail);
        }
        //进行合并
        *reinterpret_cast<void**>(tail)=_centralFreeList[idx];
        _centralFreeList[idx]=start;
    }
    catch(const std::exception& e)
    {
        _lockList[idx].clear(std::memory_order_release);
        std::cerr << e.what() << '\n';
    }
    _lockList[idx].clear(std::memory_order_release);

    return;
    
}
}