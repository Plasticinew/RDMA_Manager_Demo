
#include <vQP.h>

namespace rdmanager {

void vQP::recovery(void* local_addr, uint64_t length, uint32_t lid, uint32_t dct_num){
    ErrorType err;
    int start_ = work_queue_start_;
    int end_ = work_queue_finished_;
    if(start_ > end_)
        end_ += wr_buffer_size;
    if(start_ == end_){
        return;
    }
    read_log = new CmdMsgBlock();
    memset(read_log, 0, sizeof(CmdMsgBlock));
    // read_log_mr = context_->memory_register_temp((void *)read_log, sizeof(CmdMsgBlock));
    read_backup(local_addr, sizeof(uint32_t)*64, (void*)(context_->log_addr_persist), context_->log_rkey_persist, lid, dct_num);
    // printf("read log@%lu\n", context_->log_addr_persist);
    memcpy(read_log, local_addr, sizeof(uint32_t)*64);
    uint32_t* last_stamp = (uint32_t*)read_log;
    for(int i = start_; i < end_; i++){
        if(work_queue_[i%wr_buffer_size].wr_optype_ == READ){
            err = read_backup((void*)work_queue_[i%wr_buffer_size].wr_local_addr_, 
                work_queue_[i%wr_buffer_size].wr_length_, 
                (void*)work_queue_[i%wr_buffer_size].wr_remote_addr_, 
                work_queue_[i%wr_buffer_size].wr_rkey_, lid, dct_num);
            printf("fix read error\n");
            if(err != NO_ERROR){
                printf("double failure!\n");
            }
        }
        else if(work_queue_[i%wr_buffer_size].wr_length_ != 0 && work_queue_[i%wr_buffer_size].wr_timestamp_ != last_stamp[i%wr_buffer_size]){
            if(work_queue_[i%wr_buffer_size].wr_optype_ == WRITE){
                err = write_backup((void*)work_queue_[i%wr_buffer_size].wr_local_addr_, 
                    work_queue_[i%wr_buffer_size].wr_length_, 
                    (void*)work_queue_[i%wr_buffer_size].wr_remote_addr_, 
                    work_queue_[i%wr_buffer_size].wr_rkey_, lid, dct_num);
                printf("fix write error\n");
                if(err != NO_ERROR){
                    printf("double failure!\n");
                }
            }
        }
    }
    context_->up_primary();
}

ErrorType vQP::read(void* local_addr, uint64_t length, void* remote_addr, uint32_t rkey, uint32_t lid, uint32_t dct_num) {
    // 注释掉的语句目的在于将RCQP创建连接的过程设置为同步行为，即不使用DCQP过渡
    // while(!context_->connected());
    // 确认当前RCQP是否创建成功以及可用
    if(context_->connected()){
        ErrorType err = read_main(local_addr, length, remote_addr, rkey);
        if(err == SEND_ERROR){
            context_->switch_pigeon();
            printf("change nic\n");
            // return read(local_addr, length, remote_addr, rkey, lid, dct_num);
            return read(local_addr, length, remote_addr, rkey, context_->lid_, context_->dct_num_);
        } else if(err == RECIEVE_ERROR){
            context_->switch_pigeon();
            recovery(local_addr, length, lid, dct_num);
            return read(local_addr, length, remote_addr, rkey, context_->lid_, context_->dct_num_);
        }
    }
    else{
        // 不可用时，使用DCQP
        // printf("using temple connect\n");
        return read_backup(local_addr, length, remote_addr, rkey, context_->lid_, context_->dct_num_);
    }
}

ErrorType vQP::write(void* local_addr, uint64_t length, void* remote_addr, uint32_t rkey, uint32_t lid, uint32_t dct_num) {
    if(context_->connected()){
        ErrorType err = write_main(local_addr, length, remote_addr, rkey);
        if(err == SEND_ERROR){
            context_->switch_pigeon();
            printf("change nic\n");
            return write(local_addr, length, remote_addr, rkey, context_->lid_, context_->dct_num_);
        } else if(err == RECIEVE_ERROR){
            context_->switch_pigeon();
            printf("change nic\n");
            recovery(local_addr, length, lid, dct_num);
            return write(local_addr, length, remote_addr, rkey, context_->lid_, context_->dct_num_);
        }
    } else{
        printf("use backup\n");
        return write_backup(local_addr, length, remote_addr, rkey, context_->lid_, context_->dct_num_);
    }
}

ErrorType vQP::read_main(void* local_addr, uint64_t length, void* remote_addr, uint32_t rkey) {

    struct ibv_sge sge;
    sge.addr = (uint64_t)local_addr;
    sge.length = length;
    sge.lkey = context_->get_lkey();
    time_stamp += 1;

    struct ibv_send_wr send_wr = {};
    struct ibv_send_wr *bad_send_wr;
    send_wr.wr_id = time_stamp;
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
        return SEND_ERROR;
    }

    work_queue_[work_queue_finished_].wr_timestamp_ = time_stamp;
    work_queue_[work_queue_finished_].wr_local_addr_ = (uint64_t)local_addr;
    work_queue_[work_queue_finished_].wr_length_ = length;
    work_queue_[work_queue_finished_].wr_remote_addr_ = (uint64_t)remote_addr;
    work_queue_[work_queue_finished_].wr_rkey_ = rkey;
    work_queue_[work_queue_finished_].wr_optype_ = READ;
    work_queue_finished_ = (work_queue_finished_ + 1)%wr_buffer_size;

