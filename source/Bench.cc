
#include "vQP.h"    // vQP相关功能
#include <atomic>   // 原子操作支持
#include <unistd.h> // usleep
#include <random>   // 随机数生成

// 用于测试片上竞争的bench client

const uint64_t page_size = 1024*4;

// 读写的次数
const uint64_t iter = 1000000;

// 线程数
uint64_t thread_num = 4;

std::atomic<long> counter = thread_num; 

// 远程MR起始地址信息
uint64_t remote_addr[4096]; uint32_t rkey[4096];

pthread_barrier_t barrier_start;

int global_time[4096];

/**
 * 测试网卡内存缓存性能（如页表缓存命中率）
 * @param vqp vqp数组
 * @param addr 本地内存地址
 * @param thread_id 当前线程ID
 */

void do_mem_cache_test(rdmanager::vQP** vqp, void* addr, int thread_id) {
    // 仅线程0分配大块远程内存
    if(thread_id == 0)
        vqp[0]->alloc_RPC(&remote_addr[0], &rkey[0], page_size*1024*1024*16);
    auto start_time = TIME_NOW;
    printf("%lu\n", TIME_NOW);
    uint64_t read_items = 1024*1024;    // 每个线程执行1M次读操作
    std::random_device r;   // 真随机数生成器
    std::mt19937 rand_val(r());

    // 测试不同内存范围（从1GB到64GB）的访问时间
    for(int j = 1; j < 65; j++){
        pthread_barrier_wait(&barrier_start);   // 线程同步
        start_time = TIME_NOW;

        // 执行随机页访问，测试缓存效果
        for(uint64_t i = 0; i < read_items; i++){
            vqp[thread_id]->read_main(addr, page_size, (void*)(remote_addr[0]+page_size*(rand_val()%(j*128*1024))), rkey[0]);
        }

        // 统计各线程耗时
        global_time[thread_id] = TIME_DURATION_US(start_time, TIME_NOW);
        double total_time = 0;
        for(int i = 0; i < thread_num; i++){
            total_time += global_time[i]/read_items;
        }

        // 主线程打印平均延迟
        if(thread_id == 0)
            printf("%lu GB mem, %lf\n", j,total_time/thread_num);
    }
    return;
}

/**
 * 测试内存区域（MR）缓存性能
 * @param vqp vqp数组 
 * @param addr 本地内存地址
 * @param thread_id 当前线程ID
 */

void do_mr_cache_test(rdmanager::vQP** vqp, void* addr, int thread_id) {
    auto start_time = TIME_NOW;
    auto start_time_global = TIME_NOW;
    printf("%lu\n", TIME_NOW);
    uint64_t read_items = 1024*1024;
    uint64_t mr_num = 4096;
    std::random_device r;
    std::mt19937 rand_val(r());

    // 分批次创建MR并测试
    for(int i = 0; i < mr_num/64; i ++){
        // 每批创建64个MR
        if(thread_id == 0) {
            for(int j = i * 64; j < (i+1) * 64; j++){
                vqp[0]->alloc_RPC(&remote_addr[j], &rkey[j], page_size*1024*4);
            }
        }
        int index = 0;
        int counter[1000];
        memset(counter, 0, sizeof(int)*1000);
        pthread_barrier_wait(&barrier_start);
        start_time_global = TIME_NOW;

        // 随机访问累计创建的MR
        for(uint64_t j = 0; j < read_items; j++){
            index = rand_val()%((i+1) * 64);
            start_time = TIME_NOW;
            vqp[thread_id]->read_main(addr, page_size, (void*)(remote_addr[index] + page_size*(rand_val()%4096)), rkey[index]);
        }
        global_time[thread_id] = TIME_DURATION_US(start_time, TIME_NOW);
        double total_time = 0;
        for(int i = 0; i < thread_num; i++){
            total_time += global_time[i]/read_items;
        }
        if(thread_id == 0)
            printf("%lu GB mem, %lf\n", (i+1) * 64 , total_time/thread_num);
    }
    return;
}

/**
 * 测试队列对（QP）缓存性能
 * @param vqp vqp数组
 * @param addr 本地内存地址
 * @param thread_id 当前线程ID
 */

