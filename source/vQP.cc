
#include <vQP.h>

namespace rdmanager {

int vQP::read(void* local_addr, uint64_t length, void* remote_addr, uint32_t rkey, uint32_t lid, uint32_t dct_num) {
    // 注释掉的语句目的在于将RCQP创建连接的过程设置为同步行为，即不使用DCQP过渡
    // while(!context_->connected());
    // 确认当前RCQP是否创建成功以及可用
    if(context_->connected())
        return read_main(local_addr, length, remote_addr, rkey);
    else{
        // 不可用时，使用DCQP
        // printf("using temple connect\n");
        return read_backup(local_addr, length, remote_addr, rkey, lid, dct_num);
    }
}

int vQP::write(void* local_addr, uint64_t length, void* remote_addr, uint32_t rkey, uint32_t lid, uint32_t dct_num) {
    if(context_->connected())
        return write_main(local_addr, length, remote_addr, rkey);
    else
        return write_backup(local_addr, length, remote_addr, rkey, lid, dct_num);
}

int vQP::read_main(void* local_addr, uint64_t length, void* remote_addr, uint32_t rkey) {

    struct ibv_sge sge;
    sge.addr = (uint64_t)local_addr;
    sge.length = length;
    sge.lkey = context_->get_lkey();

    struct ibv_send_wr send_wr = {};
    struct ibv_send_wr *bad_send_wr;
    send_wr.wr_id = 0;
    send_wr.sg_list = &sge;
    send_wr.num_sge = 1;
    send_wr.next = NULL;
    send_wr.opcode = IBV_WR_RDMA_READ;
    send_wr.send_flags = IBV_SEND_SIGNALED;
    send_wr.wr.rdma.remote_addr = (uint64_t)remote_addr;
    send_wr.wr.rdma.rkey = rkey;

    ibv_qp* qp = context_->get_qp();
    if (ibv_post_send(qp, &send_wr, &bad_send_wr)) {
        std::cerr << "Error, ibv_post_send failed" << std::endl;
        return -1;
    }

    auto start = TIME_NOW;
    int ret = -1;
    struct ibv_wc wc;
    ibv_cq* cq = context_->get_cq();
    while(true) {
        if(TIME_DURATION_US(start, TIME_NOW) > RDMA_TIMEOUT_US) {
            std::cerr << "Error, read timeout" << std::endl;
            break;
        }
        if(ibv_poll_cq(cq, 1, &wc) > 0) {
            if(wc.status != IBV_WC_SUCCESS) {
                std::cerr << "Error, read failed: " << wc.status << std::endl;
                break;
            }
            ret = 0;
            break;
        }
    }

    return ret;
}

int vQP::write_main(void* local_addr, uint64_t length, void* remote_addr, uint32_t rkey) {
    struct ibv_send_wr log_wr = {};
    struct ibv_sge log_sge;
    bool use_log = false;
    if(use_log){
        time_stamp += 1;
        log_sge.addr = (uint64_t)(&time_stamp);
        log_sge.length = sizeof(uint32_t);
        log_sge.lkey = 0;
        log_wr.wr_id = 1;
        log_wr.sg_list = &log_sge;
        log_wr.num_sge = 1;
        log_wr.next = NULL;
        log_wr.opcode = IBV_WR_RDMA_WRITE;
        log_wr.send_flags = IBV_SEND_SIGNALED | IBV_SEND_INLINE;
        log_wr.wr.rdma.remote_addr = context_->get_log_addr();
        log_wr.wr.rdma.rkey = context_->get_log_rkey();
    }        
    struct ibv_sge sge;
    sge.addr = (uint64_t)local_addr;
    sge.length = length;
    sge.lkey = context_->get_lkey();

    struct ibv_send_wr send_wr = {};
    struct ibv_send_wr *bad_send_wr;
    send_wr.wr_id = 0;
    send_wr.sg_list = &sge;
    send_wr.num_sge = 1;
    if(use_log)
        send_wr.next = &log_wr;
    else
        send_wr.next = NULL;
    send_wr.opcode = IBV_WR_RDMA_WRITE;
    if(use_log)
        send_wr.send_flags = 0;
    else
        send_wr.send_flags = IBV_SEND_SIGNALED;
    send_wr.wr.rdma.remote_addr = (uint64_t)remote_addr;
    send_wr.wr.rdma.rkey = rkey;

    ibv_qp* qp = context_->get_qp();
    if (ibv_post_send(qp, &send_wr, &bad_send_wr)) {
        perror("Error, ibv_post_send failed");
        return -1;
    }

    auto start = TIME_NOW;
    int ret = -1;
    struct ibv_wc wc;
    ibv_cq* cq = context_->get_cq();
    while(true) {
        if(TIME_DURATION_US(start, TIME_NOW) > RDMA_TIMEOUT_US) {
            std::cerr << "Error, write timeout" << std::endl;
            break;
        }
        if(ibv_poll_cq(cq, 1, &wc) > 0) {
            if(wc.status != IBV_WC_SUCCESS) {
                std::cerr << "Error, write failed: " << wc.status << std::endl;
                break;
            }
            ret = 0;
            break;
        }
    }

    return ret;
}

int vQP::read_backup(void* local_addr, uint64_t length, void* remote_addr, uint32_t rkey, uint32_t lid, uint32_t dct_num) {
    return context_->get_primary_dynamic()->DynamicRead(local_addr, length, remote_addr, rkey, lid, dct_num);
}

int vQP::write_backup(void* local_addr, uint64_t length, void* remote_addr, uint32_t rkey, uint32_t lid, uint32_t dct_num) {
    return context_->get_primary_dynamic()->DynamicWrite(local_addr, length, remote_addr, rkey, lid, dct_num);
}

int vQP::alloc_RPC(uint64_t* addr, uint32_t* rkey, uint64_t size) {
    uint64_t remote_addr; uint32_t remote_rkey;
    context_->alloc_RPC(&remote_addr, &remote_rkey, size);
    *addr = remote_addr;
    *rkey = remote_rkey;
    return 0;
}

}