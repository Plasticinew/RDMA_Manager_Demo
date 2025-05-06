#pragma once

#include "PigeonContext.h"
#include "PigeonCommon.h"
#include "DynamicContext.h"

namespace rdmanager{

class vContext{
public:
    // change the vector to a pair of two string: device name and device ip
    vContext(std::vector<PigeonDevice> *skip_device_list, std::vector<PigeonDevice> *named_device_list) ;

    void add_device(PigeonDevice device);
    void create_RPC(const std::string ip, const std::string port);
    void create_connecter(const std::string ip, const std::string port);
    void create_listener(const std::string port);
    void alloc_RPC(uint64_t* addr, uint32_t* rkey, uint64_t size);

    void memory_register(void* addr, size_t length) {
        for(int i = 0; i < context_list_.size(); i++) {
            context_list_[i].PigeonMemoryRegister(addr, length);
            back_context_send_[i].PigeonMemoryRegister(addr, length);
        }
    }

    ibv_mr* memory_register_temp(void* addr, size_t length) {
        return context_list_[primary_index_].PigeonMemReg(addr, length);
    }

    void memory_bind(void* addr, size_t length) {
        uint32_t rkey_result = 0;
        for(auto iter = context_list_.begin(); iter != context_list_.end(); iter ++) {
            while((*iter).get_status() != PigeonStatus::PIGEON_STATUS_ACCEPTED && (*iter).get_status() != PigeonStatus::PIGEON_STATUS_CONNECTED);
            (*iter).PigeonBind(addr, length, rkey_result);
            printf("bind success with %u\n", rkey_result);
        }
    }

    void memory_unbind(void* addr) {
        uint32_t rkey;
        for(auto iter = context_list_.begin(); iter != context_list_.end(); iter ++) {
            (*iter).PigeonBind(addr, 0, rkey);
        }
    }

    ibv_qp* get_qp() {
        // TODO: automatically remove the pigeon that is in error status and add a new one 
        while (context_list_[primary_index_].get_status() == PigeonStatus::PIGEON_STATUS_INIT);
        while (context_list_[primary_index_].get_status() == PigeonStatus::PIGEON_STATUS_ERROR) {
            pigeon_swap(primary_index_, secondary_index_);
            pigeon_debug("primary qp error, switch to secondary qp\n");
        } 
        // context_list_[primary_index_].show_device();
        return context_list_[primary_index_].get_qp();
    }

    ibv_cq* get_cq() {
        // TODO: automatically remove the pigeon that is in error status and add a new one 
        while (context_list_[primary_index_].get_status() == PigeonStatus::PIGEON_STATUS_INIT);
        while (context_list_[primary_index_].get_status() == PigeonStatus::PIGEON_STATUS_ERROR) {
            pigeon_swap(primary_index_, secondary_index_);
            pigeon_debug("primary qp error, switch to secondary qp\n");
        }
        // context_list_[primary_index_].show_device();
        return context_list_[primary_index_].get_cq();
    }

    uint32_t get_lkey() {
        return context_list_[primary_index_].get_mr()->lkey;
    }

    DynamicContext* get_primary_dynamic() {
        return &back_context_send_[primary_index_];
    }

    void switch_pigeon() {
        pigeon_swap(primary_index_, secondary_index_);
        if(!context_list_[primary_index_].connected())
            create_connecter(ip_, port_);
    }

    bool connected() {
        return context_list_[primary_index_].connected();
    }

    uint64_t get_log_addr() {
        return context_list_[primary_index_].server_cmd_msg_;
    }
    uint32_t get_log_rkey() {
        return context_list_[primary_index_].server_cmd_rkey_;
    }

private:
    std::vector<PigeonDevice> skip_device_list_;
    std::vector<PigeonDevice> named_device_list_;
    std::vector<PigeonContext> context_list_ ;
    PigeonContext* RPC_context_ = NULL;
    std::vector<DynamicContext> back_context_send_ ;
    std::vector<DynamicContext> back_context_recv_ ;
    std::string ip_, port_;
    int primary_index_ = 0;
    int secondary_index_ = 1;

};


}