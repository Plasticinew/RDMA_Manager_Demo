
#include "vContext.h"

namespace rdmanager{

extern std::map<std::string, DynamicContext*> r_context;
extern std::map<std::string, DynamicContext*> s_context;

// 管理一组网卡多个QP的上下文
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
                    if(RPC_context_ == NULL) {
                        PigeonContext* rpc_context = new PigeonContext(device_list[i], *iter);
                        RPC_context_ = rpc_context;
                    }
                    if(r_context.find((*iter).name) == r_context.end()){
                        r_context[(*iter).name] = new DynamicContext(device_list[i], (*iter), context.get_pd());
                        // r_context[(*iter).name] = DynamicContext(device_list[i], (*iter), context.get_pd());
                        r_context[(*iter).name]->DynamicListen();
                    }
                    back_context_recv_.push_back(r_context[(*iter).name]);
                    if(s_context.find((*iter).name) == s_context.end()){
                        s_context[(*iter).name] = new DynamicContext(device_list[i], (*iter), context.get_pd());
                        // r_context[(*iter).name] = DynamicContext(device_list[i], (*iter), context.get_pd());
                        s_context[(*iter).name]->DynamicConnect();
                    }
                    back_context_send_.push_back(s_context[(*iter).name]);
                    context.SetDynamicConnection(r_context[(*iter).name]);
                    context_list_.push_back(context);
                    // DynamicContext s_context(device_list[i], (*iter), context.get_pd());
                    // s_context.DynamicConnect();
                    // back_context_send_.push_back(s_context);
                }
            }
        }
    }
    
}


void vContext::add_device(PigeonDevice device) {
    int device_num;
    struct ibv_context** device_list; 
    named_device_list_.push_back(device);
    
    // search all avaliable rdma device
    device_list = rdma_get_devices(&device_num);
    for(int i = 0; i < device_num; i ++) {
        if(named_device_list_.size() == 0) {
            pigeon_debug("not implement yet\n");
        } else {
            if(device_list[i]->device->name == device.name) {
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
                // 创建QP与DCQP
                // 未来考虑通过共享减少DCQP的个数
                PigeonContext context(device_list[i], device);
                // DynamicContext r_context(device_list[i], device, context.get_pd());
                // r_context.DynamicListen();
                // back_context_recv_.push_back(r_context);
                // DynamicContext s_context(device_list[i], device, context.get_pd());
                // s_context.DynamicConnect();
                // back_context_send_.push_back(s_context);
                if(r_context.find(device.name) == r_context.end()){
                    r_context[device.name] = new DynamicContext(device_list[i], device, context.get_pd());
                    // r_context[device.name] = DynamicContext(device_list[i], device, context.get_pd());
                    r_context[device.name]->DynamicListen();
                }
                back_context_recv_.push_back(r_context[device.name]);
                if(s_context.find(device.name) == s_context.end()){
                    s_context[device.name] = new DynamicContext(device_list[i], device, context.get_pd());
                    // r_context[device.name] = DynamicContext(device_list[i], device, context.get_pd());
                    s_context[device.name]->DynamicConnect();
                }
                back_context_send_.push_back(s_context[device.name]);
                context.SetDynamicConnection(r_context[device.name]);
                context_list_.push_back(context);
                break;
            }
        }
    }
    
}


void vContext::create_connecter(const std::string ip, const std::string port) {
    // create QP connection with certain ip...
    ip_ = ip;
    port_ = port;
    context_list_[primary_index_].PigeonConnect(ip, port, 0, CONN_ONESIDE);
    gid1 = context_list_[primary_index_].gid1;
    gid2 = context_list_[primary_index_].gid2;
    interface = context_list_[primary_index_].interface;
    subnet = context_list_[primary_index_].subnet;
    lid_ = context_list_[primary_index_].lid_;
    dct_num_ = context_list_[primary_index_].dct_num_;
    for(auto i = back_context_send_.begin(); i!=back_context_send_.end(); i++)
        (*i)->CreateAh(gid1, gid2, interface, subnet, lid_); 
    // for (auto iter = context_list_.begin(); iter != context_list_.end(); iter ++) {
    //     (*iter).PigeonConnect(ip, port);
    // }
    return;
}

void vContext::create_listener(const std::string port) {
    // create listening thread on certain ip
    port_ = port;
    for(int i = 0; i < context_list_.size(); i++) {
        // pigeon_debug("create listener on %s\n", (*iter).get_name().c_str());
        std::thread* listen_thread = new std::thread(&PigeonContext::PigeonListen, context_list_[i], named_device_list_[i].ip, port);
    }
    return;
}

void vContext::create_RPC(const std::string ip, const std::string port) {
    ip_ = ip;
    port_ = port;
    RPC_context_->PigeonConnect(ip, port, 0, CONN_RPC);
    return;
}

void vContext::alloc_RPC(uint64_t* addr, uint32_t* rkey, uint64_t size) {
    uint64_t remote_addr; uint32_t remote_rkey;
    RPC_context_->PigeonRPCAlloc(&remote_addr, &remote_rkey, size);
    *addr = remote_addr;
    *rkey = remote_rkey;
    return;
}

}