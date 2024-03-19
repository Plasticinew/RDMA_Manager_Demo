
#include "PigeonCommon.h"

namespace rdmanager{

class PigeonContext{
public:
    PigeonContext(ibv_context* context, PigeonDevice device);

    void PigeonConnect(const std::string ip, const std::string port);
    void PigeonListen(const std::string ip, const std::string port);
    void PigeonAccept(rdma_cm_id* cm_id);
    void PigeonMemoryRegister(void* addr, size_t length);

    ibv_qp* get_qp() {
        return cm_id_->qp;
    }

    ibv_cq* get_cq() {
        return cq_;
    }

    PigeonStatus get_status() {
        return status_;
    }

    std::string get_name() {
        return device_.name;
    }

    std::string get_ip() {
        return device_.ip;
    }

    ibv_mr* get_mr() {
        return mr_;
    }

    void show_device() {
        pigeon_debug("device name: %s\n", device_.name.c_str());
        pigeon_debug("device ip: %s\n", device_.ip.c_str());
    }

private:
    ibv_context* context_;
    ibv_pd* pd_;
    ibv_mr* mr_;
    rdma_event_channel* channel_;
    rdma_cm_id* cm_id_;
    ibv_cq* cq_;
    PigeonStatus status_;
    PigeonDevice device_;
};

}