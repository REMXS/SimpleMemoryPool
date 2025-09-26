#include"MemoryPool.h"
#include<iostream>
#include<atomic>
#include<random>
#include<memory>
#include<thread>
#include<vector>
#include<chrono>

using namespace tyMemoryPool;

std::random_device rd;
std::mt19937 mt(rd());
class Timer
{

private:
std::chrono::steady_clock::time_point start;
public:
    Timer():start(std::chrono::steady_clock::now()){}
    ~Timer(){
        std::cout<<std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-start).count()<<std::endl;
    }
};

void warmup() 
{
    std::cout << "Warming up memory systems...\n";
    std::vector<std::pair<void*,size_t>> warmupPtrs;
    
    // 预热内存池
    for (int i = 0; i < 4096; ++i) 
    {
        warmupPtrs.emplace_back(std::make_pair(MemoryPool::allocate(i),i));
    }
    
    // 释放预热内存
    for (auto&[ptr,size]: warmupPtrs) 
    {
        MemoryPool::deallocate(ptr, size);  // 使用默认大小
    }
    
    std::cout << "Warmup complete.\n\n";
}

void performance_test1(){
    std::cout<<"testing memory pool"<<std::endl;
    std::vector<std::pair<void*,size_t>>ptrs;
    for(int i=0;i<1000;++i){
        ptrs.emplace_back(std::make_pair(nullptr,mt()%2048));
    }
    
    {
        Timer t;
        for(auto&[ptr,size]:ptrs){
            ptr=malloc(size);
        }
        for(auto&[ptr,size]:ptrs){
            free(ptr);
        }
        std::cout<<"system allocate time: ";
    }

    {
        Timer t;
        for(auto&[ptr,size]:ptrs){
            ptr=MemoryPool::allocate(size);
        }
        for(auto&[ptr,size]:ptrs){
            MemoryPool::deallocate(ptr,size);
        }
        std::cout<<"memorypool allocate time: ";
    }

}


void better_performance_test() {
    // 存储指针和对应分配大小
    std::vector<std::pair<void*, size_t>> ptrs(1000, {nullptr, 0});
    
    // 系统分配测试（多轮）
    {
        Timer t;
        for (int round = 0; round < 1000; round++) {
            // 随机分配和释放，模拟真实使用模式
            for (int i = 0; i < 1000; i++) {
                if (ptrs[i].first == nullptr && (rand() % 100 < 60)) { // 60%概率分配
                    size_t size = rand() % 512 + 16;
                    ptrs[i].first = malloc(size);
                    ptrs[i].second = size;
                } else if (ptrs[i].first != nullptr && (rand() % 100 < 40)) { // 40%概率释放
                    free(ptrs[i].first);
                    ptrs[i].first = nullptr;
                    ptrs[i].second = 0;
                }
            }
        }
        // 清理剩余内存
        for (auto& ptr : ptrs) {
            if (ptr.first) {
                free(ptr.first);
                ptr.first = nullptr;
            }
        }
        std::cout << "系统分配器（交错模式）: ";
    }
    
    // 清空向量，重新初始化
    std::fill(ptrs.begin(), ptrs.end(), std::make_pair(nullptr, 0));
    
    // 内存池测试（多轮）
    {
        Timer t;
        for (int round = 0; round < 1000; round++) {
            for (int i = 0; i < 1000; i++) {
                if (ptrs[i].first == nullptr && (rand() % 100 < 60)) {
                    size_t size = rand() % 512 + 16;
                    ptrs[i].first = MemoryPool::allocate(size);
                    ptrs[i].second = size;
                } else if (ptrs[i].first != nullptr && (rand() % 100 < 40)) {
                    // 使用记录的大小进行释放
                    MemoryPool::deallocate(ptrs[i].first, ptrs[i].second);
                    ptrs[i].first = nullptr;
                    ptrs[i].second = 0;
                }
            }
        }
        // 清理剩余内存
        for (auto& ptr : ptrs) {
            if (ptr.first) {
                MemoryPool::deallocate(ptr.first, ptr.second);
                ptr.first = nullptr;
            }
        }
        std::cout << "内存池（交错模式）: ";
    }
}

void multi_thread_performance_test(size_t size) {
    std::cout << "多线程性能对比测试\n";
    size_t THREAD_NUM=size;
    
    // 系统内存分配多线程测试
    {
        Timer t;
        std::vector<std::thread> threads;
        
        for (int i = 0; i < THREAD_NUM; ++i) {
            threads.emplace_back([](){
                thread_local std::vector<std::pair<void*, size_t>> ptrs(1000, {nullptr, 0});
                thread_local std::mt19937 rng(std::random_device{}());
                
                for (int round = 0; round < 1000; round++) {
                    for (int i = 0; i < 1000; i++) {
                        if (ptrs[i].first == nullptr && (rng() % 100 < 60)) {
                            size_t size = rng() % 512 + 16;
                            ptrs[i].first = malloc(size);
                            ptrs[i].second = size;
                        } else if (ptrs[i].first != nullptr && (rng() % 100 < 40)) {
                            free(ptrs[i].first);
                            ptrs[i].first = nullptr;
                        }
                    }
                }
                
                // 清理内存
                for (auto& ptr : ptrs) {
                    if (ptr.first) {
                        free(ptr.first);
                        ptr.first = nullptr;
                    }
                }
            });
        }
        
        for (auto& t : threads) {
            t.join();
        }
        
        std::cout << "系统分配器 " <<THREAD_NUM << "线程 : ";
    }
    
    // 内存池多线程测试
    {
        Timer t;
        std::vector<std::thread> threads;
        
        for (int i = 0; i < THREAD_NUM; ++i) {
            threads.emplace_back([](){
                thread_local std::vector<std::pair<void*, size_t>> ptrs(1000, {nullptr, 0});
                thread_local std::mt19937 rng(std::random_device{}());
                for (int round = 0; round < 1000; round++) {
                    for (int i = 0; i < 1000; i++) {
                        if (ptrs[i].first == nullptr && (rng() % 100 < 60)) {
                            size_t size = rng() % 512 + 16;
                            ptrs[i].first = MemoryPool::allocate(size);
                            ptrs[i].second = size;
                        } else if (ptrs[i].first != nullptr && (rng() % 100 < 40)) {
                            MemoryPool::deallocate(ptrs[i].first, ptrs[i].second);
                            ptrs[i].first = nullptr;
                        }
                    }
                }
                
                // 清理内存
                for (auto& ptr : ptrs) {
                    if (ptr.first) {
                        MemoryPool::deallocate(ptr.first, ptr.second);
                        ptr.first = nullptr;
                    }
                }
            });
        }
        
        for (auto& t : threads) {
            t.join();
        }
        
        std::cout << "内存池 "<<THREAD_NUM<<"线程 : ";
    }
}

int main(){
    warmup();
    performance_test1();
    better_performance_test();
    multi_thread_performance_test(16);
    return 0;
}