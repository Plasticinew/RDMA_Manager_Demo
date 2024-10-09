
#include "PigeonCommon.h"
#include "vContext.h"

namespace rdmanager{

class vQP {

public:
    vQP(vContext* context) {
        context_ = context;
    };

    // The method for vQP read
    int read(void* local_addr, uint64_t length, void* remote_addr, uint32_t rkey);

    // The method for vQP write
    int write(void* local_addr, uint64_t length, void* remote_addr, uint32_t rkey);

    // Using RCQP
    int read_main(void* local_addr, uint64_t length, void* remote_addr, uint32_t rkey);
    int write_main(void* local_addr, uint64_t length, void* remote_addr, uint32_t rkey);

    // Using DCQP
    int read_backup(void* local_addr, uint64_t length, void* remote_addr, uint32_t rkey, uint32_t lid, uint32_t dct_num);
    int write_backup(void* local_addr, uint64_t length, void* remote_addr, uint32_t rkey, uint32_t lid, uint32_t dct_num);

    void switch_card() {
        context_->switch_pigeon();
    }

private:
    vContext* context_;

};


}