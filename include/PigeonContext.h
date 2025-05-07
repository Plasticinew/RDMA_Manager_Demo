
#include "PigeonCommon.h"
#include "DynamicContext.h"

namespace rdmanager{

class PigeonContext{
public:
struct WorkerInfo {
    CmdMsgBlock *cmd_msg;
    CmdMsgRespBlock *cmd_resp_msg;
    struct ibv_mr *msg_mr;
    struct ibv_mr *resp_mr;
    rdma_cm_id *cm_id;
    struct ibv_cq *cq;
}; 
    struct CmdMsgBlock *cmd_msg_;
    struct CmdMsgRespBlock *cmd_resp_;
    struct ibv_mr *msg_mr_;
    struct ibv_mr *resp_mr_;
    uint64_t server_cmd_msg_;
    uint32_t server_cmd_rkey_;
    uint64_t gid1, gid2, interface, subnet;
    uint16_t lid_;
    uint32_t dct_num_;
    DynamicContext* dynamic_context;
    
    PigeonContext(ibv_context* context, PigeonDevice device);

    void SetDynamicConnection(DynamicContext* dynamic) {
        printf("set dynamic %lu\n", dynamic->gid1);
        dynamic_context = dynamic;
    }

    void PigeonConnect(const std::string ip, const std::string port, uint8_t access_type, uint16_t node);
    void PigeonListen(const std::string ip, const std::string port);
    void PigeonAccept(rdma_cm_id* cm_id, uint8_t connect_type, uint16_t node_id);
    void PigeonMemoryRegister(void* addr, size_t length);
    ibv_mr* PigeonMemReg(void* addr, size_t length);
    ibv_mr* PigeonMemAllocReg(uint64_t &addr, uint32_t &rkey, size_t length);
    void PigeonRPCAlloc(uint64_t* addr, uint32_t* rkey, size_t size);
    void PigeonWoker(WorkerInfo *work_info, uint32_t num);
    // Type 2 MW
    void PigeonBind(void* addr, uint64_t length, uint32_t &result_rkey);
    void PigeonUnbind(void* addr);

    bool connected() {
        return status_ == PigeonStatus::PIGEON_STATUS_CONNECTED;
    }

    void PigeonWrite(ibv_qp* qp, ibv_cq* cq, uint64_t local_addr,
        uint32_t lkey, uint32_t length,
        uint64_t remote_addr, uint32_t rkey);

    ibv_pd* get_pd() {
        return pd_;
    }

    ibv_qp* get_qp() {
        return cm_id_->qp;
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

private:
    ibv_context* context_;
    ibv_pd* pd_;
    ibv_mr* mr_;
    std::map<uint64_t, ibv_mw*> mw_pool_;
    rdma_event_channel* channel_;
    rdma_cm_id* cm_id_;
    ibv_cq* cq_;
    PigeonStatus status_;
    PigeonDevice device_;
    uint32_t worker_num_;
    WorkerInfo** worker_info_;
    std::thread **worker_threads_;
    bool stop_;

    uint32_t fusee_rkey;
    uint32_t remote_size_;
    uint32_t conn_id_;
    char *reg_buf_;
    struct ibv_mr *reg_buf_mr_;
    CmdMsgBlock* qp_log_[1024];
    struct ibv_mr* qp_log_list_[1024];
};

}