void do_qp_cache_test(rdmanager::vQP** vqp, void* addr, int thread_id) {
    if(thread_id ==0)
        vqp[0]->alloc_RPC(&remote_addr[0], &rkey[0], page_size*1024*1024*16);
    auto start_time = TIME_NOW;
    auto start_time_global = TIME_NOW;
    printf("%lu\n", TIME_NOW);
    uint64_t read_items = 1024*1024;
    uint64_t mr_num = 4096;
    std::random_device r;
    std::mt19937 rand_val(r());
    int index = 0;
    int counter[1000];
    memset(counter, 0, sizeof(int)*1000);
    pthread_barrier_wait(&barrier_start);
    start_time_global = TIME_NOW;
    for(int i = 0; i < 64; i ++){
        int index = 0;
        int counter[1000];
        memset(counter, 0, sizeof(int)*1000);
        pthread_barrier_wait(&barrier_start);
        start_time_global = TIME_NOW;
        for(uint64_t j = 0; j < read_items; j++){
            index = rand_val()%((i+1) * 64);
            start_time = TIME_NOW;
            vqp[index]->read_main(addr, page_size, (void*)(remote_addr[0] + page_size*(rand_val()%(4096*4096))), rkey[0]);
            // printf("%lu\n", TIME_DURATION_US(start_time, TIME_NOW));
        }

        global_time[thread_id] = TIME_DURATION_US(start_time_global, TIME_NOW);
        double total_time = 0;
        for(int k = 0; k < thread_num; k++){
            total_time += global_time[k]/read_items;
        }
        if(thread_id == 0)
            printf("%lu QP, %lu\n", (i+1) * 64 , total_time/thread_num);    
    }
    return;
}

void do_read(rdmanager::vQP* vqp, void* addr, uint32_t rkey, uint32_t lid, uint32_t dct_num) {
    auto start_time = TIME_NOW;
    printf("%lu\n", TIME_NOW);
    for(uint64_t i = 0; i < iter; i++){
        vqp->read_main(addr, page_size, (void*)0x1000000, rkey);
    }
    printf("%lu\n", TIME_DURATION_US(start_time, TIME_NOW));
    start_time = TIME_NOW;
    // printf("%lu\n", TIME_NOW);
    for(uint64_t i = 0; i < iter; i++){
        vqp->read_backup(addr, page_size, (void*)0x1000000, rkey, lid, dct_num);
        // vqp->read(addr, page_size, (void*)0x1000000, rkey);
    }
    printf("%lu\n", TIME_DURATION_US(start_time, TIME_NOW));
    return;
}

void do_switch(rdmanager::vQP* vqp, void* addr, uint32_t rkey, uint32_t lid, uint32_t dct_num) {
    auto start_time = TIME_NOW;
    auto total_start_time = TIME_NOW;
    counter.fetch_sub(1);
    while(counter.load()!=0);
    for(uint64_t i = 0; i < iter/2; i++){
        start_time = TIME_NOW;
        vqp->read(addr, page_size, (void*)0x1000000, rkey, lid, dct_num);
        counter.fetch_add(1);
        // printf("%lu\n", TIME_DURATION_US(start_time, TIME_NOW));
    }
    // printf("%lu\n", TIME_DURATION_US(total_start_time, TIME_NOW)/(iter/2));
    vqp->switch_card();
    total_start_time = TIME_NOW;
    for(uint64_t i = 0; i < iter/2; i++){
        start_time = TIME_NOW;
        vqp->read(addr, page_size, (void*)0x1000000, rkey, lid, dct_num);
        counter.fetch_add(1);
        // printf("%lu\n", TIME_DURATION_US(start_time, TIME_NOW));
    }
    // printf("%lu\n", TIME_DURATION_US(total_start_time, TIME_NOW)/(iter/2));
    return;
}

/**
 * bench 参数
 * @param argc 参数个数
 * @param argv 参数数组 [程序名, 测试类型, 线程数]
 */
