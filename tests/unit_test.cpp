#include"MemoryPool.h"
#include<iostream>
#include<atomic>
#include<random>
#include<memory>
#include<thread>
#include<vector>
#include<gtest/gtest.h>





using namespace tyMemoryPool;

std::random_device rd;
std::mt19937 mt(rd());

//基础分配测试
void testBasicAllocation()
{
    std::cout<<"Runngin basic allocation test"<<std::endl;
    //小内存测试
    void* ptr1=MemoryPool::allocate(8);
    assert(ptr1!=nullptr);
    MemoryPool::deallocate(ptr1,8);
    void* ptr2=MemoryPool::allocate(3);
    assert(ptr2!=nullptr);
    MemoryPool::deallocate(ptr2,3);
    //中等内存测试
    void* ptr3=MemoryPool::allocate(2000);
    assert(ptr3!=nullptr);
    MemoryPool::deallocate(ptr3,2000);
    //大内存测试
    void* ptr4=MemoryPool::allocate(128*1024);
    assert(ptr4!=nullptr);
    MemoryPool::deallocate(ptr4,128*1024);
    std::cout<<"Basic allocation test passed\n";

}
// 内存写入测试
void testMemoryWriting()
{
    std::cout<<"Running memory writing test\n";
    size_t size=256;
    char*ptr=static_cast<char*>(MemoryPool::allocate(size));
    for(int i=0;i<size;++i){
        ptr[i]=static_cast<char>(i);
    }
    for(int i=0;i<size;++i){
        assert(ptr[i]==static_cast<char>(i));
    }
    MemoryPool::deallocate(ptr,size);
    std::cout<<"Memory writing test passed\n";
}

//多线程测试
void mutithreadTest()
{
    std::cout<<"Running muti thread allocate test\n";
    size_t threadNum=4;
    size_t allocaPerThread=1000;
    std::atomic_bool hash_error;
    auto testFunc=[=,&hash_error]()->void{
        try
        {
            std::vector<std::pair<void*,size_t>>allocations;
            allocations.reserve(allocaPerThread);
            for(int i=0;i<allocaPerThread&&hash_error;++i){
                //随机分配
                size_t size=mt();
                void* ptr=MemoryPool::allocate(size);
                if(!ptr)
                {
                    std::cerr<<"thread allocate failed:"<<std::this_thread::get_id()<<" "<<size<<std::endl;
                    hash_error=false;
                    break;
                }

                allocations.emplace_back(std::make_pair(ptr,size));
                //随机释放内存
                if(mt()%2&&!allocations.empty())
                {
                    size_t idx=mt()%allocations.size();
                    MemoryPool::deallocate(allocations[idx].first,allocations[idx].second);
                    allocations.erase(allocations.begin()+idx);
                }
            }
            for(auto&[ptr,size]:allocations)
            {
                MemoryPool::deallocate(ptr,size);
            }
        }
        catch(const std::exception& e)
        {
            std::cerr <<"Thread exception "<<std::this_thread::get_id()<<" "<< e.what() << '\n';
        } 
    };

    std::vector<std::thread>threads;
    for(int i=0;i<threadNum;++i){
        threads.emplace_back(std::thread(testFunc));
    }
    for(auto&t:threads){
        t.join();
    }
    std::cout<<"Muti thread allocate passed\n";
}

//边界测试
void edgeAllocationTest()
{   
    //小内存边界测试
    std::cout<<"Running edge allocate test\n";
    void* ptr1=MemoryPool::allocate(0);
    assert(ptr1!=nullptr);
    MemoryPool::deallocate(ptr1,0);

    void* ptr2=MemoryPool::allocate(1);
    assert(ptr2!=nullptr);
    MemoryPool::deallocate(ptr2,1);
    //大内存边界测试
    void* ptr4=MemoryPool::allocate(256*1024);
    assert(ptr4!=nullptr);
    MemoryPool::deallocate(ptr4,256*1024);
    //过大内存测试
    void* ptr3=MemoryPool::allocate(1024*1024);
    assert(ptr3!=nullptr);
    MemoryPool::deallocate(ptr3,1024*1024);

    std::cout<<"Edge allocate test passed\n";

}

int main(){
    testBasicAllocation();
    testMemoryWriting();
    mutithreadTest();
    edgeAllocationTest();
    EXPECT_EQ(MemoryPool::allocate(7),nullptr);
    
    return 0;
}