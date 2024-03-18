
#include "PigeonCommon.h"

namespace rdmanager{

class PigeonContext{
public:
    PigeonContext(ibv_context* context) ;

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
        return name_;
    }

    ibv_mr* get_mr() {
        return mr_;
    }
private:
    std::string name_;
    ibv_context* context_;
    ibv_pd* pd_;
    ibv_mr* mr_;
    rdma_event_channel* channel_;
    rdma_cm_id* cm_id_;
    ibv_cq* cq_;
    PigeonStatus status_;
};

}