
#include <vQP.h>

namespace rdmanager {

int vQP::read(void* local_addr, uint64_t length, void* remote_addr, uint32_t rkey) {
    struct ibv_sge sge;
    sge.addr = (uint64_t)local_addr;
    sge.length = length;
    sge.lkey = mapper_->get_map_lkey();

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

    ibv_qp* qp = mapper_->get_map_qp();
    if (ibv_post_send(qp, &send_wr, &bad_send_wr)) {
        std::cerr << "Error, ibv_post_send failed" << std::endl;
        return -1;
    }

    auto start = TIME_NOW;
    int ret = -1;
    struct ibv_wc wc;
    ibv_cq* cq = mapper_->get_map_cq();
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

int vQP::write(void* local_addr, uint64_t length, void* remote_addr, uint32_t rkey) {
    struct ibv_sge sge;
    sge.addr = (uint64_t)local_addr;
    sge.length = length;
    sge.lkey = mapper_->get_map_lkey();

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

    ibv_qp* qp = mapper_->get_map_qp();
    if (ibv_post_send(qp, &send_wr, &bad_send_wr)) {
        std::cerr << "Error, ibv_post_send failed" << std::endl;
        return -1;
    }

    auto start = TIME_NOW;
    int ret = -1;
    struct ibv_wc wc;
    ibv_cq* cq = mapper_->get_map_cq();
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

}