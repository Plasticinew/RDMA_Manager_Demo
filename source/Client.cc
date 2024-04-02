
#include "vContext.h"

const uint64_t page_size = 2*1024*1024;

const uint64_t iter = 1000*1000;

void do_read(rdmanager::vQP* vqp, void* addr, uint32_t rkey) {
    printf("%lu\n", TIME_NOW);
    for(uint64_t i = 0; i < iter; i++){
        vqp->read(addr, page_size, (void*)0x1000000, rkey);
        printf("%lu\n", TIME_NOW);
    }
    return;
}

void do_switch(rdmanager::vQP* vqp, void* addr, uint32_t rkey) {
    auto start_time = TIME_NOW;
    for(uint64_t i = 0; i < iter/2; i++){
        vqp->read(addr, page_size, (void*)0x1000000, rkey);
        printf("%lu\n", TIME_DURATION_US(start_time, TIME_NOW));
    }
    vqp->switch_card();
    for(uint64_t i = 0; i < iter/2; i++){
        vqp->read(addr, page_size, (void*)0x1000000, rkey);
        printf("%lu\n", TIME_DURATION_US(start_time, TIME_NOW));
    }
    return;
}

int main() {
    std::vector<PigeonDevice> skip_device_list;
    // std::vector<std::string> named_device_list = {"mlx5_2", "mlx5_3"};
    // std::vector<std::string> named_device_list = {"mlx5_4"};
    std::vector<PigeonDevice> named_device_list = {{"mlx5_1", "10.10.1.1"}, {"mlx5_3", "10.10.1.3"}};
    rdmanager::vContext vcontext(&skip_device_list, &named_device_list);
    void* addr = mmap(0, page_size, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS|MAP_HUGETLB, -1, 0);
    memset(addr, 1, page_size);
    vcontext.memory_register(addr, page_size);
    rdmanager::vQP* vqp = vcontext.create_vQP_connecter("10.10.1.4", "1145");
    int* data = (int*)addr;
    // loop parse input command, each line like: read/write [rkey]
    while(1) {
        char cmd[10];
        uint32_t rkey;
        scanf("%s %u", cmd, &rkey);
        if(strcmp(cmd, "read") == 0) {
            std::thread* read_thread = new std::thread(&do_read, vqp, addr, rkey);
            // for(int i = 0; i < 100; i ++) {
            //     printf("%d ", data[i]);
            // }
        } else if(strcmp(cmd, "write") == 0) {
            for(int i = 0; i < 100; i ++) {
                data[i] += i;
            }
            vqp->write(addr, page_size, (void*)0x1000000, rkey);
            for(int i = 0; i < 100; i ++) {
                data[i] = 0;
            }
        } else if(strcmp(cmd, "switch") == 0) {
            std::thread* read_thread = new std::thread(&do_switch, vqp, addr, rkey);
        }
    }


}