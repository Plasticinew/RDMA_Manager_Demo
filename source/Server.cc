
#include "vQP.h"

using std::vector;
using rdmanager::PigeonDevice;
using rdmanager::vContext;
using rdmanager::vQP;

const uint64_t size = (uint64_t)2*1024*1024*1024;
const uint64_t addr = 0x1000000;

int main() {
    std::vector<PigeonDevice> skip_device_list;
    // std::vector<PigeonDevice> named_device_list = {{"mlx5_2", "10.10.1.1"}, {"mlx5_7", "10.10.1.13"}};
    std::vector<PigeonDevice> named_device_list = {{"mlx5_2", "10.10.1.2"}};
    rdmanager::vContext* vcontext = new rdmanager::vContext(&skip_device_list, &named_device_list);
    mmap((void*)addr, size, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED|MAP_HUGETLB, -1, 0);
    vcontext->memory_register((void*)addr, size);
    vcontext->create_listener("1145");
    vcontext->memory_bind((void*)addr, size);
    int enode = -1;
    rdmanager::vQP* vqp = new rdmanager::vQP(vcontext, &enode);
    getchar();
}