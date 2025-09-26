#include"ThreadCache.h"
#include"CentralCacheRebuild.h"
namespace tyMemoryPool
{

void* ThreadCache::allocate(size_t size){
    //大于MAX_BYTES的内存由系统分配
    if(size>MAX_BYTES) return malloc(size);
    if(size==0) size=ALIGNMENT;

    //获取内存对应的下标
    size_t idx=SizeClass::getIndex(size);
    void* ret=_freeList[idx];
    //如果有缓存，直接返回
    if(ret)
    {
        _freeList[idx]=*reinterpret_cast<void**>(ret);
        _listSize[idx]--;
        return ret;
    }
    //对应大小的链表没有缓存的情况
    ret=fetchFromCentralCache(idx);
    _listSize[idx]--;
    return ret;
}

void* ThreadCache::fetchFromCentralCache(size_t idx){
    if(idx>=FREE_LIST_SIZE) return nullptr;
    //获取下标对应的内存块的大小
    size_t blockSize=(idx+1)*ALIGNMENT;
    size_t batchNum=getBatchNum(blockSize);

    //从中心缓存中取batchNum个内存块，但是实际数量以返回值为准
    auto[start,realNum]=CentralCacheRe::getInstance().fetchRange(idx,batchNum);

    if(!start) return nullptr;

    //更新数量
    _listSize[idx]+=realNum;

    //取一个内存块返回给上级函数
    void *ret=start;
    //如果有多的内存块，则存入自由链表
    if(batchNum>1)
    {
        _freeList[idx]=*reinterpret_cast<void**>(ret);
        //*reinterpret_cast<void**>(ret)=nullptr;
    }

    return ret;
}

size_t ThreadCache::getBatchNum(size_t size){
    //默认获取4k内存对应的内存块
    //小内存则获取2k内存对应的内存块
    static constexpr size_t BATCHSIZE=4096;
    size_t batchsize=0;
    if(size<=32) batchsize=64;
    else if(size<=64) batchsize=32;
    else if(size<=128) batchsize=16;
    else if(size<=256) batchsize=8;
    else if(size<=512) batchsize=4;
    else if(size<=1024) batchsize=2;

    //至少为1
    size_t maxBatch=std::max((size_t)1,4096/size);

    //内存小则返回bathcsize，内存大则返回1
    return std::max(std::min(batchsize,maxBatch),(size_t)1);
}



void ThreadCache::deallocate(void*addr,size_t size){
    //内存大于MAX_BYTES则说明是系统分配的内存，由系统释放
    if(size>MAX_BYTES)
    {
        free(addr);
        return;
    }
    //获取内存大小对应的下标
    size_t idx=SizeClass::getIndex(size);
    //插入到线程本地的自由链表
    *reinterpret_cast<void**>(addr)=_freeList[idx];
    _freeList[idx]=addr;
    _listSize[idx]++;
    //检查是否需要向中心内存返还内存块
    if(shouldReturnToCentralCache(idx))
    {
        returnToCentralCache(idx);
    }
    return;
}

bool ThreadCache::shouldReturnToCentralCache(size_t idx){
    return _listSize[idx]>THRESHOLD;
}

void ThreadCache::returnToCentralCache(size_t idx){
    //安全性检查
    if(idx>=FREE_LIST_SIZE) return;
    
    //保留一部分在线程缓存的链表
    size_t batchNum=_listSize[idx];
    size_t keepNum=std::max(batchNum/4,size_t(1));//至少保留一个内存块
    size_t returnNum=batchNum-keepNum;

    //检测如果要返还的内存块为0，则不用返还，直接返回
    if(!returnNum) return;

    //将下标为idx的链表进行分割，一部分保留，一部分返还
    void* start=_freeList[idx];
    char* current=static_cast<char*>(start);
    for(size_t i=0;i<keepNum-1;++i)
    {
        current=static_cast<char*>(*reinterpret_cast<void**>(current));
        //自由链表意外结束
        if(!current)
        {
            std::cerr<<"list end unexpectedly:ThreadCache index: "<<idx<<std::endl;
            //更新链表大小
            _listSize[idx]=i+1;
            return;
        }
    }
    if(current)
    {
        void* retStart=*reinterpret_cast<void**>(current);
        CentralCacheRe::getInstance().returnRange(retStart,idx);

        *reinterpret_cast<void**>(current)=nullptr;
        _freeList[idx]=start;
        _listSize[idx]=keepNum;
    }

    
    return;
}

ThreadCache::~ThreadCache(){
    //向中心缓存返还所有内存块
    for(size_t i;i<FREE_LIST_SIZE;++i){
        if(_freeList[i])
            CentralCacheRe::getInstance().returnRange(_freeList[i],i);
    }
}

}
