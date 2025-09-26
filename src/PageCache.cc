#include"PageCache.h"

namespace tyMemoryPool{

    
void* PageCache::allocateSpan(size_t pageNum){
    std::lock_guard<std::mutex>lock(_mtx);
    //查找空闲的span
    //查找大于等于pageNum的span
    auto it=_freeSpanList.lower_bound(pageNum);
    if(it!=_freeSpanList.end())
    {
        Span* span=it->second;
        void* ret=span->addr;
        //清除原有的映射
        _spanMap.erase(ret);

        if(span->next)
        {   
            span->next->prev=nullptr;
            it->second=span->next;
        }
        else _freeSpanList.erase(it);
        
        span->next=nullptr;
        //如果等于pageNum，则直接返回
        if(span->pageNum==pageNum)
        {   
            delete span;
            return ret;
        }
        //复用原有的span
        size_t size=span->pageNum-pageNum;
        void* newAddr=static_cast<void*>(static_cast<char*>(span->addr)+pageNum*PAGESIZE);
        registSpan(span,newAddr,size);

        _freeSpanList[size]=span;

        return ret;
    }
    else
    {   
        void* memory=getMemroyFromSystem(pageNum);
        if(!memory) return nullptr;
        return memory;
    }
}

void* PageCache::getMemroyFromSystem(size_t numPages){
    size_t size=numPages*PAGESIZE;
    void* ptr=mmap(nullptr,size,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    if(MAP_FAILED) return nullptr;

    memset(ptr,0,size);
    return ptr;
}


void PageCache::deallocateSpan(void* addr,size_t pageNum){
    std::lock_guard<std::mutex>lock(_mtx);
    //查找有没有可以合并的空闲span
    void* nextAddr=static_cast<void*>(static_cast<char*>(addr)+pageNum*PAGESIZE);
    auto it=_spanMap.find(nextAddr);
    //如果没有找到相邻的span，则重新构建一个span
    if(it==_spanMap.end())
    {
        registSpan(new Span,addr,pageNum);
    }
    //合并两个span
    else
    {
        Span*nextSpan=it->second;
        //清除原有的映射
        _spanMap.erase(it);
        if(nextSpan->prev)
            nextSpan->prev->next=nextSpan->next;
        if(nextSpan->next)
            nextSpan->next->prev=nextSpan->prev;
        if(!nextSpan->prev)
            _freeSpanList[nextSpan->pageNum]=nextSpan->next;
        //复用原有的对象
        registSpan(nextSpan,addr,pageNum+nextSpan->pageNum);
    }

}
void PageCache::registSpan(Span* span,void* addr,size_t pageNum){
    span->addr=addr;
    span->pageNum=pageNum;
    span->prev=nullptr;
    span->next=_freeSpanList[pageNum];
    if(_freeSpanList[pageNum])
            _freeSpanList[pageNum]->prev=span;
    _freeSpanList[pageNum]=span;
    _spanMap[addr]=span;
}


}