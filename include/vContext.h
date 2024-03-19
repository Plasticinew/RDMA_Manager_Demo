

#include "vQP.h"
#include "PigeonCommon.h"

namespace rdmanager{

class vContext{
public:
    // change the vector to a pair of two string: device name and device ip
    vContext(std::vector<PigeonDevice> *skip_device_list, std::vector<PigeonDevice> *named_device_list) ;

    vQP* create_vQP_connecter(const std::string ip, const std::string port);
    vQP* create_vQP_listener(const std::string ip, const std::string port);

    void memory_register(void* addr, size_t length) {
        for(auto iter = context_list_.begin(); iter != context_list_.end(); iter ++) {
            (*iter).PigeonMemoryRegister(addr, length);
        }
    }

private:

    std::vector<PigeonDevice> skip_device_list_;
    std::vector<PigeonDevice> named_device_list_;
    std::vector<PigeonContext> context_list_ ;

};


}