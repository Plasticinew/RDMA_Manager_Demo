
#include "vQP.h"
#include <atomic>
#include <unistd.h>
#include <random>

const uint64_t page_size = 1024*4;

const uint64_t iter = 1000000;

const uint64_t thread_num = 4;

std::atomic<long> counter = thread_num; 

void do_mem_cache_test(rdmanager::vQP* vqp, void* addr, uint64_t remote_addr , uint32_t rkey) {
    auto start_time = TIME_NOW;
    printf("%lu\n", TIME_NOW);
    uint64_t read_items = 1024*1024*16-1;
    // for(uint64_t i = 0; i < read_items; i++){
    //     vqp->read_main(addr, page_size, (void*)(remote_addr+page_size*i), rkey);
    // }
    // printf("%lu\n", TIME_DURATION_US(start_time, TIME_NOW));
    std::random_device r;
    // std::mt19937 rand_val(r());
    std::mt19937 rand_val(0);
    for(int k = 1; k < 10; k++){
        start_time = TIME_NOW;
        for(int j = 0; j < k; j++){
            for(uint64_t i = 0; i < read_items/k; i++){
                vqp->read_main(addr, page_size, (void*)(remote_addr+page_size*(rand_val()%(read_items/k))), rkey);
            }
        }
        printf("%lu MB mem, %lu\n", iter/k*page_size/1024/1024, TIME_DURATION_US(start_time, TIME_NOW));
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
    for(int i = 0;i < thread_num;i ++){
        rdmanager::vContext* vcontext = new rdmanager::vContext(&skip_device_list, &named_device_list);
        vcontext->memory_register(addr, page_size);
        vcontext->create_RPC("10.10.1.1", "1145");
        vcontext->create_connecter("10.10.1.1", "1145");
        rdmanager::vQP* vqp = new rdmanager::vQP(vcontext);
        vqp_list[i] = vqp;
    }
    uint64_t remote_addr; uint32_t remote_rkey;
    vqp_list[0]->alloc_RPC(&remote_addr, &remote_rkey, page_size*1024*1024*16);
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
            printf("read %lu, %lu\n", remote_addr, remote_rkey);
            std::thread* read_thread = new std::thread(&do_mem_cache_test, vqp_list[0], addr, remote_addr, remote_rkey);
        } else if(strcmp(cmd, "exit") == 0) {
            return 0;
        }
    }


}