    auto start = TIME_NOW;
    ErrorType ret = RECIEVE_ERROR;
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
            ret = NO_ERROR;
            int start_ = work_queue_start_;
            int end_ = work_queue_finished_;
            if(start_ > end_)
                end_ += wr_buffer_size;
            if(start_ != end_){
                for(int i = start_; i < end_; i++){
                    if(work_queue_[i%wr_buffer_size].wr_timestamp_ == wc.wr_id){
                        work_queue_[work_queue_finished_].wr_length_ = 0;
                    }
                    if(work_queue_[i%wr_buffer_size].wr_length_ == 0 && i%wr_buffer_size == work_queue_start_){
                        work_queue_start_ = (work_queue_start_ + 1)%wr_buffer_size;
                    }
                }
            }
            break;
        }
    }

    return ret;
}

ErrorType vQP::write_main(void* local_addr, uint64_t length, void* remote_addr, uint32_t rkey) {
    struct ibv_send_wr log_wr = {};
    struct ibv_sge log_sge;
    bool use_log = true;
    if(use_log){
        time_stamp += 1;
        log_sge.addr = (uint64_t)(&time_stamp);
        log_sge.length = sizeof(uint32_t);
        log_sge.lkey = 0;
        log_wr.wr_id = time_stamp;
        log_wr.sg_list = &log_sge;
        log_wr.num_sge = 1;
        log_wr.next = NULL;
        log_wr.opcode = IBV_WR_RDMA_WRITE;
        log_wr.send_flags = IBV_SEND_SIGNALED | IBV_SEND_INLINE;
        log_wr.wr.rdma.remote_addr = context_->log_addr_persist + work_queue_finished_ * sizeof(uint32_t);
        log_wr.wr.rdma.rkey = context_->log_rkey_persist;
    }        
    struct ibv_sge sge;
    sge.addr = (uint64_t)local_addr;
    sge.length = length;
    sge.lkey = context_->get_lkey();

    struct ibv_send_wr send_wr = {};
    struct ibv_send_wr *bad_send_wr;
    send_wr.wr_id = time_stamp;
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
    if(context_->down_primary()){
        // downed = true;
        printf("down before write\n");
    }
    ibv_qp* qp = context_->get_qp();
    if (ibv_post_send(qp, &send_wr, &bad_send_wr)) {
        perror("Error, ibv_post_send failed");
        return SEND_ERROR;
    }
    if(context_->down_primary()){
        // downed = true;
        printf("down after write\n");
    }
    work_queue_[work_queue_finished_].wr_timestamp_ = time_stamp;
    work_queue_[work_queue_finished_].wr_local_addr_ = (uint64_t)local_addr;
    work_queue_[work_queue_finished_].wr_length_ = length;
    work_queue_[work_queue_finished_].wr_remote_addr_ = (uint64_t)remote_addr;
    work_queue_[work_queue_finished_].wr_rkey_ = rkey;
    work_queue_[work_queue_finished_].wr_optype_ = WRITE;
    work_queue_finished_ = (work_queue_finished_ + 1)%wr_buffer_size;

    auto start = TIME_NOW;
    ErrorType ret = RECIEVE_ERROR;
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
            ret = NO_ERROR;
            int start_ = work_queue_start_;
            int end_ = work_queue_finished_;
            if(start_ > end_)
                end_ += wr_buffer_size;
            if(start_ != end_){
                for(int i = start_; i < end_; i++){
                    if(work_queue_[i%wr_buffer_size].wr_timestamp_ == wc.wr_id){
                        work_queue_[i%wr_buffer_size].wr_length_ = 0;
                    }
                    if(work_queue_[i%wr_buffer_size].wr_length_ == 0 && i%wr_buffer_size == work_queue_start_){
                        work_queue_start_ = (work_queue_start_ + 1)%wr_buffer_size;
                    }
                }
            }
            break;
        }
    }

    return ret;
}

ErrorType vQP::read_backup(void* local_addr, uint64_t length, void* remote_addr, uint32_t rkey, uint32_t lid, uint32_t dct_num) {
    // printf("%u %lu %lu %lu %lu %u %u\n", rkey, context_->gid1, context_->gid2, 
    //     context_->interface, context_->subnet,
    //     context_->lid_, context_->dct_num_);
    return context_->get_primary_dynamic()->DynamicRead(local_addr, length, remote_addr, rkey, context_->lid_, context_->dct_num_);
}

ErrorType vQP::write_backup(void* local_addr, uint64_t length, void* remote_addr, uint32_t rkey, uint32_t lid, uint32_t dct_num) {
    return context_->get_primary_dynamic()->DynamicWrite(local_addr, length, remote_addr, rkey, context_->lid_, context_->dct_num_);
}

int vQP::alloc_RPC(uint64_t* addr, uint32_t* rkey, uint64_t size) {
    uint64_t remote_addr; uint32_t remote_rkey;
    context_->alloc_RPC(&remote_addr, &remote_rkey, size);
    *addr = remote_addr;
    *rkey = remote_rkey;
    return 0;
}

}