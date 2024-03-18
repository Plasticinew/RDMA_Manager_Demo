
#include "PigeonCommon.h"
#include "vQPMapper.h"

namespace rdmanager{

class vQP {

public:
    vQP(vQPMapper* mapper) {
        mapper_ = mapper;
    };

    // The method for vQP read
    int read(void* local_addr, uint64_t length, void* remote_addr, uint32_t rkey);

    // The method for vQP write
    int write(void* local_addr, uint64_t length, void* remote_addr, uint32_t rkey);



private:
    
    vQPMapper* mapper_;


};


}