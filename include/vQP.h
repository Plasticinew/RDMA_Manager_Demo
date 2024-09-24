
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

    void switch_card() {
        context_->switch_pigeon();
    }

private:
    vContext* context_;

};


}