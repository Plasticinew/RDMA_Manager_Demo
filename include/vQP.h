
#include "PigeonCommon.h"
#include "vContext.h"
#include <thread>

namespace rdmanager{

class vQP {

public:

struct WRBuffer {
    uint32_t wr_timestamp_;
    uint32_t wr_rkey_;
    uint64_t wr_local_addr_;
    uint64_t wr_length_;
    uint64_t wr_remote_addr_;
    OpType wr_optype_;
}; 

    vQP(vContext* context) {
        context_ = context;
    };

    // The method for vQP read
    ErrorType read(void* local_addr, uint64_t length, void* remote_addr, uint32_t rkey, uint32_t lid, uint32_t dct_num);

    // The method for vQP write
    ErrorType write(void* local_addr, uint64_t length, void* remote_addr, uint32_t rkey, uint32_t lid, uint32_t dct_num);

    // Using RCQP
    ErrorType read_main(void* local_addr, uint64_t length, void* remote_addr, uint32_t rkey);
    ErrorType write_main(void* local_addr, uint64_t length, void* remote_addr, uint32_t rkey);

    // Using DCQP
    ErrorType read_backup(void* local_addr, uint64_t length, void* remote_addr, uint32_t rkey, uint32_t lid, uint32_t dct_num);
    ErrorType write_backup(void* local_addr, uint64_t length, void* remote_addr, uint32_t rkey, uint32_t lid, uint32_t dct_num);

    // Using RPC
    int alloc_RPC(uint64_t* addr, uint32_t* rkey, uint64_t size);

    void switch_card() {
        // context_->switch_pigeon();
        std::thread* listen_thread = new std::thread(&vContext::switch_pigeon, context_);
    }

    void recovery(void* local_addr, uint64_t length, uint32_t lid, uint32_t dct_num);

private:
    vContext* context_;
    uint32_t time_stamp = 0;
    WRBuffer work_queue_[wr_buffer_size];
    int work_queue_start_ = 0;
    int work_queue_finished_ = 0;
    CmdMsgBlock *read_log;
    struct ibv_mr *read_log_mr;
};


}