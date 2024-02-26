
#include <vector>
#include <string>
#include <netdb.h>
#include <cassert>
#include <rdma/rdma_cma.h>
#include <infiniband/verbs.h>
#include "rdma_device.h"

namespace rdmanager{

class RDMAConn {

public:
    RDMAConn() {};

    int add_connection(rdma_cm_id* cm_id) {
        connection_queue_.push_back(cm_id);
        return 0;
    }

private:
    
    std::vector<rdma_cm_id*> connection_queue_;


};


}