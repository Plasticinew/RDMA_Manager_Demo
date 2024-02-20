
#include <algorithm>
#include <string>
#include <vector>
#include <rdma/rdma_cma.h>
#include <infiniband/verbs.h>

namespace rdma_manager{

class rdma_device{
public:
    rdma_device(ibv_context* context) {
        context_ = context;
        name_.assign(context->device->name);
    }
private:
    std::string name_;
    ibv_context* context_;
};

class dev_manager{
public:
    dev_manager(std::vector<std::string> &skip_device_list) ;

private:
    std::vector<std::string> skip_device_list_;
    std::vector<rdma_device> device_list_ ;

};


}