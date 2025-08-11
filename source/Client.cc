
#include "vQP.h"
#include <atomic>
#include <unistd.h>
#include <random>

using std::vector;
using rdmanager::PigeonDevice;
using rdmanager::vContext;
using rdmanager::vQP;

const uint64_t page_size = 1024*4;

const uint64_t iter = 1000000;

const uint64_t thread_num = 4;

std::atomic<long> counter = thread_num; 

void do_mem_cache_test(rdmanager::vQP* vqp, void* addr) {
    uint64_t remote_addr; uint32_t rkey;
    vqp->alloc_RPC(&remote_addr, &rkey, page_size*1024*1024*16);
    auto start_time = TIME_NOW;
    printf("%lu\n", TIME_NOW);
    uint64_t read_items = 1024*1024;
    std::random_device r;
    std::mt19937 rand_val(r());
    for(int j = 1; j < 65; j++){
        start_time = TIME_NOW;
        for(uint64_t i = 0; i < read_items; i++){
            vqp->read_main(addr, page_size, (void*)(remote_addr+page_size*(rand_val()%(j*128*1024))), rkey);
        }
        printf("%lu GB mem, %lu\n", j, TIME_DURATION_US(start_time, TIME_NOW));
    }
    return;
}

void do_mr_cache_test(rdmanager::vQP* vqp, void* addr) {
    auto start_time = TIME_NOW;
    auto start_time_global = TIME_NOW;
    printf("%lu\n", TIME_NOW);
    uint64_t read_items = 1024*1024;
    uint64_t mr_num = 4096;
    std::random_device r;
    std::mt19937 rand_val(r());
    uint64_t remote_addr[mr_num]; uint32_t rkey[mr_num];
    for(int i = 0; i < mr_num/64; i ++){
        for(int j = i * 64; j < (i+1) * 64; j++){
            vqp->alloc_RPC(&remote_addr[j], &rkey[j], page_size*1024*4);
        }
        int index = 0;
        int counter[1000];
        memset(counter, 0, sizeof(int)*1000);
        start_time_global = TIME_NOW;
        for(uint64_t j = 0; j < read_items; j++){
            index = rand_val()%((i+1) * 64);
            start_time = TIME_NOW;
            vqp->read_main(addr, page_size, (void*)(remote_addr[index] + page_size*(rand_val()%4096)), rkey[index]);
            // printf("%lu\n", TIME_DURATION_US(start_time, TIME_NOW));
            uint64_t time = TIME_DURATION_US(start_time, TIME_NOW);
            time = time >= 1000 ? 1000 : time;
            counter[time] += 1;
        }
        if(i == 63){
            for(uint64_t i = 0; i < 1000; i ++){
                if(counter[i] != 0){
                    printf("%lu, %d\n", i, counter[i]);
                }
            }
        }
        printf("%lu MR, %lu\n", (i+1) * 64 , TIME_DURATION_US(start_time_global, TIME_NOW));
    }
    return;
}

void do_qp_cache_test(rdmanager::vQP** vqp, void* addr) {
    uint64_t remote_addr; uint32_t rkey;
    vqp[0]->alloc_RPC(&remote_addr, &rkey, page_size*1024*1024*16);
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
    start_time_global = TIME_NOW;
    for(uint64_t j = 0; j < read_items; j++){
        index = 0;
        start_time = TIME_NOW;
        vqp[index]->read_main(addr, page_size, (void*)(remote_addr + page_size*(rand_val()%(4096*4096))), rkey);
        // printf("%lu\n", TIME_DURATION_US(start_time, TIME_NOW));
        uint64_t time = TIME_DURATION_US(start_time, TIME_NOW);
        time = time >= 1000 ? 1000 : time;
        counter[time] += 1;
    }
    for(uint64_t i = 0; i < 1000; i ++){
        if(counter[i] != 0){
            printf("%lu, %d\n", i, counter[i]);
        }
    }
    printf("%lu QP, %lu\n", 1, TIME_DURATION_US(start_time_global, TIME_NOW));

    for(int i = 0; i < 64; i ++){
        int index = 0;
        int counter[1000];
        memset(counter, 0, sizeof(int)*1000);
        start_time_global = TIME_NOW;
        for(uint64_t j = 0; j < read_items; j++){
            index = rand_val()%((i+1) * 64);
            start_time = TIME_NOW;
            vqp[index]->read_main(addr, page_size, (void*)(remote_addr + page_size*(rand_val()%(4096*4096))), rkey);
            // printf("%lu\n", TIME_DURATION_US(start_time, TIME_NOW));
            uint64_t time = TIME_DURATION_US(start_time, TIME_NOW);
            time = time >= 1000 ? 1000 : time;
            counter[time] += 1;
        }
        if(i == 63){
            for(uint64_t j = 0; j < 1000; j ++){
                if(counter[j] != 0){
                    printf("%lu, %d\n", j, counter[j]);
                }
            }
        }
        printf("%lu QP, %lu\n", (i+1) * 64 , TIME_DURATION_US(start_time_global, TIME_NOW));
    }
    return;
}

