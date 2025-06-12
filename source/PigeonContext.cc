
#include "PigeonCommon.h"
#include "PigeonContext.h"
#include "Msg.h"

namespace rdmanager{

PigeonContext::PigeonContext(ibv_context* context, PigeonDevice device) {
    context_ = context;
    pd_ = ibv_alloc_pd(context_);
    channel_ = rdma_create_event_channel();
    int result = rdma_create_id(channel_, &cm_id_, NULL, RDMA_PS_TCP);
    status_ = PigeonStatus::PIGEON_STATUS_INIT;
    device_ = device;
    worker_info_ = new WorkerInfo *[MAX_SERVER_WORKER*MAX_SERVER_CLIENT];
    worker_threads_ = new std::thread *[MAX_SERVER_WORKER];
    for (uint32_t i = 0; i < MAX_SERVER_WORKER; i++) {
      worker_info_[i] = nullptr;
      worker_threads_[i] = nullptr;
    }
    stop_ = false;
    assert(result == 0);
    assert(pd_ != NULL);
    assert(channel_ != NULL);
}

bool PigeonContext::PigeonConnect(const std::string ip, const std::string port, uint8_t access_type, uint16_t node) {

    int result;
    addrinfo *t = NULL;
    addrinfo *res;
    while( t == NULL ) {
        while(result != 0)
            result = getaddrinfo(ip.c_str(), port.c_str(), NULL, &res);
        assert(result == 0);
    
        struct sockaddr_in src_addr;   // 设置源地址（指定网卡设备）
        memset(&src_addr, 0, sizeof(src_addr));
        src_addr.sin_family = AF_INET;
        inet_pton(AF_INET, device_.ip.c_str(), &src_addr.sin_addr); // 本地网卡IP地址
        src_addr.sin_port = htons(device_.port);
        result = rdma_bind_addr(cm_id_, (struct sockaddr *)&src_addr);
        assert(result == 0);
        
        for(t = res; t; t = t->ai_next) {
            // if(!rdma_resolve_addr(cm_id_, NULL, t->ai_addr, RESOLVE_TIMEOUT_MS)) {
            if(!rdma_resolve_addr(cm_id_, (struct sockaddr *)&src_addr, t->ai_addr, RESOLVE_TIMEOUT_MS)) {
                break;
            }
        }
        if(t == NULL){
            printf("I cannot find\n");
            return false;
        }
    }
    // assert(t != NULL);

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
    qp_init_attr.cap.max_inline_data = 256;
    qp_init_attr.sq_sig_all = 0;
    result = rdma_create_qp(cm_id_, pd_, &qp_init_attr);
    assert(result == 0);

    CNodeInit init_msg = {node, access_type};
    rdma_conn_param conn_param;
    memset(&conn_param, 0, sizeof(conn_param));
    conn_param.responder_resources = 16;
    conn_param.initiator_depth = 16;
    conn_param.private_data = &init_msg;
    conn_param.private_data_len = sizeof(CNodeInit);
    conn_param.retry_count = 7;
    conn_param.rnr_retry_count = 7;
    while(event->event != RDMA_CM_EVENT_ESTABLISHED){
        result = rdma_connect(cm_id_, &conn_param);
        rdma_get_cm_event(channel_, &event);
    }
    // assert(result == 0);
    // assert(event->event == RDMA_CM_EVENT_ESTABLISHED);
    
    struct PData server_pdata;
    memcpy(&server_pdata, event->param.conn.private_data, sizeof(server_pdata));

    server_cmd_msg_ = server_pdata.buf_addr;
    server_cmd_rkey_ = server_pdata.buf_rkey;
    conn_id_ = server_pdata.id;

    gid1 = server_pdata.gid1;
    gid2 = server_pdata.gid2;
    interface = server_pdata.interface;
    subnet = server_pdata.subnet;
    lid_ = server_pdata.lid_;
    dct_num_ = server_pdata.dct_num_;
    
    assert(server_pdata.size == sizeof(CmdMsgBlock));

    cmd_msg_ = new CmdMsgBlock();
    memset(cmd_msg_, 0, sizeof(CmdMsgBlock));
    msg_mr_ = PigeonMemReg((void *)cmd_msg_, sizeof(CmdMsgBlock));
    if (!msg_mr_) {
        perror("ibv_reg_mr m_msg_mr_ fail");
        return false;
    }

    cmd_resp_ = new CmdMsgRespBlock();
    memset(cmd_resp_, 0, sizeof(CmdMsgRespBlock));
    resp_mr_ =
        PigeonMemReg((void *)cmd_resp_, sizeof(CmdMsgRespBlock));
    if (!resp_mr_) {
        perror("ibv_reg_mr m_resp_mr_ fail");
        return false;
    }

    // reg_buf_ = new char[MAX_REMOTE_SIZE];
    // reg_buf_mr_ = PigeonMemReg((void *)reg_buf_, MAX_REMOTE_SIZE);
    // if (!reg_buf_mr_) {
    //     perror("ibv_reg_mr m_reg_buf_mr_ fail");
    //     return;
    // }

    rdma_ack_cm_event(event);

    freeaddrinfo(res);

    // Connect finished
    cq_ = cq;

    return true;
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
            CNodeInit msg = *(CNodeInit*)event->param.conn.private_data;
            rdma_ack_cm_event(event);
            PigeonAccept(new_cm_id, msg.access_type, msg.node_id);
        } else {
            rdma_ack_cm_event(event);
        }
    }
    return;
}

