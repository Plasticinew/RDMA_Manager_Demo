
#include "PigeonCommon.h"
#include "PigeonContext.h"

namespace rdmanager{

PigeonContext::PigeonContext(ibv_context* context, PigeonDevice device) {
    context_ = context;
    pd_ = ibv_alloc_pd(context_);
    channel_ = rdma_create_event_channel();
    int result = rdma_create_id(channel_, &cm_id_, NULL, RDMA_PS_TCP);
    status_ = PigeonStatus::PIGEON_STATUS_INIT;
    device_ = device;
    assert(result == 0);
    assert(pd_ != NULL);
    assert(channel_ != NULL);
}

void PigeonContext::PigeonConnect(const std::string ip, const std::string port) {

    addrinfo *res;
    int result = getaddrinfo(ip.c_str(), port.c_str(), NULL, &res);
    assert(result == 0);

    struct sockaddr_in src_addr;   // 设置源地址（指定网卡设备）
    memset(&src_addr, 0, sizeof(src_addr));
    src_addr.sin_family = AF_INET;
    inet_pton(AF_INET, device_.ip.c_str(), &src_addr.sin_addr); // 本地网卡IP地址


    addrinfo *t = NULL;
    
    for(t = res; t; t = t->ai_next) {
        if(!rdma_resolve_addr(cm_id_, (struct sockaddr *)&src_addr, t->ai_addr, RESOLVE_TIMEOUT_MS)) {
            break;
        }
    }
    assert(t != NULL);

    rdma_cm_event* event;
    result = rdma_get_cm_event(channel_, &event);
    assert(result == 0);
    assert(event->event == RDMA_CM_EVENT_ADDR_RESOLVED);
    rdma_ack_cm_event(event);
    // Addr resolve finished, make route resolve

    result = rdma_resolve_route(cm_id_, RESOLVE_TIMEOUT_MS);
    assert(result == 0);
    result = rdma_get_cm_event(channel_, &event);
    assert(result == 0);
    assert(event->event == RDMA_CM_EVENT_ROUTE_RESOLVED);
    rdma_ack_cm_event(event);
    // Addr route resolve finished

    ibv_comp_channel* comp_channel = ibv_create_comp_channel(context_);
    assert(comp_channel != NULL);
    ibv_cq* cq = ibv_create_cq(context_, 1024, NULL, comp_channel, 0);
    assert(cq != NULL);
    result = ibv_req_notify_cq(cq, 0);
    assert(result == 0);

    ibv_qp_init_attr qp_init_attr;
    memset(&qp_init_attr, 0, sizeof(qp_init_attr));
    qp_init_attr.send_cq = cq;
    qp_init_attr.recv_cq = cq;
    qp_init_attr.qp_type = IBV_QPT_RC;
    qp_init_attr.cap.max_send_wr = 512;
    qp_init_attr.cap.max_recv_wr = 1;
    qp_init_attr.cap.max_send_sge = 16;
    qp_init_attr.cap.max_recv_sge = 16;
    qp_init_attr.sq_sig_all = 0;
    result = rdma_create_qp(cm_id_, pd_, &qp_init_attr);
    assert(result == 0);
    rdma_conn_param conn_param;
    memset(&conn_param, 0, sizeof(conn_param));
    conn_param.responder_resources = 16;
    conn_param.initiator_depth = 16;
    conn_param.retry_count = 7;
    conn_param.rnr_retry_count = 7;
    result = rdma_connect(cm_id_, &conn_param);
    assert(result == 0);
    rdma_get_cm_event(channel_, &event);
    assert(event->event == RDMA_CM_EVENT_ESTABLISHED);
    rdma_ack_cm_event(event);
    freeaddrinfo(res);

    // Connect finished
    cq_ = cq;
    status_ = PigeonStatus::PIGEON_STATUS_CONNECTED;
    return;
}

void PigeonContext::PigeonListen(const std::string ip, const std::string port) {
    sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(atoi(port.c_str()));
    sin.sin_addr.s_addr = inet_addr(ip.c_str());
    int result = rdma_bind_addr(cm_id_, (sockaddr*)&sin);
    assert(result == 0);
    result = rdma_listen(cm_id_, 1024);
    assert(result == 0);
    pigeon_debug("device %s listen on %s:%s\n", device_.name.c_str(), ip.c_str(), port.c_str());
    while(true) {
        rdma_cm_event* event;
        int result = rdma_get_cm_event(channel_, &event);
        assert(result == 0);
        if(event->event == RDMA_CM_EVENT_CONNECT_REQUEST) {
            rdma_cm_id* new_cm_id = event->id;
            rdma_ack_cm_event(event);
            PigeonAccept(new_cm_id);
        } else {
            rdma_ack_cm_event(event);
        }
    }
    return;
}

void PigeonContext::PigeonAccept(rdma_cm_id* cm_id) {
    ibv_comp_channel* comp_channel = ibv_create_comp_channel(context_);
    assert(comp_channel != NULL);
    ibv_cq* cq = ibv_create_cq(context_, 1, NULL, comp_channel, 0);
    assert(cq != NULL);
    int result = ibv_req_notify_cq(cq, 0);
    assert(result == 0);
    ibv_qp_init_attr qp_init_attr;
    memset(&qp_init_attr, 0, sizeof(qp_init_attr));
    qp_init_attr.send_cq = cq;
    qp_init_attr.recv_cq = cq;
    qp_init_attr.qp_type = IBV_QPT_RC;    
    qp_init_attr.cap.max_send_wr = 1;
    qp_init_attr.cap.max_recv_wr = 1;
    qp_init_attr.cap.max_send_sge = 1;
    qp_init_attr.cap.max_recv_sge = 1;
    qp_init_attr.cap.max_inline_data = 256;
    qp_init_attr.sq_sig_all = 0;
    result = rdma_create_qp(cm_id, pd_, &qp_init_attr);
    assert(result == 0);
    rdma_conn_param conn_param;
    memset(&conn_param, 0, sizeof(conn_param));
    conn_param.responder_resources = 16;
    conn_param.initiator_depth = 16;
    conn_param.retry_count = 7;
    conn_param.rnr_retry_count = 7;
    result = rdma_accept(cm_id, &conn_param);
    assert(result == 0);

    // Accept finished
    cq_ = cq;
    cm_id_ = cm_id;
    status_ = PigeonStatus::PIGEON_STATUS_ACCEPTED;
    return;
}

void PigeonContext::PigeonMemoryRegister(void* addr, size_t length) {
    ibv_mr* mr = ibv_reg_mr(pd_, addr, length, IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_WRITE  | IBV_ACCESS_REMOTE_ATOMIC | IBV_ACCESS_MW_BIND);
    pigeon_debug("device %s register memory %p, length %lu, rkey %u\n", device_.name.c_str(), addr, length, mr->rkey);
    mr_ = mr;
    assert(mr != NULL);
    return;
}

void PigeonContext::PigeonBind(void* addr, uint64_t length, uint32_t source_rkey, uint32_t &result_rkey){
    uint64_t addr_ = (uint64_t)addr;
    ibv_mw* mw;
    if(mw_pool_.find(addr_)==mw_pool_.end()){
        mw = ibv_alloc_mw(pd_, IBV_MW_TYPE_2);
        printf("addr:%lx rkey_old: %u\n",  addr, mw->rkey);
        mw_pool_[addr_] = mw;
    } else {
        mw = mw_pool_[addr_];
    }
    struct ibv_send_wr wr_ = {};
    struct ibv_send_wr* bad_wr_ = NULL;
    wr_.wr_id = 0;
    wr_.num_sge = 0;
    wr_.next = NULL;
    wr_.opcode = IBV_WR_BIND_MW;
    wr_.sg_list = NULL;
    wr_.send_flags = IBV_SEND_SIGNALED;
    wr_.bind_mw.mw = mw;
    if(source_rkey == 0){
        wr_.bind_mw.rkey = mw->rkey; 
    } else {
        wr_.bind_mw.rkey = ((mw->rkey>>8)<<8) + (source_rkey & 0xff);
    }
    wr_.bind_mw.bind_info.addr = addr_;
    wr_.bind_mw.bind_info.length = length;
    wr_.bind_mw.bind_info.mr = mr_;
    wr_.bind_mw.bind_info.mw_access_flags = IBV_ACCESS_REMOTE_READ | 
                                IBV_ACCESS_REMOTE_WRITE;
    // printf("try to bind with rkey: %d, old_rkey is %d, old mw is %d\n", mw->rkey, mw->rkey, mw->rkey);
    if (ibv_post_send(cm_id_->qp, &wr_, &bad_wr_)) {
        perror("ibv_post_send mw_bind fail");
    } else {
        if(bad_wr_ != NULL) {
            printf("error!\n");
        }
        while (true) {
            ibv_wc wc;
            int rc = ibv_poll_cq(cq_, 1, &wc);
            if (rc > 0) {
            if (IBV_WC_SUCCESS == wc.status) {
                mw->rkey = wr_.bind_mw.rkey;
                // printf("bind success! rkey = %d, mw.rkey = %d \n", wr_.bind_mw.rkey, mw->rkey);
                break;
            } else if (IBV_WC_WR_FLUSH_ERR == wc.status) {
                perror("cmd_send IBV_WC_WR_FLUSH_ERR");
                break;
            } else if (IBV_WC_RNR_RETRY_EXC_ERR == wc.status) {
                perror("cmd_send IBV_WC_RNR_RETRY_EXC_ERR");
                break;
            } else {
                perror("cmd_send ibv_poll_cq status error");
                printf("%d\n", wc.status);
                break;
            }
            } else if (0 == rc) {
            continue;
            } else {
            perror("ibv_poll_cq fail");
            break;
            }
        }
    }
    pigeon_debug("device %s bind memory %p, length %lu, bind rkey %u, rkey %u\n", device_.name.c_str(), addr, length, wr_.bind_mw.rkey, mw->rkey);
    result_rkey = mw->rkey;
    return;
}


void PigeonContext::PigeonUnbind(void* addr){
    uint64_t addr_ = (uint64_t)addr;
    ibv_mw* mw = mw_pool_[addr_];
    struct ibv_send_wr wr_ = {};
    struct ibv_send_wr* bad_wr_ = NULL;
    wr_.wr_id = 0;
    wr_.num_sge = 0;
    wr_.next = NULL;
    wr_.opcode = IBV_WR_LOCAL_INV;
    wr_.sg_list = NULL;
    wr_.send_flags = IBV_SEND_SIGNALED;
    wr_.invalidate_rkey = mw->rkey;
    if (ibv_post_send(cm_id_->qp, &wr_, &bad_wr_)) {
        perror("ibv_post_send mw_bind fail");
    } else {
        if(bad_wr_ != NULL) {
            printf("error!\n");
        }
        while (true) {
            ibv_wc wc;
            int rc = ibv_poll_cq(cq_, 1, &wc);
            if (rc > 0) {
            if (IBV_WC_SUCCESS == wc.status) {
                // printf("bind success! rkey = %d, mw.rkey = %d \n", wr_.bind_mw.rkey, mw->rkey);
                break;
            } else if (IBV_WC_WR_FLUSH_ERR == wc.status) {
                perror("cmd_send IBV_WC_WR_FLUSH_ERR");
                break;
            } else if (IBV_WC_RNR_RETRY_EXC_ERR == wc.status) {
                perror("cmd_send IBV_WC_RNR_RETRY_EXC_ERR");
                break;
            } else {
                perror("cmd_send ibv_poll_cq status error");
                printf("%d\n", wc.status);
                break;
            }
            } else if (0 == rc) {
            continue;
            } else {
            perror("ibv_poll_cq fail");
            break;
            }
        }
    }
    pigeon_debug("device %s invalidate memory %p, rkey %u\n", device_.name.c_str(), addr, mw->rkey);
    return;
}

}