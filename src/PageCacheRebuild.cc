#include"PageCacheRebuild.h"

namespace tyMemoryPool{



void* PageCacheRe::allocateSpan(size_t pageNum){
    std::lock_guard<std::mutex>lock(_mtx);
    //查找空闲的span
    //查找大于等于pageNum的span
    auto it=_freeSpanList.lower_bound(pageNum);
    if(it!=_freeSpanList.end())
    {   
        void* span=it->second.spanList;
        it->second.size--;
        void* ret=span;
        //清除原有的映射
        _spanMap.erase(ret);
        
        //如果next非空，更新链表
        void* nextSpan=getSpanControl(span)->next;
        if(nextSpan)
        {   
            getSpanControl(span)->prev=nullptr;
            it->second.spanList=nextSpan;
        }
        else _freeSpanList.erase(it);
        
        //如果等于pageNum，则直接返回
        if(getSpanControl(span)->pageNum==pageNum)
        {   
            return ret;
        }
        //在新地址处构建新的span
        size_t size=getSpanControl(span)->pageNum-pageNum;
        void* newAddr=static_cast<void*>(static_cast<char*>(getSpanControl(span)->addr)+pageNum*PAGESIZE);
        registSpan(newAddr,newAddr,size);

        return ret;
    }
    else
    {   
        void* memory=getMemroyFromSystem(pageNum);
        if(!memory) return nullptr;
        return memory;
    }
}

void* PageCacheRe::getMemroyFromSystem(size_t numPages){
    size_t size=numPages*PAGESIZE;
    void* ptr=mmap(nullptr,size,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    if(ptr==MAP_FAILED) return nullptr;

    memset(ptr,0,size);
    return ptr;
}


void PageCacheRe::deallocateSpan(void* addr,size_t pageNum){
    //安全性检查
    if(!addr) return;

    std::lock_guard<std::mutex>lock(_mtx);
    size_t newPageNum=pageNum;
    //查找有没有可以合并的空闲span
    void* nextAddr=static_cast<void*>(static_cast<char*>(addr)+pageNum*PAGESIZE);
    auto it=_spanMap.find(nextAddr);
    //如果没有找到相邻的span，则重新构建一个span
    if(it==_spanMap.end())
    {
        registSpan(addr,addr,pageNum);
    }
    //合并两个span
    else
    {   
        //有映射就说明有空闲的span
        void*nextSpan=*it;
        newPageNum=pageNum+getSpanControl(nextSpan)->pageNum;
        //清除原有的映射
        _spanMap.erase(it);

        //将nextSpan从原有的链表中擦除
        void* prev = getSpanControl(nextSpan)->prev;
        void* next = getSpanControl(nextSpan)->next;

        if (prev) 
            getSpanControl(prev)->next = next;

        if (next)
            getSpanControl(next)->prev = prev;
        if(!prev)
        {
            _freeSpanList[getSpanControl(nextSpan)->pageNum].spanList=next;
            _freeSpanList[getSpanControl(nextSpan)->pageNum].size--;
        }

        registSpan(addr,addr,newPageNum);
    }
    //检查是否需要释放内存
    if(shouldReturnMemory(newPageNum))
    {
        //返还内存
        returnMemroyToSystem(newPageNum);
    }

}
bool PageCacheRe::shouldReturnMemory(size_t pageNum){
    if(pageNum<16) return _freeSpanList[pageNum].size>8;
    else if(pageNum<32) return _freeSpanList[pageNum].size>4;
    else if(pageNum<64) return _freeSpanList[pageNum].size>2;
    else return _freeSpanList[pageNum].size>1;
}

void PageCacheRe::returnMemroyToSystem(size_t pageNum){
    //安全性检查
    if(_freeSpanList.find(pageNum)==_freeSpanList.end()||!_freeSpanList[pageNum].size) return;

    //保留1/4的span，至少保留一个span
    size_t keepNum=std::max(_freeSpanList[pageNum].size/4,size_t(1));
    //应该返回的span数量
    size_t returnNum=_freeSpanList[pageNum].size-keepNum;

    void* prev=nullptr;
    void* current=_freeSpanList[pageNum].spanList;
    size_t currentNum=0;
    //遍历链表
    while(current&&currentNum<returnNum)
    {   
        prev=current;
        current=getSpanControl(current)->next;
        //清除原有的映射
        _spanMap.erase(prev);
        //清理内存
        munmap(prev,pageNum*PAGESIZE);
        currentNum++;
    }
    //更新数量
    _freeSpanList[pageNum].size-=currentNum;
    //如果current为nullptr，则说明链表中没有元素，删除链表
    if(!current)
    {
        _freeSpanList.erase(pageNum);
    }
    else
    {   
        //如果current不为nullptr，更新对应的spanList和映射
        _freeSpanList[pageNum].spanList=current;
    }
    
}

void PageCacheRe::registSpan(void* span,void* addr,size_t pageNum){
    //更新数量
    _freeSpanList[pageNum].size++;
    
    auto&[prev,spanAddr,pages,next]=*getSpanControl(span);
    spanAddr=addr;
    pages=pageNum;
    prev=nullptr;
    next=_freeSpanList[pageNum].spanList;
    //更新头节点的前驱
    if(_freeSpanList[pageNum].spanList)
        getSpanControl(_freeSpanList[pageNum].spanList)->prev=span;
    _freeSpanList[pageNum].spanList=span;
    _spanMap.emplace(span);
}



}