void PigeonContext::PigeonAccept(rdma_cm_id* cm_id, uint8_t connect_type, uint16_t node_id) {
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

       
    struct PData rep_pdata;
    CmdMsgBlock *cmd_msg = nullptr;
    CmdMsgRespBlock *cmd_resp = nullptr;
    struct ibv_mr *msg_mr = nullptr;
    struct ibv_mr *resp_mr = nullptr;
    cmd_msg = new CmdMsgBlock();
    memset(cmd_msg, 0, sizeof(CmdMsgBlock));
    msg_mr = PigeonMemReg((void *)cmd_msg, sizeof(CmdMsgBlock));

    cmd_resp = new CmdMsgRespBlock();
    memset(cmd_resp, 0, sizeof(CmdMsgRespBlock));
    resp_mr = PigeonMemReg((void *)cmd_resp, sizeof(CmdMsgRespBlock));

    if(qp_log_list_[node_id] == NULL){
        qp_log_[node_id] = new CmdMsgBlock();    
        memset(qp_log_[node_id], 0, sizeof(CmdMsgBlock));
        qp_log_list_[node_id] = PigeonMemReg((void *)qp_log_[node_id], sizeof(CmdMsgBlock));
    }

    rep_pdata.id = -1;
    if(connect_type == CONN_RPC){
        int num = worker_num_;
        if (num < MAX_SERVER_WORKER) {
            worker_info_[num] = new WorkerInfo();
            worker_info_[num]->cmd_msg = cmd_msg;
            worker_info_[num]->cmd_resp_msg = cmd_resp;
            worker_info_[num]->msg_mr = msg_mr;
            worker_info_[num]->resp_mr = resp_mr;
            worker_info_[num]->cm_id = cm_id;
            worker_info_[num]->cq = cq;
            worker_threads_[num] =
                new std::thread(&PigeonContext::PigeonWoker, this, worker_info_[num], num);
        } else {
            worker_info_[num] = new WorkerInfo();
            worker_info_[num]->cmd_msg = cmd_msg;
            worker_info_[num]->cmd_resp_msg = cmd_resp;
            worker_info_[num]->msg_mr = msg_mr;
            worker_info_[num]->resp_mr = resp_mr;
            worker_info_[num]->cm_id = cm_id;
            worker_info_[num]->cq = cq;
        } 
        rep_pdata.id = num;
        worker_num_ += 1;
    } 
    if(connect_type == CONN_RPC){
        rep_pdata.buf_addr = (uintptr_t)cmd_msg;
        rep_pdata.buf_rkey = msg_mr->rkey;
    }
    else{
        rep_pdata.buf_addr = (uintptr_t)qp_log_[node_id];
        rep_pdata.buf_rkey = qp_log_list_[node_id]->rkey;
    }
    rep_pdata.size = sizeof(CmdMsgRespBlock);
    rep_pdata.global_rkey_ = 0;
    rep_pdata.gid1 = dynamic_context->gid1;
    rep_pdata.gid2 = dynamic_context->gid2;
    rep_pdata.interface = dynamic_context->interface;
    rep_pdata.subnet = dynamic_context->subnet;
    rep_pdata.lid_ = dynamic_context->lid_;
    rep_pdata.dct_num_ = dynamic_context->dct_num_;


    rdma_conn_param conn_param;
    memset(&conn_param, 0, sizeof(conn_param));
    conn_param.responder_resources = 16;
    conn_param.initiator_depth = 16;
    conn_param.private_data = &rep_pdata;
    conn_param.private_data_len = sizeof(rep_pdata);
    conn_param.retry_count = 7;
    conn_param.rnr_retry_count = 7;
    result = rdma_accept(cm_id, &conn_param);
    assert(result == 0);
    // use RPC ?
    // Accept finished
    cq_ = cq;
    cm_id_ = cm_id;
    status_ = PigeonStatus::PIGEON_STATUS_ACCEPTED;
    return;
}

