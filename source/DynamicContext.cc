
#include "PigeonCommon.h"
#include "DynamicContext.h"

namespace rdmanager{

DynamicContext::DynamicContext(ibv_context* context, PigeonDevice device, ibv_pd* pd) {
    context_ = context;
    pd_ = pd;
    status_ = PigeonStatus::PIGEON_STATUS_INIT;
    device_ = device;
    assert(pd_ != NULL);
    ah_ = NULL;
}

void DynamicContext::DynamicConnect() {
    struct mlx5dv_qp_init_attr dv_init_attr;
    struct ibv_qp_init_attr_ex init_attr;
    
    memset(&dv_init_attr, 0, sizeof(dv_init_attr));
    memset(&init_attr, 0, sizeof(init_attr));
    
    cq_ = ibv_create_cq(context_, 128, NULL, NULL, 0);

    // 和正常QP创建相同，需要设置参数
    init_attr.qp_type = IBV_QPT_DRIVER;
    init_attr.send_cq = cq_;
    init_attr.recv_cq = cq_;
    init_attr.pd = pd_; 
    init_attr.cap.max_send_wr = 512;
    init_attr.cap.max_send_sge = 16;
    init_attr.sq_sig_all = 0;

    // DCQP需要额外设置的内容
    init_attr.comp_mask |= IBV_QP_INIT_ATTR_SEND_OPS_FLAGS | IBV_QP_INIT_ATTR_PD ;
    init_attr.send_ops_flags |= IBV_QP_EX_WITH_SEND | IBV_QP_EX_WITH_RDMA_READ | IBV_QP_EX_WITH_RDMA_WRITE;
 
    dv_init_attr.comp_mask |=
                MLX5DV_QP_INIT_ATTR_MASK_DC |
                MLX5DV_QP_INIT_ATTR_MASK_QP_CREATE_FLAGS;
    dv_init_attr.create_flags |=
                MLX5DV_QP_CREATE_DISABLE_SCATTER_TO_CQE;
    
    // 类型为发送端DCI
    dv_init_attr.dc_init_attr.dc_type = MLX5DV_DCTYPE_DCI;

    qp_ = mlx5dv_create_qp(context_, &init_attr, &dv_init_attr);

    if(qp_ == NULL) {
        perror("create dcqp failed!");
    }

    struct ibv_qp_attr         qp_attr_to_init;
    struct ibv_qp_attr         qp_attr_to_rtr;
    struct ibv_qp_attr         qp_attr_to_rts;

    memset(&qp_attr_to_init, 0, sizeof(qp_attr_to_init));
    memset(&qp_attr_to_rtr, 0, sizeof(qp_attr_to_rtr));
    memset(&qp_attr_to_rts, 0, sizeof(qp_attr_to_rts));

    // 状态切换为INIT
    qp_attr_to_init.qp_state   = IBV_QPS_INIT;
    qp_attr_to_init.pkey_index = 0;
    qp_attr_to_init.port_num   = 1;
    
    // 状态切换为RTR
    qp_attr_to_rtr.qp_state          = IBV_QPS_RTR;
    qp_attr_to_rtr.path_mtu          = IBV_MTU_4096;
    qp_attr_to_rtr.min_rnr_timer     = 7;
    qp_attr_to_rtr.ah_attr.port_num  = 1;
    qp_attr_to_rtr.ah_attr.is_global = 1;

    // 状态切换为RTS
    qp_attr_to_rts.qp_state      = IBV_QPS_RTS;
    qp_attr_to_rts.timeout       = 14;
    qp_attr_to_rts.retry_cnt     = 7;
    qp_attr_to_rts.rnr_retry     = 7;
    qp_attr_to_rts.sq_psn        = 114;
    qp_attr_to_rts.max_rd_atomic = 1;

    int ret = ibv_modify_qp(qp_, &qp_attr_to_init, IBV_QP_STATE | IBV_QP_PKEY_INDEX | IBV_QP_PORT);
    if (ret) {
        printf("%d\n", ret);
        perror("init state failed\n");
        abort();
    }

    ret = ibv_modify_qp(qp_, &qp_attr_to_rtr, IBV_QP_STATE | IBV_QP_PATH_MTU | IBV_QP_AV);
    if (ret) {
        printf("%d\n", ret);
        perror("rtr state failed\n");
        abort();
    }
    
    ret = ibv_modify_qp(qp_, &qp_attr_to_rts, IBV_QP_STATE | IBV_QP_TIMEOUT | IBV_QP_RETRY_CNT |
                      IBV_QP_RNR_RETRY | IBV_QP_SQ_PSN |
                      IBV_QP_MAX_QP_RD_ATOMIC | IBV_QP_MIN_RNR_TIMER);
    if (ret) {
        printf("%d\n", ret);
        perror("rts state failed\n");
        abort();
    }

    struct ibv_qp_attr attr;
    struct ibv_qp_init_attr init_attr_;
    
    // 获取qp的信息，主要是dct_num
    // lid和port在IB网络下有效，RoCE网络下不会用到
    ibv_query_qp(qp_, &attr,
            IBV_QP_STATE, &init_attr_);

    lid_ = attr.ah_attr.dlid;
    port_num_ = attr.ah_attr.port_num;
    dct_num_ = qp_->qp_num;

    printf("%d, %d, %d, %d\n", attr.qp_state ,lid_, port_num_, dct_num_);

    // 提取ex qp与mlx qp
    qp_ex_ = ibv_qp_to_qp_ex(qp_);

    qp_mlx_ex_ = mlx5dv_qp_ex_from_ibv_qp_ex(qp_ex_);
    
    return;
}

void DynamicContext::DynamicListen() {
    struct mlx5dv_qp_init_attr dv_init_attr;
    struct ibv_qp_init_attr_ex init_attr;
    
    memset(&dv_init_attr, 0, sizeof(dv_init_attr));
    memset(&init_attr, 0, sizeof(init_attr));
    
    cq_ = ibv_create_cq(context_, 1024, NULL, NULL, 0);
    struct ibv_srq_init_attr srq_init_attr;
 
    memset(&srq_init_attr, 0, sizeof(srq_init_attr));
 
    srq_init_attr.attr.max_wr  = 1;
    srq_init_attr.attr.max_sge = 1;
 
    srq_ = ibv_create_srq(pd_, &srq_init_attr);

    init_attr.qp_type = IBV_QPT_DRIVER;
    init_attr.send_cq = cq_;
    init_attr.recv_cq = cq_;
    init_attr.pd = pd_; 
    init_attr.cap.max_send_wr = 1;
    init_attr.cap.max_recv_wr = 1;
    init_attr.cap.max_send_sge = 1;
    init_attr.cap.max_recv_sge = 1;
    init_attr.cap.max_inline_data = 256;
    init_attr.sq_sig_all = 0;

    init_attr.comp_mask |= IBV_QP_INIT_ATTR_PD;
    init_attr.srq = srq_;
    dv_init_attr.comp_mask = MLX5DV_QP_INIT_ATTR_MASK_DC;
    // 类型为接收端DCT
    dv_init_attr.dc_init_attr.dc_type = MLX5DV_DCTYPE_DCT;
    // 需要设置访问的key
    dv_init_attr.dc_init_attr.dct_access_key = 114514;

    qp_ = mlx5dv_create_qp(context_, &init_attr, &dv_init_attr);

    if(qp_ == NULL) {
        perror("create dcqp failed!");
    }

    // 切换为INIT
    ibv_qp_attr qp_attr{};
    qp_attr.qp_state = IBV_QPS_INIT;
    qp_attr.pkey_index = 0;
    qp_attr.port_num = 1;
    qp_attr.qp_access_flags = IBV_ACCESS_REMOTE_WRITE |
                                IBV_ACCESS_REMOTE_READ; 

    int attr_mask =
        IBV_QP_STATE | IBV_QP_ACCESS_FLAGS | IBV_QP_PORT |IBV_QP_PKEY_INDEX;

    int ret = ibv_modify_qp(qp_, &qp_attr, attr_mask);
    if (ret) {
        printf("%d\n", ret);
        perror("change state failed\n");
        abort();
    }

    // 切换为RTR
    qp_attr.qp_state = IBV_QPS_RTR;
    qp_attr.path_mtu = IBV_MTU_4096;
    qp_attr.min_rnr_timer = 7;
    qp_attr.ah_attr.is_global = 1;
    qp_attr.ah_attr.grh.hop_limit = 1;
    qp_attr.ah_attr.grh.traffic_class = 0;
    qp_attr.ah_attr.grh.sgid_index = 1;
    qp_attr.ah_attr.port_num = 1;

    attr_mask = IBV_QP_STATE | IBV_QP_MIN_RNR_TIMER | IBV_QP_AV | IBV_QP_PATH_MTU;

    ret = ibv_modify_qp(qp_, &qp_attr, attr_mask);
    if (ret) {
        printf("%d\n", ret);
        perror("change state failed\n");
        abort();
    }

    struct ibv_port_attr port_attr;

	memset(&port_attr, 0, sizeof(port_attr));

    // 查询dct_num与gid
	ibv_query_port(context_, 1,
		&port_attr);

    lid_ = port_attr.lid;
    port_num_ = 1;

    ibv_gid gid;
    ibv_query_gid(context_, 1, 1, &gid);

    dct_num_ = qp_->qp_num;

    // dct_num_ = qp_->qp_num;

    // gid未来会设计一个metadata server来存储与共享
    // 这里选择了明文存储进行测试
    // 主要有两项：interface_id和subnet_prefix
    // raw是相同的内容
    printf("%u, %u, %u\n", lid_, port_num_, dct_num_);
    // struct timeval timeout = { 1, 500000 }; // 1.5 seconds
    // redisContext *redis_conn;
    // redisReply *redis_reply;
    // redis_conn = redisConnectWithTimeout("10.10.1.2", 2222, timeout);
    // // redis_reply = (redisReply*)redisCommand(redis_conn,"SET bench_start 0");
    // // freeReplyObject(redis_reply);
    // if (redis_conn == NULL || redis_conn->err) {
    //     if (redis_conn) {
    //         printf("Connection error: %s\n", redis_conn->errstr);
    //         redisFree(redis_conn);
    //     } else {
    //         printf("Connection error: can't allocate redis context\n");
    //     }
    //     exit(1);
    // }
    // std::string node_name = std::to_string();
    // redis_reply = (redisReply*)redisCommand(redis_conn, "SET node1_lid_ %s"), "node"+.c_str(), std::to_string(lid_).c_str();
    // freeReplyObject(redis_reply);
    // redis_reply = (redisReply*)redisCommand(redis_conn, "SET node1_dct_num_ %s"), std::to_string(dct_num_).c_str();
    // freeReplyObject(redis_reply);
    printf("%lu, %lu, %lu, %lu\n", *(uint64_t*)gid.raw, *((uint64_t*)(gid.raw)+1), gid.global.interface_id, gid.global.subnet_prefix);
    gid1 = *(uint64_t*)gid.raw;
    gid2 = *((uint64_t*)(gid.raw)+1);
    interface = gid.global.interface_id;
    subnet = gid.global.subnet_prefix;
    // redis_reply = (redisReply*)redisCommand(redis_conn, "SET node1_lid_ %s"), std::to_string(lid_).c_str();

    return;
}

void DynamicContext::CreateAh(uint64_t gid1, uint64_t gid2, uint64_t interface, uint64_t subnet, uint32_t lid){
    if(ah_ == NULL){
        ibv_gid gid;
        // 这里采用了明文存储
        // 在新环境下测试时需要根据目标server打印的gid进行更新
        // 未来会替换为从metadata server中获取
        *(uint64_t*)gid.raw = gid1;
        *((uint64_t*)(gid.raw)+1) = gid2;
        gid.global.interface_id = interface;
        gid.global.subnet_prefix = subnet;
        struct ibv_ah_attr ah_attr;
        ah_attr.dlid = lid;
        ah_attr.port_num = 1;
        ah_attr.is_global = 1;
        ah_attr.grh.hop_limit = 1;
        ah_attr.grh.sgid_index = 1;
        ah_attr.grh.dgid = gid;
        // ah_attr.sl = 1;
        // ah_attr.src_path_bits = 0;
        
        ah_ = ibv_create_ah(pd_, &ah_attr);
        if (!ah_) {
            perror("create ah failed!");
            return ;
        }
        printf("create ah success\n");
    }
}


ErrorType DynamicContext::DynamicRead(void* local_addr, uint64_t length, void* remote_addr, uint32_t rkey, uint32_t lid, uint32_t dct_num){
    // 对于同一个目标网卡，ah是可以复用的
    if(ah_ == NULL) {
        ibv_gid gid;
        // 这里采用了明文存储
        // 在新环境下测试时需要根据目标server打印的gid进行更新
        // 未来会替换为从metadata server中获取
        *(uint64_t*)gid.raw = (uint64_t)33022;
        *((uint64_t*)(gid.raw)+1) = (uint64_t)7105540014128381702;
        gid.global.interface_id = 7105540014128381702;
        gid.global.subnet_prefix = 33022;
        struct ibv_ah_attr ah_attr;
        ah_attr.dlid = lid;
        ah_attr.port_num = 1;
        ah_attr.is_global = 1;
        ah_attr.grh.hop_limit = 1;
        ah_attr.grh.sgid_index = 1;
        ah_attr.grh.dgid = gid;
        // ah_attr.sl = 1;
        // ah_attr.src_path_bits = 0;
        
        ah_ = ibv_create_ah(pd_, &ah_attr);
        if (!ah_) {
            perror("create ah failed!");
            return SEND_ERROR;
        }
    }
    ibv_wr_start(qp_ex_);
    qp_ex_->wr_id = 0;
    qp_ex_->wr_flags = IBV_SEND_SIGNALED;
    ibv_wr_rdma_read(qp_ex_, rkey, (uint64_t)remote_addr);
    ibv_wr_set_sge(qp_ex_, mr_->lkey, (uint64_t)local_addr, length);
    mlx5dv_wr_set_dc_addr(qp_mlx_ex_, ah_, dct_num, 114514);
    ibv_wr_complete(qp_ex_);
    auto start = TIME_NOW;
    struct ibv_wc wc;
    // 与正常读写相同，需要调用poll_cq来确认操作完成
    while(true) {
        if(TIME_DURATION_US(start, TIME_NOW) > RDMA_TIMEOUT_US) {
            std::cerr << "Error, read timeout" << std::endl;
            break;
        }
        if(ibv_poll_cq(cq_, 1, &wc) > 0) {
            if(wc.status != IBV_WC_SUCCESS) {
                std::cerr << "Error, read failed: " << wc.status << std::endl;
                break;
            }
            break;        
        }
    }
    return NO_ERROR;
}

// Write与Read类似
ErrorType DynamicContext::DynamicWrite(void* local_addr, uint64_t length, void* remote_addr, uint32_t rkey, uint32_t lid, uint32_t dct_num){
    if(ah_ == NULL) {
        ibv_gid gid;
        *(uint64_t*)gid.raw = (uint64_t)33022;
        *((uint64_t*)(gid.raw)+1) = (uint64_t)4934949029753786626;
        gid.global.interface_id = 4934949029753786626;
        gid.global.subnet_prefix = 33022;
        struct ibv_ah_attr ah_attr;
        ah_attr.dlid = lid;
        ah_attr.port_num = 1;
        ah_attr.is_global = 1;
        ah_attr.grh.hop_limit = 1;
        ah_attr.grh.sgid_index = 1;
        ah_attr.grh.dgid = gid;
        // ah_attr.sl = 1;
        // ah_attr.src_path_bits = 0;
        
        ah_ = ibv_create_ah(pd_, &ah_attr);
        if (!ah_) {
            perror("create ah failed!");
            return SEND_ERROR;
        }
    }
    ibv_wr_start(qp_ex_);
    qp_ex_->wr_id = 0;
    qp_ex_->wr_flags = IBV_SEND_SIGNALED;
    ibv_wr_rdma_write(qp_ex_, rkey, (uint64_t)remote_addr);
    ibv_wr_set_sge(qp_ex_, mr_->lkey, (uint64_t)local_addr, length);
    mlx5dv_wr_set_dc_addr(qp_mlx_ex_, ah_, dct_num, 114514);
    ibv_wr_complete(qp_ex_);
    auto start = TIME_NOW;
    struct ibv_wc wc;
    while(true) {
        if(TIME_DURATION_US(start, TIME_NOW) > RDMA_TIMEOUT_US) {
            std::cerr << "Error, read timeout" << std::endl;
            break;
        }
        if(ibv_poll_cq(cq_, 1, &wc) > 0) {
            if(wc.status != IBV_WC_SUCCESS) {
                std::cerr << "Error, read failed: " << wc.status << std::endl;
                break;
            }
            break;        
        }
    }
    return NO_ERROR;
}

void DynamicContext::PigeonMemoryRegister(void* addr, size_t length) {
    ibv_mr* mr = ibv_reg_mr(pd_, addr, length, IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_WRITE  | IBV_ACCESS_REMOTE_ATOMIC | IBV_ACCESS_MW_BIND);
    // pigeon_debug("device %s register memory %p, length %lu, rkey %u\n", device_.name.c_str(), addr, length, mr->rkey);
    mr_ = mr;
    assert(mr != NULL);
    return;
}

// Bind内存的过程与RCQP相同
void DynamicContext::PigeonBind(void* addr, uint64_t length, uint32_t &result_rkey){
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
    if(ibv_bind_mw(qp_, mw, &bind_)){
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

}