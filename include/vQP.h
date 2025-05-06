
#include "PigeonCommon.h"
#include "vContext.h"
#include <thread>

namespace rdmanager{

class vQP {

public:
    vQP(vContext* context) {
        context_ = context;
    };

    // The method for vQP read
    int read(void* local_addr, uint64_t length, void* remote_addr, uint32_t rkey, uint32_t lid, uint32_t dct_num);

    // The method for vQP write
    int write(void* local_addr, uint64_t length, void* remote_addr, uint32_t rkey, uint32_t lid, uint32_t dct_num);

    // Using RCQP
    int read_main(void* local_addr, uint64_t length, void* remote_addr, uint32_t rkey);
    int write_main(void* local_addr, uint64_t length, void* remote_addr, uint32_t rkey);

    // Using DCQP
    int read_backup(void* local_addr, uint64_t length, void* remote_addr, uint32_t rkey, uint32_t lid, uint32_t dct_num);
    int write_backup(void* local_addr, uint64_t length, void* remote_addr, uint32_t rkey, uint32_t lid, uint32_t dct_num);

    // Using RPC
    int alloc_RPC(uint64_t* addr, uint32_t* rkey, uint64_t size);

    void switch_card() {
        // context_->switch_pigeon();
        std::thread* listen_thread = new std::thread(&vContext::switch_pigeon, context_);
    }

private:
    vContext* context_;
    uint32_t time_stamp = 0;

};


}