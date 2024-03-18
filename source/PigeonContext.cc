
#include "PigeonCommon.h"
#include "PigeonContext.h"

namespace rdmanager{

PigeonContext::PigeonContext(ibv_context* context) {
    context_ = context;
    name_.assign(context->device->name);
    pd_ = ibv_alloc_pd(context_);
    channel_ = rdma_create_event_channel();
    int result = rdma_create_id(channel_, &cm_id_, context_, RDMA_PS_TCP);
    status_ = PigeonStatus::PIGEON_STATUS_INIT;
    assert(result == 0);
    assert(pd_ != NULL);
    assert(channel_ != NULL);
}

void PigeonContext::PigeonConnect(const std::string ip, const std::string port) {

    addrinfo *res;
    int result = getaddrinfo(ip.c_str(), port.c_str(), NULL, &res);
    assert(result == 0);

    addrinfo *t = NULL;
    for(t = res; t; t = t->ai_next) {
        if(!rdma_resolve_addr(cm_id_, NULL, t->ai_addr, RESOLVE_TIMEOUT_MS)) {
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
    pigeon_debug("device %s listen on %s:%s\n", name_.c_str(), ip.c_str(), port.c_str());
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
    ibv_mr* mr = ibv_reg_mr(pd_, addr, length, IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_WRITE);
    pigeon_debug("device %s register memory %p, length %lu, rkey %u\n", name_.c_str(), addr, length, mr->rkey);
    mr_ = mr;
    assert(mr != NULL);
    return;
}

}