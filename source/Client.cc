
#include "vContext.h"

const uint64_t page_size = 2*1024*1024;

int main() {
    std::vector<PigeonDevice> skip_device_list;
    // std::vector<std::string> named_device_list = {"mlx5_2", "mlx5_3"};
    // std::vector<std::string> named_device_list = {"mlx5_4"};
    std::vector<PigeonDevice> named_device_list = {{"mlx5_4", "10.10.1.10"}, {"mlx5_5", "10.10.1.11"}};
    rdmanager::vContext vcontext(&skip_device_list, &named_device_list);
    void* addr = mmap(0, page_size, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS|MAP_HUGETLB, -1, 0);
    vcontext.memory_register(addr, page_size);
    rdmanager::vQP* vqp = vcontext.create_vQP_connecter("10.10.1.2", "1145");
    int* data = (int*)addr;
    // loop parse input command, each line like: read/write [rkey]
    while(1) {
        char cmd[10];
        uint32_t rkey;
        scanf("%s %u", cmd, &rkey);
        if(strcmp(cmd, "read") == 0) {
            vqp->read(addr, page_size, (void*)0x1000000, rkey);
            for(int i = 0; i < 100; i ++) {
                printf("%d ", data[i]);
            }
        } else if(strcmp(cmd, "write") == 0) {
            for(int i = 0; i < 100; i ++) {
                data[i] += i;
            }
            vqp->write(addr, page_size, (void*)0x1000000, rkey);
            for(int i = 0; i < 100; i ++) {
                data[i] = 0;
            }
        } else if(strcmp(cmd, "switch") == 0) {
            vqp->switch_card();
        }
    }


}