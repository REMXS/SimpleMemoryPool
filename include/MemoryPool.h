#pragma once
#include"ThreadCache.h"

namespace tyMemoryPool{
namespace MemoryPool{
thread_local ThreadCache tls_cache;

void*allocate(size_t size){
    return tls_cache.allocate(size);
}
void deallocate(void* addr,size_t size){
    tls_cache.deallocate(addr,size);
}

}
}
