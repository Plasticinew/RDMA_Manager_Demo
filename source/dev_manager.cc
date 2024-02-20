
#include "dev_manager.h"

namespace rdma_manager{

    dev_manager::dev_manager(std::vector<std::string> &skip_device_list) {
        int device_num;
        struct ibv_context** device_list; 
        // get unused device list
        for(auto iter = skip_device_list.begin(); iter != skip_device_list.end(); iter ++) {
            std::string str(*iter);
            skip_device_list_.push_back(str);
        }
        // search all avaliable rdma device
        device_list = rdma_get_devices(&device_num);
        for(int i = 0; i < device_num; i ++) {
            if(skip_device_list_.end() == 
                std::find(skip_device_list_.begin(), skip_device_list_.end(), device_list[i]->device->name)) {
                rdma_device new_dev(device_list[i]);
                device_list_.push_back(new_dev);
            }
        }
    }

}