
#include "vContext.h"

namespace rdmanager{

vContext::vContext(std::vector<std::string> *skip_device_list, std::vector<std::string> *named_device_list) {
    int device_num;
    struct ibv_context** device_list; 
    
    // get unused device list
    if(skip_device_list != NULL){
        for(auto iter = skip_device_list->begin(); iter != skip_device_list->end(); iter ++) {
            std::string str(*iter);
            skip_device_list_.push_back(str);
        }
    }

    if(named_device_list != NULL){
        for(auto iter = named_device_list->begin(); iter != named_device_list->end(); iter ++) {
            std::string str(*iter);
            named_device_list_.push_back(str);
        }
    }

    // search all avaliable rdma device
    device_list = rdma_get_devices(&device_num);
    for(int i = 0; i < device_num; i ++) {
        if(named_device_list_.size() == 0) {
            if(skip_device_list_.end() == 
                std::find(skip_device_list_.begin(), skip_device_list_.end(), device_list[i]->device->name)) {
                PigeonContext new_dev(device_list[i]);
                device_list_.push_back(new_dev);
            }
        } else {
            if(named_device_list_.end() != 
                std::find(named_device_list_.begin(), named_device_list_.end(), device_list[i]->device->name)) {
                PigeonContext new_dev(device_list[i]);
                device_list_.push_back(new_dev);
            }
        }
    }
    
}

vQP* vContext::create_vQP_connecter(const std::string ip, const std::string port) {
    vQPMapper* mapper = new vQPMapper();
    for (auto iter = device_list_.begin(); iter != device_list_.end(); iter ++) {
        (*iter).PigeonConnect(ip, port);
        mapper->add_pigeon_context(&(*iter));
    }
    vQP* vqp = new vQP(mapper);
    return vqp;
}

vQP* vContext::create_vQP_listener(const std::string ip, const std::string port) {
    vQPMapper* mapper = new vQPMapper();
    for(auto iter = device_list_.begin(); iter != device_list_.end(); iter ++) {
        // pigeon_debug("create listener on %s\n", (*iter).get_name().c_str());
        std::thread* listen_thread = new std::thread(&PigeonContext::PigeonListen, &(*iter), ip, port);
        mapper->add_pigeon_context(&(*iter));
    }
    vQP* vqp = new vQP(mapper);
    return vqp;
}

}