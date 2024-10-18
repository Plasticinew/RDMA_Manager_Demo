
#include <vQP.h>

namespace rdmanager {

int vQP::read(void* local_addr, uint64_t length, void* remote_addr, uint32_t rkey, uint32_t lid, uint32_t dct_num) {
    if(context_->connected())
        return read_main(local_addr, length, remote_addr, rkey);
    else{
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
    send_wr.opcode = IBV_WR_RDMA_WRITE;
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


}