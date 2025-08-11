#include "vQP.h"

using std::vector;
using rdmanager::PigeonDevice;
using rdmanager::vContext;
using rdmanager::vQP;

uint64_t buffer_size = (uint64_t)2*1024*1024*1024;
uint64_t addr = 0x1000000;

int main() {
    printf("Server start\n");
    vector<PigeonDevice> skip_device_list;
    vector<PigeonDevice> named_device_list = {{"mlx5_2", "10.10.1.1"}, {"mlx5_7", "10.10.1.13"}};
    rdmanager::vContext* vcontext = new rdmanager::vContext(&skip_device_list, &named_device_list);
    mmap((void*)addr, buffer_size, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED|MAP_HUGETLB, -1, 0);
    vcontext->memory_register((void*)addr, buffer_size);
    vcontext->create_RPC("10.10.1.1", "1145");
    vcontext->create_connecter("10.10.1.1", "1145");
    int enode = -1;
    rdmanager::vQP* vqp = new rdmanager::vQP(vcontext, &enode);
    getchar();
    return 0;
}