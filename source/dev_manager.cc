
#include "dev_manager.h"

namespace rdmanager{

    DevContext::DevContext(std::vector<std::string> *skip_device_list, std::vector<std::string> *named_device_list, dev_policy policy) {
        int device_num;
        struct ibv_context** device_list; 
        
        // get unused device list
        if(skip_device_list != NULL){
            for(auto iter = skip_device_list->begin(); iter != skip_device_list->end(); iter ++) {
                std::string str(*iter);
                skip_device_list_.push_back(str);
            }
        }

        if(named_device_list != NULL){
            for(auto iter = named_device_list->begin(); iter != named_device_list->end(); iter ++) {
                std::string str(*iter);
                named_device_list_.push_back(str);
            }
        }

        // search all avaliable rdma device
        device_list = rdma_get_devices(&device_num);
        for(int i = 0; i < device_num; i ++) {
            if(named_device_list_.size() == 0) {
                if(skip_device_list_.end() == 
                    std::find(skip_device_list_.begin(), skip_device_list_.end(), device_list[i]->device->name)) {
                    RDMADevice new_dev(device_list[i]);
                    device_list_.push_back(new_dev);
                }
            } else {
                if(named_device_list_.end() != 
                    std::find(named_device_list_.begin(), named_device_list_.end(), device_list[i]->device->name)) {
                    RDMADevice new_dev(device_list[i]);
                    device_list_.push_back(new_dev);
                }
            }
        }
        
        policy_ = policy;
    }

    rdma_cm_id* single_client_connect(RDMADevice* device, const std::string ip, const std::string port) {
        
        rdma_event_channel* channel = device->get_channel();
        rdma_cm_id* cm_id = device->get_cm_id();

        addrinfo *res;
        int result = getaddrinfo(ip.c_str(), port.c_str(), NULL, &res);
        assert(result == 0);

        addrinfo *t = NULL;
        for(t = res; t; t = t->ai_next) {
            if(!rdma_resolve_addr(cm_id, NULL, t->ai_addr, RESOLVE_TIMEOUT_MS)) {
                break;
            }
        }
        assert(t != NULL);

        rdma_cm_event* event;

        result = rdma_get_cm_event(channel, &event);
        assert(result == 0);
        assert(event->event == RDMA_CM_EVENT_ADDR_RESOLVED);

        rdma_ack_cm_event(event);
        // Addr resolve finished, make route resolve

        result = rdma_resolve_route(cm_id, RESOLVE_TIMEOUT_MS);
        assert(result == 0);

        result = rdma_get_cm_event(channel, &event);
        assert(result == 0);
        assert(event->event == RDMA_CM_EVENT_ROUTE_RESOLVED);

        rdma_ack_cm_event(event);
        // Addr route resolve finished

        ibv_comp_channel* comp_channel = ibv_create_comp_channel(device->get_context());
        assert(comp_channel != NULL);

        ibv_cq* cq = ibv_create_cq(device->get_context(), 1024, NULL, comp_channel, 0);
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

        result = rdma_create_qp(cm_id, device->get_pd(), &qp_init_attr);
        assert(result == 0);

        rdma_conn_param conn_param;
        memset(&conn_param, 0, sizeof(conn_param));
        conn_param.responder_resources = 16;
        conn_param.initiator_depth = 16;
        conn_param.retry_count = 7;
        conn_param.rnr_retry_count = 7;
        result = rdma_connect(cm_id, &conn_param);
        assert(result == 0);

        rdma_get_cm_event(channel, &event);
        assert(event->event == RDMA_CM_EVENT_ESTABLISHED);

        rdma_ack_cm_event(event);

        freeaddrinfo(res);

        return cm_id;
    }

    int single_server_listen(RDMADevice* device, const std::string port) {

        rdma_cm_id* cm_id = device->get_cm_id();
        
        sockaddr_in sin;
        memset(&sin, 0, sizeof(sin));
        sin.sin_family = AF_INET;
        sin.sin_port = htons(atoi(port.c_str()));
        sin.sin_addr.s_addr = INADDR_ANY;

        int result = rdma_bind_addr(cm_id, (sockaddr*)&sin);
        assert(result == 0);

        result = rdma_listen(cm_id, 1024);
        assert(result == 0);

        return 0;
    }

    RDMAConn* DevContext::create_client_connect(const std::string ip, const std::string port) {
        RDMAConn* conn = new RDMAConn();
        for(auto iter = device_list_.begin(); iter != device_list_.end(); iter ++) {
            rdma_cm_id* cm_id = single_client_connect(&(*iter), ip, port);
            conn->add_connection(cm_id);
        }
        return conn;
    }

    RDMAServer* DevContext::create_server_listen(const std::string port) {
        RDMAServer* server = new RDMAServer();
        for(auto iter = device_list_.begin(); iter != device_list_.end(); iter ++) {
            single_server_listen(&(*iter), port);
            std::thread* listen_thread = new std::thread(&DevContext::server_handle_connection, this, server, &(*iter));
            server->add_listen(listen_thread);
        }
        return server;
    }

    void DevContext::server_handle_connection(RDMAServer* server, RDMADevice* device) {
        while(true) {
            rdma_cm_event* event;
            rdma_event_channel* channel = device->get_channel();
            int result = rdma_get_cm_event(channel, &event);
            assert(result == 0);

            if(event->event == RDMA_CM_EVENT_CONNECT_REQUEST) {
                rdma_cm_id* new_cm_id = event->id;
                rdma_ack_cm_event(event);
                create_server_connect(new_cm_id, server, device);
                server->add_connection(new_cm_id);
            } else {
                rdma_ack_cm_event(event);
            }
        }
    }

    int DevContext::create_server_connect(rdma_cm_id* cm_id,RDMAServer* server, RDMADevice* device) {
        
        ibv_comp_channel* comp_channel = ibv_create_comp_channel(device->get_context());
        assert(comp_channel != NULL);
        
        ibv_cq* cq = ibv_create_cq(device->get_context(), 1, NULL, comp_channel, 0);
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

        result = rdma_create_qp(cm_id, device->get_pd(), &qp_init_attr);
        assert(result == 0);

        rdma_conn_param conn_param;
        memset(&conn_param, 0, sizeof(conn_param));
        conn_param.responder_resources = 16;
        conn_param.initiator_depth = 16;
        conn_param.retry_count = 7;
        conn_param.rnr_retry_count = 7;

        result = rdma_accept(cm_id, &conn_param);
        assert(result == 0);

        return 0;
    }


}