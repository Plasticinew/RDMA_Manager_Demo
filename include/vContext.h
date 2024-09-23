

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

    void memory_bind(void* addr, size_t length) {
        uint32_t global_rkey = rand();
        uint32_t rkey_result = 0;
        for(auto iter = context_list_.begin(); iter != context_list_.end(); iter ++) {
            while((*iter).get_status() != PigeonStatus::PIGEON_STATUS_ACCEPTED);
            if(global_rkey){
                (*iter).PigeonBind(addr, length, global_rkey, rkey_result);
                if((rkey_result & 0xff) != (global_rkey & 0xff)){
                    printf("bind failed!\n");
                }
            } else {
                (*iter).PigeonBind(addr, length, 0, global_rkey);
            }
        }
    }

    void memory_unbind(void* addr) {
        for(auto iter = context_list_.begin(); iter != context_list_.end(); iter ++) {
            (*iter).PigeonUnbind(addr);
        }
    }
private:

    std::vector<PigeonDevice> skip_device_list_;
    std::vector<PigeonDevice> named_device_list_;
    std::vector<PigeonContext> context_list_ ;

};


}