int main(int argc, char* argv[]) {
    
    if(argc < 3){
        printf("Usage: %s <bench type> <thread number>", argv[0]);
        return 0;
    }

    std::string bench_type = argv[1];
    thread_num = atoi(argv[2]);
    
    pthread_barrier_init(&barrier_start, NULL, thread_num);

    std::vector<PigeonDevice> skip_device_list;
    // std::vector<PigeonDevice> named_device_list = {{"mlx5_4", "10.10.1.14"}, {"mlx5_7", "10.10.1.17"}};
    std::vector<PigeonDevice> named_device_list = {{"mlx5_2", "10.10.1.2"}};
    // std::vector<PigeonDevice> named_device_list = {{"mlx5_7", "10.10.1.13"}, {"mlx5_4", "10.10.1.10"}};
    // std::vector<PigeonDevice> named_device_list = {{"mlx5_2", "10.10.1.1"}};
    // 分配大页内存
    void* addr = mmap(0, page_size, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS|MAP_HUGETLB, -1, 0);
    memset(addr, 0, page_size);
    
    // 创建vQP实例
    rdmanager::vQP* vqp_list[thread_num];
    rdmanager::vQP* vqp_cache[4096];
    for(int i = 0;i < thread_num;i ++){
        // 创建虚拟上下文，注册内存，建立连接
        rdmanager::vContext* vcontext = new rdmanager::vContext(&skip_device_list, &named_device_list);
        vcontext->memory_register(addr, page_size);
        vcontext->create_RPC("10.10.1.1", "1145");
        vcontext->create_connecter("10.10.1.1", "1145");
        rdmanager::vQP* vqp = new rdmanager::vQP(vcontext);
        vqp_list[i] = vqp;
    }
    
    // create vQP cache for test
    for(int i = 0; i < 4096; i ++) {
        rdmanager::vContext* vcontext = new rdmanager::vContext(&skip_device_list, &named_device_list);
        vcontext->memory_register(addr, page_size);
        if(i == 0 ){
            vcontext->create_RPC("10.10.1.1", "1145");
        }
        vcontext->create_connecter("10.10.1.1", "1145");
        rdmanager::vQP* vqp = new rdmanager::vQP(vcontext);
        vqp_cache[i] = vqp;
        printf("%d success\n", i);
    }

    // vcontext->memory_bind(addr, page_size);
    int* data = (int*)addr;
    // loop parse input command, each line like: read/write [rkey]
    uint32_t rkey;
    uint32_t lid;
    uint32_t dct;
    scanf("%s %u %u %u", bench_type, &rkey, &lid, &dct);
    if(bench_type == "read") {
        for(int i = 0;i < thread_num; i++){
            std::thread* read_thread = new std::thread(&do_read, vqp_list[i], addr, rkey, lid, dct);
        }
    } else if(bench_type == "write") {
        for(int i = 0; i < 100; i ++) {
            data[i] += i;
        }
        vqp_list[0]->write_backup(addr, page_size, (void*)0x1000000, rkey, lid, dct);
        for(int i = 0; i < 100; i ++) {
            data[i] = 0;
        }
    } else if(bench_type == "switch") {
        for(int i = 0;i < thread_num; i++)
            std::thread* read_thread = new std::thread(&do_switch, vqp_list[i], addr, rkey, lid, dct);
        long old_val, new_val;
        while(counter.load() < thread_num * iter -1){
            old_val = counter.load();
            usleep(1000);
            new_val = counter.load();
            printf("%lf\n", 1.0*(new_val-old_val)*1000*page_size);
        }
    } else if(bench_type == "cache") {
        for(int i = 0;i < thread_num; i++){
            std::thread* read_thread = new std::thread(&do_mem_cache_test, vqp_cache, addr, i);
        }
    } else if(bench_type == "mr") {
        for(int i = 0;i < thread_num; i++){
            std::thread* read_thread = new std::thread(&do_mr_cache_test, vqp_cache, addr, i);
        }
    } else if(bench_type == "qp") {
        for(int i = 0;i < thread_num; i++){
            std::thread* read_thread = new std::thread(&do_qp_cache_test, vqp_cache, addr, i);
        }
    } else if(bench_type == "exit") {
        return 0;
    }
    return 0;
}