#pragma once

#include "PigeonContext.h"
#include "PigeonCommon.h"
#include "DynamicContext.h"
#include <map>
#include <string>
#include <cstdlib>
#include <random>
#include <unistd.h>
#include <mutex>

namespace rdmanager{

static std::mutex m_mutex_;

static std::map<std::string, DynamicContext*> r_context;
static std::map<std::string, DynamicContext*> s_context;

class vContext{
public:
    // change the vector to a pair of two string: device name and device ip
    uint64_t gid1, gid2, interface, subnet;
    uint16_t lid_;
    uint32_t dct_num_;
    int total_failure = 1;

    vContext(std::vector<PigeonDevice> *skip_device_list, std::vector<PigeonDevice> *named_device_list) ;

    void add_device(PigeonDevice device);
    void create_RPC(const std::string ip, const std::string port);
    bool create_connecter(const std::string ip, const std::string port);
    void create_listener(const std::string port);
    void alloc_RPC(uint64_t* addr, uint32_t* rkey, uint64_t size);

    void memory_register(void* addr, size_t length) {
        for(int i = 0; i < context_list_.size(); i++) {
            context_list_[i].PigeonMemoryRegister(addr, length);
            // back_context_send_[i]->PigeonMemoryRegister(addr, length);
        }
        for(int i = 0; i < back_context_send_.size(); i++) {
            back_context_send_[i]->PigeonMemoryRegister(addr, length);
        }
        addr_remote = (uint64_t)addr;
        length_remote = length;
    }

    ibv_mr* memory_register_temp(void* addr, size_t length) {
        pigeon_debug("temp device register memory\n");
        return back_context_send_[primary_index_]->PigeonMemReg(addr, length);
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
        // while (context_list_[primary_index_].get_status() == PigeonStatus::PIGEON_STATUS_ERROR) {
        //     pigeon_swap(primary_index_, secondary_index_);
        //     pigeon_debug("primary qp error, switch to secondary qp\n");
        // } 
        // context_list_[primary_index_].show_device();
        return context_list_[primary_index_].get_qp();
    }

    ibv_cq* get_cq() {
        // TODO: automatically remove the pigeon that is in error status and add a new one 
        while (context_list_[primary_index_].get_status() == PigeonStatus::PIGEON_STATUS_INIT);
        // while (context_list_[primary_index_].get_status() == PigeonStatus::PIGEON_STATUS_ERROR) {
        //     pigeon_swap(primary_index_, secondary_index_);
        //     pigeon_debug("primary qp error, switch to secondary qp\n");
        // }
        // context_list_[primary_index_].show_device();
        return context_list_[primary_index_].get_cq();
    }

    uint32_t get_lkey() {
        return context_list_[primary_index_].get_mr()->lkey;
    }

    DynamicContext* get_primary_dynamic() {
        return back_context_send_[primary_index_];
    }

    void switch_pigeon() {
        context_list_[primary_index_].status_ = PIGEON_STATUS_ERROR;
        // context_list_[primary_index_].PigeonDisconnected();
        // secondary_index_ = context_list_.size()-1;
        pigeon_swap(primary_index_, secondary_index_);
        
        // lock.unlock();
    }

    void new_connect() {
        retry:
        std::unique_lock<std::mutex> lock(m_mutex_);
        if(!context_list_[primary_index_].connected()){
            if(!create_connecter(ip_, port_)){
                lock.unlock();
                std::this_thread::yield();
                goto retry;
            }
        }
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

    uint64_t log_addr_persist;
    uint32_t log_rkey_persist;

    bool down_primary() {
        if(total_failure > 0 && rand_val()%1000 == 1){
            // downed = true;
            total_failure -= 1;
            recovery_primary = primary_index_;
            std::string command = "sudo ip link set ";
            command += context_list_[primary_index_].get_netname();
            command += " down";
            system(command.c_str());
            return true;
        }
        return false;
    }

    void up_primary() {
        std::string command = "sudo ip link set ";
        command += context_list_[recovery_primary].get_netname();
        command += " up";
        system(command.c_str());
        // usleep(1000);
        // if(!context_list_[recovery_primary].connected()) {
        // add_device(context_list_[recovery_primary].device_);
            // create_RPC(ip_, port_);
        // context_list_[context_list_.size()-1].PigeonMemoryRegister((void*)addr_remote, length_remote);
        // }
    }

    void readd_device(int index);
    int primary_index_ = 0;
    int secondary_index_ = 1;
    int recovery_primary = 0;

private:
    std::vector<PigeonDevice> skip_device_list_;
    std::vector<PigeonDevice> named_device_list_;
    std::vector<PigeonContext> context_list_ ;
    PigeonContext* RPC_context_ = NULL;
    std::vector<DynamicContext*> back_context_send_ ;
    std::vector<DynamicContext*> back_context_recv_ ;
    std::string ip_, port_;
    uint64_t addr_remote;
    uint64_t length_remote;
    std::random_device r;
    std::mt19937 rand_val;

};


}