void PigeonContext::PigeonMemoryRegister(void* addr, size_t length) {
    ibv_mr* mr = ibv_reg_mr(pd_, addr, length, IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_WRITE  | IBV_ACCESS_REMOTE_ATOMIC | IBV_ACCESS_MW_BIND);
    // pigeon_debug("device %s register memory %p, length %lu, rkey %u\n", device_.name.c_str(), addr, length, mr->rkey);
    mr_ = mr;
    assert(mr != NULL);
    return;
}

ibv_mr* PigeonContext::PigeonMemReg(void* addr, size_t length) {
    ibv_mr* mr = ibv_reg_mr(pd_, addr, length, IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_WRITE  | IBV_ACCESS_REMOTE_ATOMIC | IBV_ACCESS_MW_BIND);
    // pigeon_debug("device %s register memory %p, length %lu, rkey %u\n", device_.name.c_str(), addr, length, mr->rkey);
    return mr;
}

ibv_mr* PigeonContext::PigeonMemAllocReg(uint64_t &addr, uint32_t &rkey, size_t length) {
    addr = (uint64_t)mmap(NULL, length, PROT_READ | PROT_WRITE, MAP_PRIVATE |MAP_ANONYMOUS, -1, 0);
    ibv_mr* mr = ibv_reg_mr(pd_, (void*)addr, length, IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_WRITE  | IBV_ACCESS_REMOTE_ATOMIC | IBV_ACCESS_MW_BIND);
    rkey = mr->rkey;
    // pigeon_debug("device %s register memory %p, length %lu, rkey %u\n", device_.name.c_str(), addr, length, mr->rkey);
    if(mr == NULL){
        perror("mr alloc failed");
    }
    return mr;
}

