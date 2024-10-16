
#include "vQP.h"

const uint64_t page_size = 2*1024;

const uint64_t iter = 1000;

void do_read(rdmanager::vQP* vqp, void* addr, uint32_t rkey, uint32_t lid, uint32_t dct_num) {
    auto start_time = TIME_NOW;
    // printf("%lu\n", TIME_NOW);
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
    for(uint64_t i = 0; i < iter/2; i++){
        vqp->read_backup(addr, page_size, (void*)0x1000000, rkey, lid, dct_num);
        printf("%lu\n", TIME_DURATION_US(start_time, TIME_NOW));
    }
    vqp->switch_card();
    for(uint64_t i = 0; i < iter/2; i++){
        vqp->read_backup(addr, page_size, (void*)0x1000000, rkey, lid, dct_num);
        printf("%lu\n", TIME_DURATION_US(start_time, TIME_NOW));
    }
    return;
}

int main() {
    std::vector<PigeonDevice> skip_device_list;
    // std::vector<std::string> named_device_list = {"mlx5_2", "mlx5_3"};
    // std::vector<std::string> named_device_list = {"mlx5_4"};
    std::vector<PigeonDevice> named_device_list = {{"mlx5_4", "10.10.1.10"}, {"mlx5_5", "10.10.1.11"}};
    // std::vector<PigeonDevice> named_device_list = {{"mlx5_7", "10.10.1.13"}, {"mlx5_4", "10.10.1.10"}};
    // std::vector<PigeonDevice> named_device_list = {{"mlx5_2", "10.10.1.1"}};
    void* addr = mmap(0, page_size, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS|MAP_HUGETLB, -1, 0);
    memset(addr, 0, page_size);
    rdmanager::vContext* vcontext = new rdmanager::vContext(&skip_device_list, &named_device_list);
    vcontext->memory_register(addr, page_size);
    vcontext->create_connecter("10.10.1.2", "1145");
    rdmanager::vQP* vqp = new rdmanager::vQP(vcontext);
    vcontext->memory_bind(addr, page_size);
    int* data = (int*)addr;
    // loop parse input command, each line like: read/write [rkey]
    while(1) {
        char cmd[10];
        uint32_t rkey;
        uint32_t lid;
        uint32_t dct;
        scanf("%s %u %u %u", cmd, &rkey, &lid, &dct);
        if(strcmp(cmd, "read") == 0) {
            std::thread* read_thread = new std::thread(&do_read, vqp, addr, rkey, lid, dct);
        } else if(strcmp(cmd, "write") == 0) {
            for(int i = 0; i < 100; i ++) {
                data[i] += i;
            }
            vqp->write_backup(addr, page_size, (void*)0x1000000, rkey, lid, dct);
            for(int i = 0; i < 100; i ++) {
                data[i] = 0;
            }
        } else if(strcmp(cmd, "switch") == 0) {
            std::thread* read_thread = new std::thread(&do_switch, vqp, addr, rkey, lid, dct);
        }
    }


}