
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
                    struct ibv_device_attr device_attr;
                    struct ibv_device_attr_ex device_attr_ex;
                    struct ibv_query_device_ex_input input;
                    ibv_query_device(device_list[i], & device_attr);
                    if (!(device_attr.device_cap_flags & IBV_DEVICE_MEM_WINDOW)) {
                        printf("do not support memory window\n");
                    } else {
                        printf("support %d memory window and %d qp total\n", device_attr.max_mw, device_attr.max_qp);
                    }
                    ibv_query_device_ex(device_list[i], NULL, &device_attr_ex);
                    if(!(device_attr_ex.odp_caps.general_caps & IBV_ODP_SUPPORT)){
                        printf("Not support odp!\n");
                    }
                    PigeonContext context(device_list[i], *iter);
                    context_list_.push_back(context);
                    DynamicContext r_context(device_list[i], (*iter));
                    r_context.DynamicListen();
                    back_context_recv_.push_back(r_context);
                    DynamicContext s_context(device_list[i], (*iter));
                    s_context.DynamicConnect();
                    back_context_send_.push_back(s_context);
                }
            }
        }
    }
    
}

void vContext::create_connecter(const std::string ip, const std::string port) {
    // create QP connection with certain ip...
    for (auto iter = context_list_.begin(); iter != context_list_.end(); iter ++) {
        (*iter).PigeonConnect(ip, port);
    }
    return;
}

void vContext::create_listener(const std::string ip, const std::string port) {
    // create listening thread on certain ip
    for(auto iter = context_list_.begin(); iter != context_list_.end(); iter ++) {
        // pigeon_debug("create listener on %s\n", (*iter).get_name().c_str());
        std::thread* listen_thread = new std::thread(&PigeonContext::PigeonListen, &(*iter), ip, port);
    }
    return;
}

}