
#pragma once
#include <infiniband/verbs.h>
#include <rdma/rdma_cma.h>
#include <cassert>
#include <string>

namespace rdmanager{

class RDMADevice{
public:
    RDMADevice(ibv_context* context) {
        context_ = context;
        name_.assign(context->device->name);
        pd_ = ibv_alloc_pd(context_);
        channel_ = rdma_create_event_channel();
        int result = rdma_create_id(channel_, &cm_id_, context_, RDMA_PS_TCP);
        assert(result == 0);
        assert(pd_ != NULL);
        assert(channel_ != NULL);

    }
    std::string get_name() {return name_;}
    ibv_context* get_context() {return context_;}
    ibv_pd* get_pd() {return pd_;}
    rdma_event_channel* get_channel() {return channel_;}
    rdma_cm_id* get_cm_id() {return cm_id_;}

private:
    std::string name_;
    ibv_context* context_;
    ibv_pd* pd_;
    rdma_event_channel* channel_;
    rdma_cm_id* cm_id_;
};

}