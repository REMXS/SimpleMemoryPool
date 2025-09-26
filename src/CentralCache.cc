#include"Common.h"
#include"PageCache.h"
#include"CentralCache.h"
#include<thread>
#include<unordered_set>

namespace tyMemoryPool{
static constexpr size_t SPAN_PAGES=8;
std::pair<void*,size_t> CentralCache::fetchRange(size_t idx,size_t batchNum){
    //安全性检查
    if(idx>=FREE_LIST_SIZE||batchNum==0) 
        return {nullptr,0};
    
    //获取锁
    SpinLockGuard slg(_centralFreeList[idx].lock);
    SpanInfo* currSpan=_centralFreeList[idx].currSpan;
    void* ret=nullptr;
    try
    {
        if(currSpan)
            ret=currSpan->freeList;
        //如果中心自由链表中有内存块，则直接拿取(注意：操作的是currSpan)
        if(currSpan&&ret)
        {   
            //更新自由链表空闲span的数量
            if(currSpan->freeCount==currSpan->totalBlocks)
                _centralFreeList[idx].freeSize--;

            char* current=static_cast<char*>(ret);
            size_t currNum=0;
            char* prev=nullptr;
            while(current&&currNum<batchNum){
                currNum++;
                prev=current;
                current=static_cast<char*>(*reinterpret_cast<void**>(current));
            }
            //切断链表
            if(prev)
            {
                *reinterpret_cast<void**>(prev)=nullptr;
            }
            //更新currSpan
            _centralFreeList[idx].currSpan->freeList=static_cast<void*>(current);
            _centralFreeList[idx].currSpan->freeCount-=currNum;
            //如果空闲的内存块为空，则向前移动一个span（因为申请内存采用的是头插法）
            if(!_centralFreeList[idx].currSpan->freeCount)
                _centralFreeList[idx].currSpan=_centralFreeList[idx].currSpan->prev;

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
            //将剩余部分的链表更新至相应的span
            _centralFreeList[idx].spanList->freeList=*reinterpret_cast<void**>(splitSpot);
            _centralFreeList[idx].spanList->freeCount-=allowBlock;
            
            //切断链表
            *reinterpret_cast<void**>(splitSpot)=nullptr;

            if(!_centralFreeList[idx].spanList->freeCount)
                _centralFreeList[idx].currSpan=_centralFreeList[idx].spanList->prev;

            return {ret,allowBlock};
        }
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        return{nullptr,0};
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
    //小于32kb，固定分配32kb(8页)
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

    //切割span，构建链表和spanInfo
    char* current=static_cast<char*>(start);
    char* prev=nullptr;
    size_t totalBlock=(span_page*PageCache::PAGESIZE)/blockSize;
    for(int i=0;i<totalBlock-1;++i)
    {   
        prev=current;
        current+=blockSize;
        *reinterpret_cast<void**>(prev)=current;
    }
    //显式将链表结尾置空
    *reinterpret_cast<void**>(current)=nullptr;

    //注册span的信息
    registSpan(start,totalBlock,start,span_page,idx);

    //因为有可用内存块的span数量为0时才调用这个函数，所以要更新currSpan
    //更新当前可获取的span
    _centralFreeList[idx].currSpan=_centralFreeList[idx].spanList;

    return {start,span_page};
}

void CentralCache::registSpan(void* start,size_t totalBlocks,void* addr,size_t pageNum,size_t idx){
    SpanInfo*newSpan=new SpanInfo;
    newSpan->addr=addr;
    newSpan->freeCount=newSpan->totalBlocks=totalBlocks;
    newSpan->prev=nullptr;
    newSpan->next=_centralFreeList[idx].spanList;
    newSpan->pageNum=pageNum;

    //更新自由链表
    if(_centralFreeList[idx].spanList)
    _centralFreeList[idx].spanList->prev=newSpan;
    
    //加入中心链表（采用头插法）
    _centralFreeList[idx].spanList=newSpan;
    //加入首地址对应span的映射
    size_t midx=findShard(idx);

    SpinLockGuard mslg(_mapLocks[midx]);
    _spanRegistry[midx][addr]=newSpan;

    return;
}

void CentralCache::returnRange(void*start,size_t idx){
    //返还的链表中的内存块同属于一个sizeclass
    std::unordered_set<SpanInfo*>temSpanMap;
    //安全性检查
    if(!start||idx>=FREE_LIST_SIZE)
        return;

    //获取内存块的大小
    size_t blockSize=(idx+1)*ALIGNMENT;
    //获取锁
    SpinLockGuard slg(_centralFreeList[idx].lock);
    try
    {   
        //遍历thread cache归还的链表，寻找每个内存块的span并归还
        char* current=static_cast<char*>(start);
        char* prev=nullptr;
        while(current){
            prev=current;
            
            current=static_cast<char*>(*reinterpret_cast<void**>(current));

            //寻找内存块对应的span
            //首先寻找对应的分片
            size_t midx=findShard(idx);
            auto it=_spanRegistry[midx].upper_bound(current);
            //没有找到对应的span直接抛出异常
            if(it==_spanRegistry[midx].begin())
            {
                throw std::runtime_error("can't find span");
            }
            it--;

            //更新对应的span
            SpanInfo*span=it->second;  
            *reinterpret_cast<void**>(prev)=span->freeList;
            span->freeList=prev;
            span->freeCount++;
            temSpanMap.emplace(span);
        }

        //遍历temSpanMap，整理出完全空闲的span
        auto it=temSpanMap.begin();
        while(it!=temSpanMap.end())
        {
            if((*it)->freeCount==(*it)->totalBlocks)
            {
                _centralFreeList[idx].freeSize++;
                it++;
            }
            else
            {
                it=temSpanMap.erase(it);
            }
        }
        //查看是否需要返还内存块
        if(shouldReturnToPageCache(idx))
        {   
            //返还内存块
            for(auto&span:temSpanMap)
            {   
                void*spanAddr=span->addr;
                size_t pageNum=span->pageNum;
                //注销span
                cancelSpan(span,idx);
                //返还内存
                PageCache::getInstance().deallocateSpan(spanAddr,pageNum);
            }
            //更新空闲内存块的数量
            _centralFreeList[idx].freeSize-=temSpanMap.size();
        }
        //归还内存后，尝试更新指向当前可用span的指针
        SpanInfo*nextSpan=_centralFreeList[idx].currSpan;
        if(!nextSpan)
        {
            nextSpan=_centralFreeList[idx].spanList;
        }
        while(nextSpan&&nextSpan->next)
        {
            nextSpan=nextSpan->next;
        }
        _centralFreeList[idx].currSpan=nextSpan;

    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        return;
    }

    return;
    
}

void CentralCache::cancelSpan(SpanInfo*span,size_t idx){
    //如果span的prev为空，则说明span为对应链表的第一个节点，需要更新对应的链表
    if(!span->prev)
    {
        _centralFreeList[idx].spanList=span->next;
    }
    if(_centralFreeList[idx].currSpan==span)
    {   
        _centralFreeList[idx].currSpan=span->prev;
    }

    if(span->prev)
        span->prev->next=span->next;
    if(span->next)
        span->next->prev=span->prev;
    
    size_t midx=findShard(idx);
    //上锁
    SpinLockGuard mslg(_mapLocks[midx]);
    _spanRegistry[midx].erase(span->addr);
    delete span;
}

size_t CentralCache::findShard(size_t idx){
    return idx%NUM_SHARDS;
}

bool CentralCache::shouldReturnToPageCache(size_t idx){
    if(idx<64) return _centralFreeList[idx].freeSize>8;
    else if(idx<128) return _centralFreeList[idx].freeSize>4;
    else if(idx<256) return _centralFreeList[idx].freeSize>3;
    else if(idx<512) return _centralFreeList[idx].freeSize>2;
    else return _centralFreeList[idx].freeSize>1;
}

}