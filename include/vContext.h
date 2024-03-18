

#include "vQP.h"
#include "PigeonCommon.h"

namespace rdmanager{

class vContext{
public:
    vContext(std::vector<std::string> *skip_device_list, std::vector<std::string> *named_device_list) ;

    vQP* create_vQP_connecter(const std::string ip, const std::string port);
    vQP* create_vQP_listener(const std::string ip, const std::string port);

    void memory_register(void* addr, size_t length) {
        for(auto iter = device_list_.begin(); iter != device_list_.end(); iter ++) {
            (*iter).PigeonMemoryRegister(addr, length);
        }
    }

private:

    std::vector<std::string> skip_device_list_;
    std::vector<std::string> named_device_list_;
    std::vector<PigeonContext> device_list_ ;

};


}