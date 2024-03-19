
#include "vContext.h"

namespace rdmanager{

vContext::vContext(std::vector<PigeonDevice> *skip_device_list, std::vector<PigeonDevice> *named_device_list) {
    int device_num;
    struct ibv_context** device_list; 
    
    // get unused device list
    if(skip_device_list != NULL){
        for(auto iter = skip_device_list->begin(); iter != skip_device_list->end(); iter ++) {
            PigeonDevice dev(*iter);
            skip_device_list_.push_back(dev);
        }
    }

    if(named_device_list != NULL){
        for(auto iter = named_device_list->begin(); iter != named_device_list->end(); iter ++) {
            PigeonDevice dev(*iter);
            named_device_list_.push_back(dev);
        }
    }

    // search all avaliable rdma device
    device_list = rdma_get_devices(&device_num);
    for(int i = 0; i < device_num; i ++) {
        if(named_device_list_.size() == 0) {
            pigeon_debug("not implement yet\n");
        } else {
            for(auto iter = named_device_list_.begin(); iter != named_device_list_.end(); iter ++) {
                if((*iter).name == device_list[i]->device->name) {
                    PigeonContext context(device_list[i], *iter);
                    context_list_.push_back(context);
                }
            }
        }
    }
    
}

vQP* vContext::create_vQP_connecter(const std::string ip, const std::string port) {
    vQPMapper* mapper = new vQPMapper();
    for (auto iter = context_list_.begin(); iter != context_list_.end(); iter ++) {
        (*iter).PigeonConnect(ip, port);
        mapper->add_pigeon_context(&(*iter));
    }
    vQP* vqp = new vQP(mapper);
    return vqp;
}

vQP* vContext::create_vQP_listener(const std::string ip, const std::string port) {
    vQPMapper* mapper = new vQPMapper();
    for(auto iter = context_list_.begin(); iter != context_list_.end(); iter ++) {
        // pigeon_debug("create listener on %s\n", (*iter).get_name().c_str());
        std::thread* listen_thread = new std::thread(&PigeonContext::PigeonListen, &(*iter), ip, port);
        mapper->add_pigeon_context(&(*iter));
    }
    vQP* vqp = new vQP(mapper);
    return vqp;
}

}