void do_read(rdmanager::vQP* vqp, void* addr, uint32_t rkey, uint32_t lid, uint32_t dct_num) {
    auto start_time = TIME_NOW;
    printf("%lu\n", TIME_NOW);
    for(uint64_t i = 0; i < iter; i++){
        vqp->read_main(addr, page_size, (void*)0x1000000, rkey);
        // vqp->read_backup(addr, page_size, (void*)0x1000000, rkey, lid, dct_num);
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

int main() {
    std::vector<PigeonDevice> skip_device_list;
    // std::vector<PigeonDevice> named_device_list = {{"mlx5_4", "10.10.1.14"}, {"mlx5_7", "10.10.1.17"}};
    std::vector<PigeonDevice> named_device_list = {{"mlx5_2", "10.10.1.2"}};
    // std::vector<PigeonDevice> named_device_list = {{"mlx5_7", "10.10.1.13"}, {"mlx5_4", "10.10.1.10"}};
    // std::vector<PigeonDevice> named_device_list = {{"mlx5_2", "10.10.1.1"}};
    void* addr = mmap(0, page_size, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS|MAP_HUGETLB, -1, 0);
    memset(addr, 0, page_size);
    rdmanager::vQP* vqp_list[thread_num];
    rdmanager::vQP* vqp_cache[4096];
    int enode = -1;
    for(int i = 0;i < thread_num;i ++){
        rdmanager::vContext* vcontext = new rdmanager::vContext(&skip_device_list, &named_device_list);
        vcontext->memory_register(addr, page_size);
        vcontext->create_RPC("10.10.1.1", "1145");
        vcontext->create_connecter("10.10.1.1", "1145");
        rdmanager::vQP* vqp = new rdmanager::vQP(vcontext, &enode);
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
        rdmanager::vQP* vqp = new rdmanager::vQP(vcontext, &enode);
        vqp_cache[i] = vqp;
        printf("%d success\n", i);
    }

    // vcontext->memory_bind(addr, page_size);
    int* data = (int*)addr;
    // loop parse input command, each line like: read/write [rkey]
    while(1) {
        char cmd[10];
        uint32_t rkey;
        uint32_t lid;
        uint32_t dct;
        scanf("%s %u %u %u", cmd, &rkey, &lid, &dct);
        if(strcmp(cmd, "read") == 0) {
            for(int i = 0;i < thread_num; i++){
                std::thread* read_thread = new std::thread(&do_read, vqp_list[i], addr, rkey, lid, dct);
            }
        } else if(strcmp(cmd, "write") == 0) {
            for(int i = 0; i < 100; i ++) {
                data[i] += i;
            }
            vqp_list[0]->write_backup(addr, page_size, (void*)0x1000000, rkey, lid, dct);
            for(int i = 0; i < 100; i ++) {
                data[i] = 0;
            }
        } else if(strcmp(cmd, "switch") == 0) {
            for(int i = 0;i < thread_num; i++)
                std::thread* read_thread = new std::thread(&do_switch, vqp_list[i], addr, rkey, lid, dct);
            long old_val, new_val;
            while(counter.load() < thread_num * iter -1){
                old_val = counter.load();
                usleep(1000);
                new_val = counter.load();
                printf("%lf\n", 1.0*(new_val-old_val)*1000*page_size);
            }
        } else if(strcmp(cmd, "cache") == 0) {
            // printf("read %lu, %lu\n", remote_addr, remote_rkey);
            std::thread* read_thread = new std::thread(&do_mem_cache_test, vqp_list[0], addr);
        
        } else if(strcmp(cmd, "mr") == 0) {

            std::thread* read_thread = new std::thread(&do_mr_cache_test, vqp_list[0], addr);
        
        } else if(strcmp(cmd, "qp") == 0) {

            std::thread* read_thread = new std::thread(&do_qp_cache_test, vqp_cache, addr);

        }else if(strcmp(cmd, "exit") == 0) {
            return 0;
        }
    }


}