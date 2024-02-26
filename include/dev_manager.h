
#include <algorithm>
#include <string>
#include <vector>
#include <iostream>
#include "rdma_conn.h"
#include "rdma_server.h"
#include <rdma/rdma_cma.h>
#include <infiniband/verbs.h>
#include "rdma_device.h"

namespace rdmanager{

const int RESOLVE_TIMEOUT_MS = 5000;

enum dev_policy{
    main_replica, round_robin, least_loaded 
};

class DevContext{
public:
    DevContext(std::vector<std::string> *skip_device_list, std::vector<std::string> *named_device_list, dev_policy policy) ;

    RDMAConn* create_client_connect(const std::string ip, const std::string port);
    RDMAServer* create_server_listen(const std::string port);

private:

    rdma_cm_id* single_client_connect(RDMADevice* device, const std::string ip, const std::string port);
    int single_server_listen(RDMADevice* device, const std::string port);
    void server_handle_connection(RDMAServer* server, RDMADevice* device);
    int create_server_connect(rdma_cm_id* cm_id, RDMAServer* server, RDMADevice* device);

    std::vector<std::string> skip_device_list_;
    std::vector<std::string> named_device_list_;
    std::vector<RDMADevice> device_list_ ;
    dev_policy policy_;

};


}