void PigeonContext::PigeonBind(void* addr, uint64_t length, uint32_t &result_rkey){
    uint64_t addr_ = (uint64_t)addr;
    ibv_mw* mw;
    if(mw_pool_.find(addr_)==mw_pool_.end()){
        mw = ibv_alloc_mw(pd_, IBV_MW_TYPE_1);
        printf("addr:%lx rkey_old: %u\n",  addr, mw->rkey);
        mw_pool_[addr_] = mw;
    } else {
        mw = mw_pool_[addr_];
    }
    struct ibv_mw_bind_info bind_info_ = {.mr = mr_, 
                                            .addr = addr_, 
                                            .length = length,
                                            .mw_access_flags = IBV_ACCESS_REMOTE_READ | 
                                                IBV_ACCESS_REMOTE_WRITE } ;
    struct ibv_mw_bind bind_ = {.wr_id = 0, .send_flags = IBV_SEND_SIGNALED, .bind_info = bind_info_};
    if(ibv_bind_mw(cm_id_->qp, mw, &bind_)){
        perror("ibv_post_send mw_bind fail");
    } else {
        while (true) {
            ibv_wc wc;
            int rc = ibv_poll_cq(cq_, 1, &wc);
            if (rc > 0) {
            if (IBV_WC_SUCCESS == wc.status) {
                // mw->rkey = wr_.bind_mw.rkey;
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
    pigeon_debug("device %s bind memory %p, length %lu, rkey %u\n", device_.name.c_str(), addr, length, mw->rkey);
    result_rkey = mw->rkey;
    return;
}


void PigeonContext::PigeonWrite(ibv_qp* qp, ibv_cq* cq, uint64_t local_addr,
    uint32_t lkey, uint32_t length,
    uint64_t remote_addr, uint32_t rkey){
    struct ibv_sge sge;
    sge.addr = (uintptr_t)local_addr;
    sge.length = length;
    sge.lkey = lkey;

    struct ibv_send_wr send_wr = {};
    struct ibv_send_wr *bad_send_wr;
    send_wr.wr_id = 0;
    send_wr.num_sge = 1;
    send_wr.next = NULL;
    send_wr.opcode = IBV_WR_RDMA_WRITE;
    send_wr.sg_list = &sge;
    send_wr.send_flags = IBV_SEND_SIGNALED;
    send_wr.wr.rdma.remote_addr = remote_addr;
    send_wr.wr.rdma.rkey = rkey;
    int error_code;
    if (error_code = ibv_post_send(qp, &send_wr, &bad_send_wr)) {
        perror("ibv_post_send write fail");
        printf("error code %d\n", error_code);
        return;
    }

    auto start = TIME_NOW;
    struct ibv_wc wc;
    int ret = -1;
    while (true) {
        if (TIME_DURATION_US(start, TIME_NOW) > RDMA_TIMEOUT_US) {
        perror("remote write timeout");
        return;
        }
        int rc = ibv_poll_cq(cq, 1, &wc);
        if (rc > 0) {
        if (IBV_WC_SUCCESS == wc.status) {
            ret = 0;
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
    return ;
}

void PigeonContext::PigeonRPCAlloc(uint64_t* addr, uint32_t* rkey, size_t size) {
    memset(cmd_msg_, 0, sizeof(CmdMsgBlock));
    memset(cmd_resp_, 0, sizeof(CmdMsgRespBlock));
    cmd_resp_->notify = NOTIFY_IDLE;
    RegisterRequest *request = (RegisterRequest *)cmd_msg_;
    request->resp_addr = (uint64_t)cmd_resp_;
    request->resp_rkey = resp_mr_->rkey;
    request->id = conn_id_;
    request->type = MSG_REGISTER;
    request->size = size;
    cmd_msg_->notify = NOTIFY_WORK;
  
    /* send a request to sever */
    PigeonWrite(cm_id_->qp, cq_, (uint64_t)cmd_msg_, msg_mr_->lkey,
                                sizeof(CmdMsgBlock), server_cmd_msg_,
                                server_cmd_rkey_);
    
    /* wait for response */
    auto start = TIME_NOW;
    while (cmd_resp_->notify == NOTIFY_IDLE) {
      if (TIME_DURATION_US(start, TIME_NOW) > RDMA_TIMEOUT_US*1000) {
        printf("wait for request completion timeout\n");
        return;
      }
    }
    RegisterResponse *resp_msg = (RegisterResponse *)cmd_resp_;
    if (resp_msg->status != RES_OK) {
      printf("register remote memory fail\n");
      return ;
    }
    *addr = resp_msg->addr;
    *rkey = resp_msg->rkey;
    return;
}

void PigeonContext::PigeonWoker(WorkerInfo *work_info, uint32_t num){
    printf("start worker %d\n", num);
    CmdMsgBlock *cmd_msg = work_info->cmd_msg;
    CmdMsgRespBlock *cmd_resp = work_info->cmd_resp_msg;
    struct ibv_mr *resp_mr = work_info->resp_mr;
    cmd_resp->notify = NOTIFY_WORK;
    int active_id = -1;
    int record = num;
    while (true) {
        if (stop_) break;
        for (int i = record; i < worker_num_; i+=MAX_SERVER_WORKER) {
            if (worker_info_[i]->cmd_msg->notify != NOTIFY_IDLE){
                active_id = i;
                cmd_msg = worker_info_[i]->cmd_msg;
                record = i + MAX_SERVER_WORKER;
                break;
            }
        }
        if (active_id == -1) {
            record = num;
            continue;
        }
        cmd_msg->notify = NOTIFY_IDLE;
        RequestsMsg *request = (RequestsMsg *)cmd_msg;
        if(active_id != request->id) {
            printf("find %d, receive from id:%d\n", active_id, request->id);
        }
        assert(active_id == request->id);
        work_info = worker_info_[request->id];
        cmd_resp = work_info->cmd_resp_msg;
        memset(cmd_resp, 0, sizeof(CmdMsgRespBlock));
        resp_mr = work_info->resp_mr;
        cmd_resp->notify = NOTIFY_WORK;
        active_id = -1;
        if (request->type == MSG_REGISTER) {
            /* handle memory register requests */
            RegisterRequest *reg_req = (RegisterRequest *)request;
            RegisterResponse *resp_msg = (RegisterResponse *)cmd_resp;
            if (PigeonMemAllocReg(resp_msg->addr, resp_msg->rkey,
                                            reg_req->size) == NULL) {
                resp_msg->status = RES_FAIL;
            } else {
                resp_msg->status = RES_OK;
            }
            /* write response */
            // printf("%lx %u\n", resp_msg->addr, resp_msg->rkey);
            // resp_msg->addr = resp_msg->addr; resp_msg->rkey = 0;
            PigeonWrite(work_info->cm_id->qp, work_info->cq, (uint64_t)cmd_resp, resp_mr->lkey,
                        sizeof(CmdMsgRespBlock), reg_req->resp_addr,
                        reg_req->resp_rkey);
        } else if (request->type == MSG_FETCH_FAST) {
           
        } else if (request->type == MSG_FREE_FAST) {
            
        } else if (request->type == MSG_MW_REBIND) {
            
        } else if (request->type == RPC_FUSEE_SUBTABLE){
            
        } else if (request->type == MSG_UNREGISTER) {
            
        } else if(request->type == MSG_PRINT_INFO){
           
        } else if(request->type == MSG_MW_BATCH){
            
        } else {
            printf("wrong request type\n");
        }
    }
}

}