
#include "vContext.h"

const uint64_t size = (uint64_t)2*1024*1024*1024;
const uint64_t addr = 0x1000000;

int main() {
    std::vector<std::string> skip_device_list;
    std::vector<std::string> named_device_list = {"mlx5_2"};
    rdmanager::vContext vcontext(&skip_device_list, &named_device_list);
    mmap((void*)addr, size, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED|MAP_HUGETLB, -1, 0);
    vcontext.memory_register((void*)addr, size);
    rdmanager::vQP* vqp = vcontext.create_vQP_listener("10.10.1.2", "1145");
    getchar();
}