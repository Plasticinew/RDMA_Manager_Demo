
#pragma once

#include "PigeonCommon.h"
#include <infiniband/mlx5dv.h>
// #include "hiredis/hiredis.h"
// #include <infiniband/verbs_exp.h>

namespace rdmanager{

class DynamicContext{
public:
    DynamicContext(ibv_context* context, PigeonDevice device, ibv_pd* pd);

    void DynamicConnect();
    void DynamicListen();
    ErrorType DynamicRead(void* local_addr, uint64_t length, void* remote_addr, uint32_t rkey, uint32_t lid, uint32_t dct_num);
    ErrorType DynamicWrite(void* local_addr, uint64_t length, void* remote_addr, uint32_t rkey, uint32_t lid, uint32_t dct_num);
    void PigeonMemoryRegister(void* addr, size_t length);
    ibv_mr* PigeonMemReg(void* addr, size_t length){
        ibv_mr* mr = ibv_reg_mr(pd_, addr, length, IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_WRITE  | IBV_ACCESS_REMOTE_ATOMIC | IBV_ACCESS_MW_BIND);
        // pigeon_debug("device %s register memory %p, length %lu, rkey %u\n", device_.name.c_str(), addr, length, mr->rkey);
        assert(mr != NULL);
        return mr;
    }
    // Type 2 MW
    void PigeonBind(void* addr, uint64_t length, uint32_t &result_rkey);
    void PigeonUnbind(void* addr);

    void CreateAh(uint64_t input_gid1, uint64_t input_gid2, uint64_t input_interface, uint64_t input_subnet, uint32_t input_lid);

    ibv_qp* get_qp() {
        return qp_;
    }

    ibv_cq* get_cq() {
        return cq_;
    }

    PigeonStatus get_status() {
        return status_;
    }

    std::string get_name() {
        return device_.name;
    }

    std::string get_ip() {
        return device_.ip;
    }

    ibv_mr* get_mr() {
        return mr_;
    }

    void show_device() {
        pigeon_debug("device name: %s\n", device_.name.c_str());
        pigeon_debug("device ip: %s\n", device_.ip.c_str());
    }
    uint64_t gid1, gid2, interface, subnet;
    uint8_t port_num_;
    uint16_t lid_;
    uint32_t dct_num_;

    uint64_t target_gid1, target_gid2, target_interface, target_subnet;
    uint8_t target_port_num_;
    uint16_t target_lid_;
    uint32_t target_dct_num_;

private:
    ibv_context* context_;
    ibv_device* dev_;
    ibv_pd* pd_;
    ibv_mr* mr_;
    std::map<uint64_t, ibv_mw*> mw_pool_;
    ibv_qp* qp_;
    ibv_qp_ex* qp_ex_;
    mlx5dv_qp_ex* qp_mlx_ex_;
    ibv_cq* cq_;
    ibv_srq* srq_;
    ibv_ah* ah_;
    PigeonStatus status_;
    PigeonDevice device_;

};

}