
#include <string>
#include <vector>
#include <rdma/rdma_cma.h>
#include <infiniband/verbs.h>
#include <thread>

namespace rdmanager{

class RDMAServer {
public:
    RDMAServer() {};
    ~RDMAServer() {};

    int add_listen(std::thread* listen_thread) {
        listen_thread_.push_back(listen_thread);
        return 0;
    }

    int add_connection(rdma_cm_id* cm_id) {
        connection_queue_.push_back(cm_id);
        return 0;
    } 

private:
    std::vector<rdma_cm_id*> connection_queue_;
    std::vector<std::thread*> listen_thread_;

};

}